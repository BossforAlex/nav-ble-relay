/// BLE GATT Client 管理
///
/// Flutter 端作为 GATT Client（中心设备）：
///   - 扫描所有 BLE 设备并展示（用户需求：批量开发，不再过滤名字）
///   - 用户在"发现的设备"页面手动选择设备连接
///   - 通过 writeCharacteristic / writeCharacteristicWithoutResponse
///     主动向 1 个特征值（charDataUuid）写入 JSON 数据
///   - 订阅 charPollUuid 接收 ESP32 的 indicate 轮询，收到后立刻写最新数据
///
/// v0.5.7 重构：完全匹配开源参考库 alexanderlavrushko/BLE-HUD-navigation-ESP32
///   - 单 write char（所有 5 类数据通过 JSON type 字段路由）
///   - 单 INDICATE char（ESP32 主动 poll，2 秒一次）+ BLE2902 CCCD 订阅
///   - 收到 indicate → 立刻 flush 一次最新缓存数据
///   - flutter_blue_plus 用 setNotifyValue(true) 同时支持 notify + indicate
///
/// 用户需求（批量开发）：
///   - 移除 isTargetName 过滤，扫描到所有设备都展示
///   - 移除"自动连接第一个匹配设备"逻辑，必须用户手动选择
///   - 通过 MAC/名称推断设备类型展示对应图标
///   - 选中设备连接成功后才能进行数据传输交互
library;

import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../ble/ble_constants.dart';
import '../models/nav_data.dart';
import '../protocol/amap_protocol.dart';

/// BLE 连接状态
enum BleStatus {
  /// 未启动
  stopped,
  /// 蓝牙未开启
  bluetoothOff,
  /// 正在扫描
  scanning,
  /// 正在连接
  connecting,
  /// 已连接 ESP32
  connected,
  /// 启动出错
  error,
}

/// 设备类型（用于图标展示）
enum DeviceCategory {
  /// 本项目 ESP32 HUD（AutoNavDisplay / NavDisplay）
  hud,
  /// 其他 ESP32 / espressif 设备
  esp32,
  /// 蓝牙耳机/音箱
  audio,
  /// 电脑/手机/平板
  computer,
  /// 可穿戴设备
  wearable,
  /// 其他未知设备
  unknown,
}

class BleService extends ChangeNotifier {
  BleService() {
    _adapterSub = FlutterBluePlus.adapterState.listen(_onAdapterChanged);
  }

  /// v0.5.12: Android 前台服务 MethodChannel（防止后台断联）
  static const MethodChannel _serviceChannel =
      MethodChannel('com.navblerelay/service');

  // ── 状态字段 ──────────────────────────────────────────
  BleStatus _status = BleStatus.stopped;
  BleStatus get status => _status;

  String _deviceAddress = '';
  String get deviceAddress => _deviceAddress;

  String _deviceName = '';
  String get deviceName => _deviceName;

  String _lastError = '';
  String get lastError => _lastError;

  /// v0.5.8: 服务发现摘要（UI 可见，诊断特征值为何未发现）
  String _discoverySummary = '';
  String get discoverySummary => _discoverySummary;

  /// 扫描过程中发现的所有设备（用户需求：不再过滤名字）
  final List<ScanResult> _allDevices = [];
  List<ScanResult> get allDevices => List.unmodifiable(_allDevices);

  /// 当前连接的设备是否是本项目 ESP32（找到 2 个特征值）
  bool _isOurDevice = false;
  bool get isOurDevice => _isOurDevice;

  bool get isRunning =>
      _status == BleStatus.scanning ||
      _status == BleStatus.connecting ||
      _status == BleStatus.connected;

  bool get isConnected => _status == BleStatus.connected;

  bool get isScanning => _status == BleStatus.scanning;

  // ── 内部 ──────────────────────────────────────────────
  StreamSubscription<BluetoothAdapterState>? _adapterSub;
  StreamSubscription<List<ScanResult>>? _scanSub;
  StreamSubscription<BluetoothConnectionState>? _connSub;
  StreamSubscription<List<int>>? _pollSub;
  bool _disposed = false;

  BluetoothDevice? _device;
  BluetoothCharacteristic? _chrData;  // 手机写（WRITE | WRITE_NR）
  BluetoothCharacteristic? _chrPoll;  // 手机订阅（NOTIFY）

