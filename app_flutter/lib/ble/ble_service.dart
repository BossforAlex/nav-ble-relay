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

  // ── 状态字段 ──────────────────────────────────────────
  BleStatus _status = BleStatus.stopped;
  BleStatus get status => _status;

  String _deviceAddress = '';
  String get deviceAddress => _deviceAddress;

  String _deviceName = '';
  String get deviceName => _deviceName;

  String _lastError = '';
  String get lastError => _lastError;

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
  Future<void> connectTo(ScanResult r) async {
    if (isConnected || _status == BleStatus.connecting) {
      debugPrint('[BLE] 已有连接/正在连接，忽略 connectTo');
      return;
    }
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

    try {
      await _connectAndDiscover();
    } catch (e) {
      _status = BleStatus.error;
      _lastError = '连接失败: $e';
      _device = null;
      _safeNotify();
    }
  }

  /// 断开连接
  Future<void> disconnect() async {
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
        _status = BleStatus.stopped;
        _deviceAddress = '';
        _deviceName = '';
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

    BluetoothService? targetService;
    for (var s in services) {
      if (s.uuid.str.toLowerCase() == BleConstants.serviceUuid.toLowerCase()) {
        targetService = s;
        break;
      }
    }

    if (targetService == null) {
      // 非本项目设备：连接成功但不进行数据传输
      debugPrint('[BLE] ⚠ 未找到目标服务 ${BleConstants.serviceUuid}'
          '（非 AutoNavDisplay 设备）');
      _status = BleStatus.connected;
      _isOurDevice = false;
      _safeNotify();
      return;
    }

    debugPrint('[BLE] ✓ 找到目标服务: ${targetService.uuid.str}');

    // 找到 2 个特征值：CHAR_DATA（写）+ CHAR_POLL（订阅）
    int found = 0;
    for (var c in targetService.characteristics) {
      final u = c.uuid.str.toLowerCase();
      final p = c.properties;
      debugPrint('[BLE] 特征值 ${c.uuid.str}'
          ' [read=${p.read} write=${p.write} '
          'writeNoResp=${p.writeWithoutResponse} '
          'notify=${p.notify} indicate=${p.indicate}]');
      if (u == BleConstants.charDataUuid.toLowerCase()) {
        _chrData = c;
        found++;
      } else if (u == BleConstants.charPollUuid.toLowerCase()) {
        _chrPoll = c;
        found++;
      }
    }
    debugPrint('[BLE] ✓ 映射 $found/2 个特征值 (DATA + POLL)');

    _isOurDevice = (found == 2 && _chrData != null && _chrPoll != null);
    _status = BleStatus.connected;
    _safeNotify();

    if (_isOurDevice && _chrPoll != null) {
      // 订阅 CHAR_POLL 的 indicate —— 收到 ESP32 的 indicate 后立刻 flush 缓存
      // 注意：flutter_blue_plus 的 setNotifyValue(true) 同时支持 notify + indicate
      try {
        _pollSub?.cancel();
        _pollSub = _chrPoll!.lastValueStream.listen(_onPollReceived);
        await _chrPoll!.setNotifyValue(true);
        debugPrint('[BLE] ✓ 已订阅 CHAR_POLL indicate');
      } catch (e) {
        debugPrint('[BLE] ✗ 订阅 CHAR_POLL 失败: $e');
      }

      // 连接成功后立刻发送一次测试数据，验证双向通信
      await sendTestPacket();
    }
  }

  /// 收到 ESP32 的 indicate：立刻把缓存的最新数据 flush 一次
  void _onPollReceived(List<int> value) {
    // ESP32 每 2 秒无活动就发空 indicate
    // 收到后立刻把所有 pending 数据写一次
    if (!_isOurDevice || _chrData == null) return;

    // 节流：避免太频繁（虽然 ESP32 端 2 秒一次，但保险起见 100ms 内只 flush 一次）
    final now = DateTime.now();
    if (now.difference(_lastFlushMs).inMilliseconds < 100) {
      _throttled++;
      _safeNotify();
      return;
    }
    _lastFlushMs = now;

    // 并行写入所有 pending 数据
    final futures = <Future<void>>[];
    if (_pendingGuide != null) {
      futures.add(sendGuideInfo(_pendingGuide!, compact: false));
    }
    if (_pendingDrive != null) {
      futures.add(sendDriveWay(_pendingDrive!));
    }
    if (_pendingTmc != null) {
      futures.add(sendTmcSegment(_pendingTmc!));
    }
    if (_pendingLocation != null) {
      futures.add(sendLocation(_pendingLocation!));
    }
    if (_pendingState != null) {
      futures.add(sendMapState(_pendingState!, _pendingCrossMap));
    }
    if (futures.isNotEmpty) {
      Future.wait(futures);
    }
  }

  /// v0.5.8: 发送测试数据包，验证 BLE 写入通道（公开方法，供 UI 按钮调用）
  Future<bool> sendTestPacket() async {
    debugPrint('[BLE] >>> 开始发送测试数据包 <<<');
    final ts = DateTime.now().millisecondsSinceEpoch;
    final test = <String, dynamic>{
      'type': 'state',
      'ts': ts,
      'data': {
        'EXTRA_STATE': 0,  // 0=导航中
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
  /// 写入策略：
  ///   - 优先 writeWithResponse（可靠，有 ACK 确认）
  ///   - 其次 writeWithoutResponse（快但不可靠）
  ///   - 检查 chrData 的属性
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
    final chr = _chrData!;
    final json = jsonEncode(packet);
    final bytes = utf8.encode(json);
    final p = chr.properties;
    final canWriteResp = p.write;
    final canWriteNoResp = p.writeWithoutResponse;
    if (!canWriteResp && !canWriteNoResp) {
      _lastError = '$label: chrData 不支持 write'
          '（read=${p.read} write=${p.write} writeNoResp=${p.writeWithoutResponse}）';
      debugPrint('[BLE] ✗ $_lastError');
      _txFail++;
      _safeNotify();
      return false;
    }
    // 优先 writeWithResponse（可靠）
    if (canWriteResp) {
      try {
        await chr.write(bytes, withoutResponse: false);
        _lastError = '';
        _txOk++;
        _safeNotify();
        return true;
      } catch (e) {
        _lastError = '$label writeWithResponse: $e';
        debugPrint('[BLE] ✗ $_lastError');
        // 回退到 writeWithoutResponse
        if (canWriteNoResp) {
          try {
            await chr.write(bytes, withoutResponse: true);
            _lastError = '';
            _txOk++;
            _safeNotify();
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
      // 只有 writeWithoutResponse
      try {
        await chr.write(bytes, withoutResponse: true);
        _lastError = '';
        _txOk++;
        _safeNotify();
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

  @override
  void dispose() {
    _disposed = true;
    _adapterSub?.cancel();
    _scanSub?.cancel();
    _connSub?.cancel();
    _pollSub?.cancel();
    _device?.disconnect();
    super.dispose();
  }
}