  // ── UUID 归一化比较 ─────────────────────────────────
  /// 将 UUID 字符串归一化为短格式用于比较。
  /// 蓝牙 SIG 16-bit UUID（0000XXXX-0000-1000-8000-00805F9B34FB）
  /// 归一化为 4 位短格式 "XXXX"，其他 UUID 保持完整格式。
  static String _shortUuid(String uuid) {
    final n = uuid.replaceAll('-', '').toLowerCase();
    // 16-bit Bluetooth SIG UUID
    if (n.length == 32 &&
        n.startsWith('0000') &&
        n.endsWith('00001000800000805f9b34fb')) {
      return n.substring(4, 8);
    }
    // 已为短格式（4 或 8 字符）
    if (n.length <= 8) return n;
    return n;
  }

  /// 比较 Guid 对象与字符串 UUID（自动处理 16-bit ↔ 128-bit 格式差异）
  static bool _uuidMatch(Guid a, String b) =>
      _shortUuid(a.str) == _shortUuid(b);

  // ── 发送统计（用户需求：发送统计卡片） ────────────────
  int _txOk = 0;
  int _txFail = 0;
  int _throttled = 0;
  int get txOkCount => _txOk;
  int get txFailCount => _txFail;
  int get throttledCount => _throttled;
  void resetTxStats() { _txOk = 0; _txFail = 0; _throttled = 0; _safeNotify(); }
  void resetThrottledCount() { _throttled = 0; _safeNotify(); }

  // ── 待发送的导航数据缓存（收到 poll 后立刻发这些） ────
  GuideInfo? _pendingGuide;
  DriveWayInfo? _pendingDrive;
  TmcSegmentInfo? _pendingTmc;
  LocationInfo? _pendingLocation;
  int? _pendingState;
  String? _pendingCrossMap;
  DateTime _lastFlushMs = DateTime.fromMillisecondsSinceEpoch(0);

  // ── v0.6.6 关键修复:全局 BLE 写锁 ──
  // 之前的问题:_onPollReceived 用 Future.wait() 并行发送 5 个 pending 数据
  // 当其中 2 个 (guide + drive) 都被分片传输时,它们各自的 chunk 会交错到达
  // ESP32 → 分片校验失败 → 整个 JSON 被丢弃 → 屏幕永远停在 idle
  // 修复:用串行写链确保任意时刻只有 1 个 BLE write 在飞
  Future<void> _writeChain = Future.value();

  /// 监听适配器状态变化
  void _onAdapterChanged(BluetoothAdapterState state) {
    if (state == BluetoothAdapterState.on) {
      if (_status == BleStatus.bluetoothOff) {
        _status = BleStatus.stopped;
      }
    } else if (state == BluetoothAdapterState.off) {
      _status = BleStatus.bluetoothOff;
      _deviceAddress = '';
      _deviceName = '';
      _allDevices.clear();
    }
    _safeNotify();
  }

  /// 启动扫描（用户需求：扫描所有设备，不再过滤）
  Future<void> startScan() async {
    if (isScanning) return;

    final adapterState = FlutterBluePlus.adapterStateNow;
    if (adapterState != BluetoothAdapterState.on) {
      _status = BleStatus.bluetoothOff;
      _lastError = '蓝牙未开启';
      _safeNotify();
      return;
    }

    _allDevices.clear();
    _status = BleStatus.scanning;
    _lastError = '';
    _safeNotify();

    try {
      _scanSub?.cancel();
      _scanSub = FlutterBluePlus.scanResults.listen(_onScanResult);
      await FlutterBluePlus.startScan(
        timeout: const Duration(seconds: 15),
        androidScanMode: AndroidScanMode.lowLatency,
      );
    } catch (e) {
      _status = BleStatus.error;
      _lastError = '扫描启动失败: $e';
      _safeNotify();
    }
  }

  /// 启动（兼容旧 API：startScan 等价）
  Future<void> start() => startScan();

  /// 手动重扫
  Future<void> rescan() async {
    if (isConnected) {
      debugPrint('[BLE] 已连接，忽略重扫请求');
      return;
    }
    if (isScanning) {
      debugPrint('[BLE] 已在扫描中');
      return;
    }
    debugPrint('[BLE] 手动重扫...');
    await startScan();
  }

  /// 停止扫描
  Future<void> stopScan() async {
    try {
      _scanSub?.cancel();
      _scanSub = null;
      await FlutterBluePlus.stopScan();
    } catch (_) {}
    if (_status == BleStatus.scanning) {
      _status = BleStatus.stopped;
    }
    _safeNotify();
  }

  /// 手动连接指定的扫描结果（用户在设备列表中点击连接）
  ///
  /// v0.5.12: 添加 error 133 (GATT_ERROR) 重试机制。
  /// Android 后台杀进程后 BLE 栈进入脏状态，首次连接返回 133，
  /// 等待 500ms 后重试通常能恢复。
  Future<void> connectTo(ScanResult r) async {
    if (isConnected || _status == BleStatus.connecting) {
      debugPrint('[BLE] 已有连接/正在连接，忽略 connectTo');
      return;
    }

    // v0.5.12: 先清理旧连接状态，防止 BLE 栈脏状态
    await _cleanupBleStack();

    try {
      await FlutterBluePlus.stopScan();
    } catch (_) {}
    _scanSub?.cancel();
    _scanSub = null;

    _device = r.device;
    _deviceAddress = r.device.remoteId.str;
    _deviceName = r.advertisementData.advName.isNotEmpty
        ? r.advertisementData.advName
        : 'Unknown';
    _isOurDevice = false;

    _status = BleStatus.connecting;
    _lastError = '';
    _safeNotify();

    // v0.5.12: 最多重试 2 次（error 133 常见于后台恢复后首次连接）
    const maxRetries = 2;
    for (int attempt = 0; attempt <= maxRetries; attempt++) {
      try {
        if (attempt > 0) {
          debugPrint('[BLE] 重试连接 (第 $attempt 次)...');
          await Future.delayed(const Duration(milliseconds: 500));
          // 重试前重新获取设备引用（解决 GATT 缓存问题）
          _device = r.device;
        }
        await _connectAndDiscover();
        return; // 成功
      } catch (e) {
        final errStr = e.toString();
        final isGattError = errStr.contains('133') || errStr.contains('GATT_ERROR');
        if (isGattError && attempt < maxRetries) {
          debugPrint('[BLE] GATT_ERROR (133)，将重试...');
          _lastError = '连接失败(133)，重试中...';
          _safeNotify();
          continue;
        }
        _status = BleStatus.error;
        _lastError = '连接失败: $e';
        _device = null;
        _safeNotify();
        return;
      }
    }
  }

  /// v0.5.12: 清理 BLE 栈状态，防止 error 133
  Future<void> _cleanupBleStack() async {
    try {
      if (_device != null) {
        await _device?.disconnect();
      }
    } catch (_) {}
    _connSub?.cancel();
    _connSub = null;
    _pollSub?.cancel();
    _pollSub = null;
    _device = null;
    _chrData = null;
    _chrPoll = null;
    _isOurDevice = false;
    // 停止前台服务
    await _stopFgService();
  }

  /// 断开连接
  Future<void> disconnect() async {
    // v0.5.12: 停止前台服务
    await _stopFgService();
    try {
      _connSub?.cancel();
      _connSub = null;
      _pollSub?.cancel();
      _pollSub = null;
      await _device?.disconnect();
    } catch (_) {}
    _device = null;
    _chrData = null;
    _chrPoll = null;
    _isOurDevice = false;
    _discoverySummary = '';
    _status = BleStatus.stopped;
    _deviceAddress = '';
    _deviceName = '';
    _safeNotify();
  }

  /// 停止 GATT Client（兼容旧 API）
  Future<void> stop() => disconnect();

  /// 处理扫描结果（用户需求：展示所有设备，不再过滤）
  void _onScanResult(List<ScanResult> results) {
    if (_status != BleStatus.scanning) return;

    // 收集所有设备
    bool changed = false;
    for (final r in results) {
      final addr = r.device.remoteId.str;
      final idx = _allDevices
          .indexWhere((e) => e.device.remoteId.str == addr);
      if (idx < 0) {
        _allDevices.add(r);
        changed = true;
      } else {
        // 更新 RSSI（设备信号会变化）
        final old = _allDevices[idx];
        if (old.rssi != r.rssi) {
          _allDevices[idx] = r;
          changed = true;
        }
      }
    }
    if (changed) {
      _safeNotify();
    }
  }

  /// 连接并发现服务/特征值
  Future<void> _connectAndDiscover() async {
    final device = _device;
    if (device == null) return;

    debugPrint('[BLE] >>> 开始连接 ${device.remoteId.str} <<<');

    _connSub?.cancel();
    _connSub = device.connectionState.listen((state) {
      if (state == BluetoothConnectionState.disconnected && !_disposed) {
        debugPrint('[BLE] 连接已断开');
        // 用户需求：断开后不自动重连，避免抢占手机的连接槽位
        // 用户需手动到"发现设备"页面重新选择设备连接
        _pollSub?.cancel();
        _pollSub = null;
        _chrData = null;
        _chrPoll = null;
        _isOurDevice = false;
        _discoverySummary = '';
        _status = BleStatus.stopped;
        _deviceAddress = '';
        _deviceName = '';
        // v0.5.12: 停止前台服务
        _stopFgService();
        _safeNotify();
      }
    });

    await device.connect(timeout: const Duration(seconds: 10));
    debugPrint('[BLE] ✓ 物理层已连接 ${device.remoteId.str}');

    // 请求更大 MTU
    try {
      final mtu = await device.requestMtu(512);
      debugPrint('[BLE] ✓ MTU 协商: $mtu 字节');
    } catch (e) {
      debugPrint('[BLE] ! MTU 协商失败: $e（继续用默认）');
    }

    // 发现服务
    List<BluetoothService> services = await device.discoverServices();
    debugPrint('[BLE] 发现 ${services.length} 个服务');

    // v0.5.9 修复：用 _uuidMatch 归一化比较（处理 16-bit ↔ 128-bit 格式差异）
    // flutter_blue_plus Guid.str 对 16-bit UUID 返回 "ffe0"（短格式），
    // 而我们常量是完整 128-bit 格式，Guid.== 底层比较 str 值，必不相等
    BluetoothService? targetService;
    for (var s in services) {
      if (_uuidMatch(s.uuid, BleConstants.serviceUuid)) {
        targetService = s;
        break;
      }
    }

    if (targetService == null) {
      // 非本项目设备：连接成功但不进行数据传输
      _discoverySummary = '未找到目标服务 ${BleConstants.serviceUuid.toUpperCase()}\n'
          '已发现 ${services.length} 个服务: ${services.map((s) => s.uuid.str).join(", ")}';
      _lastError = '服务 ${BleConstants.serviceUuid.toUpperCase()} 未发现';
      debugPrint('[BLE] ⚠ $_discoverySummary');
      _status = BleStatus.connected;
      _isOurDevice = false;
      _safeNotify();
      return;
    }

    debugPrint('[BLE] ✓ 找到目标服务: ${targetService.uuid.str}');

    // 找到 2 个特征值：CHAR_DATA（写）+ CHAR_POLL（订阅）
    int found = 0;
    final foundChars = <String>[];
    for (var c in targetService.characteristics) {
      final p = c.properties;
      foundChars.add('${c.uuid.str} [W=${p.write} WNR=${p.writeWithoutResponse} I=${p.indicate}]');
      debugPrint('[BLE] 特征值 ${c.uuid.str}'
          ' [read=${p.read} write=${p.write} '
          'writeNoResp=${p.writeWithoutResponse} '
          'notify=${p.notify} indicate=${p.indicate}]');
      if (_uuidMatch(c.uuid, BleConstants.charDataUuid)) {
        _chrData = c;
        found++;
      } else if (_uuidMatch(c.uuid, BleConstants.charPollUuid)) {
        _chrPoll = c;
        found++;
      }
    }

    _isOurDevice = (found == 2 && _chrData != null && _chrPoll != null);
    _discoverySummary = '目标服务: ${targetService.uuid.str}\n'
        '特征值已映射 $found/2:\n${foundChars.join("\n")}'
        '${_isOurDevice ? "" : "\n⚠ 缺少 CHAR_DATA(${BleConstants.charDataUuid.toUpperCase()}) 或 CHAR_POLL(${BleConstants.charPollUuid.toUpperCase()})"}';
    debugPrint('[BLE] $_discoverySummary');

    if (!_isOurDevice) {
      _lastError = '特征值不全: 找到 $found/2 (DATA=${_chrData != null} POLL=${_chrPoll != null})';
    } else {
      _lastError = '';
    }

    _status = BleStatus.connected;
    // v0.5.12: 启动 Android 前台服务，防止后台断联
    _startFgService();
    _safeNotify();

    if (_isOurDevice && _chrPoll != null) {
      // v0.9.9: 改用 onValueReceived 替代 lastValueStream
      // 原因：lastValueStream 是 ValueStream，当值未变化时不发射事件，
      // 导致 ESP32 发送的重复 poll 值被丢弃，Flutter 端永远收不到后续 poll
      try {
        _pollSub?.cancel();
        _pollSub = _chrPoll!.onValueReceived.listen(_onPollReceived);
        await _chrPoll!.setNotifyValue(true);
        debugPrint('[BLE] ✓ 已订阅 CHAR_POLL notify (onValueReceived)');
      } catch (e) {
        debugPrint('[BLE] ✗ 订阅 CHAR_POLL 失败: $e');
      }

      // 连接成功后立刻发送一次测试数据，验证双向通信
      await sendTestPacket();
    }
  }

  /// 收到 ESP32 的 indicate：立刻把缓存的最新数据 flush 一次
  // v0.9.8: 添加并发保护 —— 防止两次 poll 同时触发 flush 导致 BLE 写交错
  bool _flushing = false;
  void _onPollReceived(List<int> value) async {
    // ESP32 每 500ms 无活动就发空 indicate
    // 收到后立刻把所有 pending 数据写一次
    if (!_isOurDevice || _chrData == null) return;

    // 节流：避免太频繁（虽然 ESP32 端 500ms 一次，但保险起见 100ms 内只 flush 一次）
    final now = DateTime.now();
    if (now.difference(_lastFlushMs).inMilliseconds < 100) {
      _throttled++;
      _safeNotify();
      return;
    }
    _lastFlushMs = now;

    // v0.9.8: 并发保护 —— 防止两次 poll 同时触发 flush
    if (_flushing) {
      debugPrint('[BLE] 上一轮 flush 未完成，跳过本次 poll');
      _throttled++;
      _safeNotify();
      return;
    }
    _flushing = true;
    try {
      // v0.6.6 关键修复:绝对禁止并行写!
      if (_pendingGuide != null) {
        await sendGuideInfo(_pendingGuide!, compact: false);
      }
      if (_pendingDrive != null) {
        await sendDriveWay(_pendingDrive!);
      }
      if (_pendingTmc != null) {
        await sendTmcSegment(_pendingTmc!);
      }
      if (_pendingLocation != null) {
        await sendLocation(_pendingLocation!);
      }
      if (_pendingState != null) {
        await sendMapState(_pendingState!, _pendingCrossMap);
      }
    } finally {
      _flushing = false;
    }
  }

  /// v0.5.8: 发送测试数据包，验证 BLE 写入通道（公开方法，供 UI 按钮调用）
  Future<bool> sendTestPacket() async {
    if (!_isOurDevice) {
      _lastError = 'TEST: 非本项目设备（_isOurDevice=false）\n$_discoverySummary';
      debugPrint('[BLE] ✗ $_lastError');
      return false;
    }
    debugPrint('[BLE] >>> 开始发送测试数据包 <<<');
    final ts = DateTime.now().millisecondsSinceEpoch;
    final test = <String, dynamic>{
      'type': 'state',
      'ts': ts,
      'data': {
        'EXTRA_STATE': 1,  // 1=导航中 (Amap 协议: 0=idle, 1=导航中, 2=已到达, 3=暂停)
        'EXTRA_CROSS_MAP': 'BLE TEST OK @ $ts',
      },
    };
    final ok = await _write(test, label: 'TEST');
    debugPrint('[BLE] 测试包发送${ok ? "成功" : "失败"}');
    return ok;
  }

  // ── 数据发送（全部通过单一 chrData） ──────────────────

  Future<void> sendGuideInfo(GuideInfo info, {bool compact = false}) async {
    if (!_isOurDevice) return;
    _pendingGuide = info;
    final now = DateTime.now().millisecondsSinceEpoch;
    final packet = <String, dynamic>{
      'type': 'guide',
      'ts': now,
      'data': {
        'ICON': info.icon,
        'CUR_ROAD_NAME': info.curRoadName,
        'NEXT_ROAD_NAME': info.nextRoadName,
        'SEG_REMAIN_DIS': info.segRemainDis,
        'turn_label': AmapProtocol.iconShortLabel(info.icon),
        'distance_text': _formatDistance(info.segRemainDis),
        'intersection': _formatIntersection(info.curRoadName, info.nextRoadName),
        if (!compact) ...{
          'ROUTE_REMAIN_DIS': info.routeRemainDis,
          'ROUTE_REMAIN_TIME': info.routeRemainTime,
          'CUR_SPEED': info.curSpeed,
          'LIMITED_SPEED': info.limitedSpeed,
          'ROAD_TYPE': info.roadType,
          'CAMERA_DIST': info.cameraDist,
          'CAMERA_TYPE': info.cameraType,
          'CAMERA_SPEED': info.cameraSpeed,
        },
      },
    };
    await _write(packet, label: 'GUIDE');
  }

  Future<void> sendDriveWay(DriveWayInfo info) async {
    if (!_isOurDevice) return;
    _pendingDrive = info;
    final packet = <String, dynamic>{
      'type': 'drive',
      'ts': DateTime.now().millisecondsSinceEpoch,
      'data': {
        'drive_way_enabled': info.enabled,
        'drive_way_size': info.size,
        'drive_way_info': info.lanes
            .map((l) => {
                  'drive_way_number': l.number,
                  'drive_way_lane_Back_icon': l.backIcon,
                })
            .toList(),
      },
    };
    await _write(packet, label: 'DRIVE');
  }

  Future<void> sendTmcSegment(TmcSegmentInfo info) async {
    if (!_isOurDevice) return;
    _pendingTmc = info;
    final packet = <String, dynamic>{
      'type': 'tmc',
      'ts': DateTime.now().millisecondsSinceEpoch,
      'data': {
        'total_distance': info.totalDistance,
        'residual_distance': info.residualDistance,
        'tmc_info': info.segments
            .map((s) => {
                  'tmc_segment_number': s.number,
                  'tmc_status': s.status,
                  'tmc_segment_distance': s.distance,
                })
            .toList(),
      },
    };
    await _write(packet, label: 'TMC');
  }

  Future<void> sendMapState(int state, String? crossMap) async {
    if (!_isOurDevice) return;
    _pendingState = state;
    _pendingCrossMap = crossMap;
    final packet = <String, dynamic>{
      'type': 'state',
      'ts': DateTime.now().millisecondsSinceEpoch,
      'data': {
        'EXTRA_STATE': state,
        if (crossMap != null) 'EXTRA_CROSS_MAP': crossMap,
      },
    };
    await _write(packet, label: 'STATE');
  }

  Future<void> sendLocation(LocationInfo info) async {
    if (!_isOurDevice) return;
    _pendingLocation = info;
    final packet = <String, dynamic>{
      'type': 'location',
      'ts': DateTime.now().millisecondsSinceEpoch,
      'data': {
        'bearing': info.bearing,
        'accuracy': info.accuracy,
        'speed': info.speed,
        'provider': info.provider,
      },
    };
    await _write(packet, label: 'LOC');
  }

  /// 向 CHAR_DATA 写入 JSON 数据
  ///
  /// v0.6.4: 自动分片传输。当 JSON 编码后超过 MTU 安全阈值 (400 bytes)，
  /// 自动拆分为多个 chunk，每个 chunk 带 [0xAA, idx, total, ...data] 二进制头。
  /// ESP32 端自动重组后解析，对上层透明。
  ///
  /// 写入策略：
  ///   - 优先 writeWithResponse（可靠，有 ACK 确认）
  ///   - 其次 writeWithoutResponse（快但不可靠）
  Future<bool> _write(Map<String, dynamic> packet, {String label = 'DATA'}) async {
    if (!isConnected) {
      _lastError = '$label: BLE 未连接';
      debugPrint('[BLE] ✗ $_lastError');
      _txFail++;
      _safeNotify();
      return false;
    }
    if (_chrData == null) {
      _lastError = '$label: chrData 为空（特征值未发现）';
      debugPrint('[BLE] ✗ $_lastError');
      _txFail++;
      _safeNotify();
      return false;
    }
    final json = jsonEncode(packet);
    final bytes = utf8.encode(json);

    // v0.6.4: 数据超过 MTU 安全阈值 → 自动分片
    const int maxPayload = 400;
    if (bytes.length <= maxPayload) {
      final ok = await _writeRaw(bytes, label);
      if (ok) { _txOk++; _safeNotify(); }
      return ok;
    }

    // 分片传输
    final total = (bytes.length + maxPayload - 1) ~/ maxPayload;
    debugPrint('[BLE] $label: ${bytes.length}B 超过 MTU, 分 $total 片发送');
    bool allOk = true;
    for (int i = 0; i < total; i++) {
      final start = i * maxPayload;
      final end = (start + maxPayload).clamp(0, bytes.length);
      final chunk = <int>[
        0xAA, // magic: chunked message
        i,    // chunk index
        total, // total chunks
        ...bytes.sublist(start, end),
      ];
      final ok = await _writeRaw(chunk, '$label#${i + 1}/$total');
      if (!ok) {
        debugPrint('[BLE] ✗ $label 分片 ${i + 1}/$total 失败, 中止');
        allOk = false;
        break;
      }
    }
    if (allOk) {
      _txOk++;
      _safeNotify();
    } else {
      _txFail++;
      _safeNotify();
    }
    return allOk;
  }

  /// 向 CHAR_DATA 写入原始字节（不编码 JSON）
  ///
  /// v0.6.6: 整段代码包在 _writeChain 串行链中,保证任意时刻只有 1 个 BLE write 在飞。
  /// 防止并发写入导致 ESP32 端分片重组错乱。
  Future<bool> _writeRaw(List<int> bytes, String label) async {
    final prev = _writeChain;
    final completer = Completer<void>();
    _writeChain = completer.future;
    try {
      await prev;
      return await _writeRawImpl(bytes, label);
    } finally {
      completer.complete();
    }
  }

  /// _writeRaw 的实际实现（不带锁）
  Future<bool> _writeRawImpl(List<int> bytes, String label) async {
    final chr = _chrData!;
    final p = chr.properties;
    final canWriteResp = p.write;
    final canWriteNoResp = p.writeWithoutResponse;

    if (!canWriteResp && !canWriteNoResp) {
      _lastError = '$label: chrData 不支持 write';
      _txFail++;
      _safeNotify();
      return false;
    }

    if (canWriteResp) {
      try {
        await chr.write(bytes, withoutResponse: false);
        _lastError = '';
        return true;
      } catch (e) {
        _lastError = '$label writeWithResponse: $e';
        debugPrint('[BLE] ✗ $_lastError');
        if (canWriteNoResp) {
          try {
            await chr.write(bytes, withoutResponse: true);
            _lastError = '';
            return true;
          } catch (e2) {
            _lastError = '$label writeNoResp 回退: $e2';
            debugPrint('[BLE] ✗ $_lastError');
          }
        }
        _txFail++;
        _safeNotify();
        return false;
      }
    } else {
      try {
        await chr.write(bytes, withoutResponse: true);
        _lastError = '';
        return true;
      } catch (e) {
        _lastError = '$label writeNoResp: $e';
        debugPrint('[BLE] ✗ $_lastError');
        _txFail++;
        _safeNotify();
        return false;
      }
    }
  }

  // ── 设备分类（用于图标展示）──────────────────────────

  /// 根据 MAC 地址 + 设备名推断设备类型
  /// 用户需求：批量开发，扫描到哪些设备展示哪些设备，通过 MAC 区别类型
  static DeviceCategory classifyDevice(ScanResult r) {
    final name = r.advertisementData.advName;
    final mac = r.device.remoteId.str.toLowerCase();

    // 1. 本项目 ESP32 HUD（AutoNavDisplay / NavDisplay）
    if (name.startsWith('AutoNavDisplay') ||
        name.startsWith('NavDisplay')) {
      return DeviceCategory.hud;
    }

    // 2. 其他 ESP32 / espressif
    if (name.startsWith('ESP32') ||
        name.toLowerCase().startsWith('espressif')) {
      return DeviceCategory.esp32;
    }
    const espOuiPrefixes = [
      '24:0a:c4', '24:6f:28', '30:ae:a4', '3c:71:37', '40:f5:20',
      '4c:11:bf', '54:5a:a6', '5c:cf:7f', '60:01:94', '68:c6:3f',
      '84:0d:8e', '84:cc:a8', '8c:4b:14', '90:97:d5', '94:b5:34',
      'a4:cf:12', 'ac:0b:fb', 'b0:a7:32', 'bc:dd:c2', 'c4:4f:33',
      'd4:f9:8d', 'd8:a0:1d', 'e0:98:06', 'ec:fa:bc', 'f0:08:d1',
      'e8:db:84',
    ];
    final macNorm = mac.replaceAll(':', '');
    for (final oui in espOuiPrefixes) {
      final ouiNorm = oui.replaceAll(':', '');
      if (macNorm.startsWith(ouiNorm)) {
        return DeviceCategory.esp32;
      }
    }

    // 3. 蓝牙耳机/音箱
    final nameLower = name.toLowerCase();
    if (nameLower.contains('headphone') ||
        nameLower.contains('headset') ||
        nameLower.contains('earphone') ||
        nameLower.contains('earbuds') ||
        nameLower.contains('airpods') ||
        nameLower.contains('speaker') ||
        nameLower.contains('soundbar')) {
      return DeviceCategory.audio;
    }

    // 4. 电脑/手机/平板
    if (nameLower.contains('macbook') ||
        nameLower.contains('iphone') ||
        nameLower.contains('ipad') ||
        nameLower.contains('android') ||
        nameLower.contains('samsung') ||
        nameLower.contains('pixel') ||
        nameLower.contains('huawei') ||
        nameLower.contains('xiaomi') ||
        nameLower.contains('oppo') ||
        nameLower.contains('vivo') ||
        nameLower.contains('laptop') ||
        nameLower.contains('desktop')) {
      return DeviceCategory.computer;
    }

    // 5. 可穿戴设备
    if (nameLower.contains('watch') ||
        nameLower.contains('band') ||
        nameLower.contains('fit') ||
        nameLower.contains('tracker')) {
      return DeviceCategory.wearable;
    }

    return DeviceCategory.unknown;
  }

  /// 设备分类的可读名称
  static String categoryLabel(DeviceCategory cat) {
    switch (cat) {
      case DeviceCategory.hud:
        return 'AutoNav HUD';
      case DeviceCategory.esp32:
        return 'ESP32';
      case DeviceCategory.audio:
        return '音频设备';
      case DeviceCategory.computer:
        return '电脑/手机';
      case DeviceCategory.wearable:
        return '可穿戴';
      case DeviceCategory.unknown:
        return '其他设备';
    }
  }

  String _formatDistance(int meters) {
    if (meters <= 0) return '';
    if (meters >= 1000) return '${(meters / 1000).toStringAsFixed(1)} km';
    return '$meters m';
  }

  String _formatIntersection(String cur, String next) {
    final c = cur.isEmpty ? '未知道路' : cur;
    final n = next.isEmpty ? '未知道路' : next;
    return '$c → $n';
  }

  void _safeNotify() {
    if (_disposed) return;
    notifyListeners();
  }

  /// v0.5.12: 启动 Android 前台服务（防止后台断联）
  Future<void> _startFgService() async {
    try {
      await _serviceChannel.invokeMethod('start');
      debugPrint('[BLE] 前台服务已启动');
    } catch (e) {
      debugPrint('[BLE] 前台服务启动失败（非 Android 或权限不足）: $e');
    }
  }

  /// v0.5.12: 停止 Android 前台服务
  Future<void> _stopFgService() async {
    try {
      await _serviceChannel.invokeMethod('stop');
      debugPrint('[BLE] 前台服务已停止');
    } catch (e) {
      // 忽略（非 Android 平台）
    }
  }

  @override
  void dispose() {
    _disposed = true;
    _adapterSub?.cancel();
    _scanSub?.cancel();
    _connSub?.cancel();
    _pollSub?.cancel();
    _stopFgService();
    _device?.disconnect();
    super.dispose();
  }
}
