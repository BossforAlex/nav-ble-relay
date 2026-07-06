/// BLE GATT Client 管理
///
/// Flutter 端作为 GATT Client（中心设备）：
///   - 扫描所有 BLE 设备并展示（用户需求：批量开发，不再过滤名字）
///   - 用户在"发现的设备"页面手动选择设备连接
///   - 通过 writeCharacteristic / writeCharacteristicWithoutResponse
///     主动向 5 个特征值推送 JSON 数据
///
/// 用户最新需求（重构）：
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

  /// 当前连接的设备是否是本项目 ESP32（找到 5 个特征值）
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
  bool _disposed = false;

  BluetoothDevice? _device;
  BluetoothCharacteristic? _chrGuide;
  BluetoothCharacteristic? _chrDriveWay;
  BluetoothCharacteristic? _chrTmc;
  BluetoothCharacteristic? _chrState;
  BluetoothCharacteristic? _chrLocation;

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
      await _device?.disconnect();
    } catch (_) {}
    _device = null;
    _chrGuide = null;
    _chrDriveWay = null;
    _chrTmc = null;
    _chrState = null;
    _chrLocation = null;
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
      debugPrint('[BLE] 连接状态变化: $state');
      if (state == BluetoothConnectionState.disconnected && !_disposed) {
        // 用户需求：断开后不自动重连，避免抢占手机的连接槽位
        // 用户需手动到"发现设备"页面重新选择设备连接
        _status = BleStatus.stopped;
        _deviceAddress = '';
        _deviceName = '';
        _chrGuide = null;
        _chrDriveWay = null;
        _chrTmc = null;
        _chrState = null;
        _chrLocation = null;
        _isOurDevice = false;
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

    int found = 0;
    for (var c in targetService.characteristics) {
      final u = c.uuid.str.toLowerCase();
      final p = c.properties;
      debugPrint('[BLE] 特征值 ${c.uuid.str}'
          ' [read=${p.read} write=${p.write} '
          'writeNoResp=${p.writeWithoutResponse} '
          'notify=${p.notify} indicate=${p.indicate}]');
      if (u == BleConstants.charGuideUuid.toLowerCase()) {
        _chrGuide = c;
        found++;
      } else if (u == BleConstants.charDriveWayUuid.toLowerCase()) {
        _chrDriveWay = c;
        found++;
      } else if (u == BleConstants.charTmcUuid.toLowerCase()) {
        _chrTmc = c;
        found++;
      } else if (u == BleConstants.charStateUuid.toLowerCase()) {
        _chrState = c;
        found++;
      } else if (u == BleConstants.charLocationUuid.toLowerCase()) {
        _chrLocation = c;
        found++;
      }
    }
    debugPrint('[BLE] ✓ 映射 $found/5 个特征值');

    _isOurDevice = (found == 5);
    _status = BleStatus.connected;
    _safeNotify();

    if (_isOurDevice) {
      // 连接成功后立刻发送测试数据，验证双向通信正常
      await _sendTestPackets();
    }
  }

  /// 发送测试数据包，验证 BLE 写入通信
  Future<void> _sendTestPackets() async {
    debugPrint('[BLE] >>> 开始发送测试数据包 <<<');
    final ts = DateTime.now().millisecondsSinceEpoch;
    final test = <String, dynamic>{
      'type': 'test',
      'ts': ts,
      'msg': 'BLE communication test from Flutter GATT Client',
      'data': {
        'CUR_ROAD_NAME': 'TestRoad',
        'NEXT_ROAD_NAME': 'NextRoad',
        'SEG_REMAIN_DIS': 1234,
        'CUR_SPEED': 60,
        'LIMITED_SPEED': 80,
      },
    };
    final ok = await _write(_chrState, test, label: 'TEST');
    debugPrint('[BLE] 测试包发送${ok ? "成功" : "失败"}');
  }

  // ── 数据发送 ──────────────────────────────────────────

  Future<void> sendGuideInfo(GuideInfo info, {bool compact = false}) async {
    if (!_isOurDevice) return;
    final now = DateTime.now().millisecondsSinceEpoch;
    final data = <String, dynamic>{
      'type': AmapProtocol.keyGuideInfo,
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
    await _write(_chrGuide, data, label: 'GUIDE');
  }

  Future<void> sendDriveWay(DriveWayInfo info) async {
    if (!_isOurDevice) return;
    final data = <String, dynamic>{
      'type': AmapProtocol.keyDriveWay,
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
    await _write(_chrDriveWay, data, label: 'DRIVE');
  }

  Future<void> sendTmcSegment(TmcSegmentInfo info) async {
    if (!_isOurDevice) return;
    final data = <String, dynamic>{
      'type': AmapProtocol.keyTmcSegment,
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
    await _write(_chrTmc, data, label: 'TMC');
  }

  Future<void> sendMapState(int state, String? crossMap) async {
    if (!_isOurDevice) return;
    final data = <String, dynamic>{
      'type': AmapProtocol.keyMapState,
      'ts': DateTime.now().millisecondsSinceEpoch,
      'data': {
        'EXTRA_STATE': state,
        if (crossMap != null) 'EXTRA_CROSS_MAP': crossMap,
      },
    };
    await _write(_chrState, data, label: 'STATE');
  }

  Future<void> sendLocation(LocationInfo info) async {
    if (!_isOurDevice) return;
    final data = <String, dynamic>{
      'type': AmapProtocol.keyLocation,
      'ts': DateTime.now().millisecondsSinceEpoch,
      'data': {
        'bearing': info.bearing,
        'accuracy': info.accuracy,
        'speed': info.speed,
        'provider': info.provider,
      },
    };
    await _write(_chrLocation, data, label: 'LOC');
  }

  /// 向指定特征值写入 JSON 数据
  /// 返回 true 表示写入成功，false 表示失败
  ///
  /// 写入策略：
  ///   - 优先 writeWithResponse（可靠，有 ACK 确认）
  ///   - 其次 writeWithoutResponse（快但不可靠）
  ///   - 逐特征值判断属性，不再用全局变量
  Future<bool> _write(BluetoothCharacteristic? chr, Map<String, dynamic> packet, {String label = 'DATA'}) async {
    if (!isConnected) {
      debugPrint('[BLE] ✗ $label: 未连接，跳过写入');
      return false;
    }
    if (chr == null) {
      debugPrint('[BLE] ✗ $label: 特征值为空，跳过写入');
      return false;
    }
    final json = jsonEncode(packet);
    final bytes = utf8.encode(json);
    final p = chr.properties;
    final canWriteResp = p.write;
    final canWriteNoResp = p.writeWithoutResponse;
    if (!canWriteResp && !canWriteNoResp) {
      debugPrint('[BLE] ✗ $label: 特征值 ${chr.uuid.str} 不支持 write'
          '（props: read=${p.read} write=${p.write} '
          'writeNoResp=${p.writeWithoutResponse} notify=${p.notify}）');
      return false;
    }
    debugPrint('[BLE] → $label 写 ${bytes.length}字节'
        ' [${canWriteResp ? "writeWithResponse" : "writeNoResp"}]'
        ' uuid=${chr.uuid.str}');
    // 优先 writeWithResponse（可靠）
    if (canWriteResp) {
      try {
        await chr.write(bytes, withoutResponse: false);
        debugPrint('[BLE] ✓ $label 写入成功');
        return true;
      } catch (e) {
        debugPrint('[BLE] ✗ $label writeWithResponse 失败: $e');
        // 回退到 writeWithoutResponse
        if (canWriteNoResp) {
          try {
            await chr.write(bytes, withoutResponse: true);
            debugPrint('[BLE] ✓ $label writeNoResp 回退成功');
            return true;
          } catch (e2) {
            debugPrint('[BLE] ✗ $label writeNoResp 回退失败: $e2');
          }
        }
        return false;
      }
    } else {
      // 只有 writeWithoutResponse
      try {
        await chr.write(bytes, withoutResponse: true);
        debugPrint('[BLE] ✓ $label writeNoResp 写入完成');
        return true;
      } catch (e) {
        debugPrint('[BLE] ✗ $label writeNoResp 失败: $e');
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
    // - 名字包含 ESP32 / espressif
    // - MAC 厂商前缀（Espressif OUI 列表）
    if (name.startsWith('ESP32') ||
        name.toLowerCase().startsWith('espressif')) {
      return DeviceCategory.esp32;
    }
    // Espressif 常见 OUI 前缀（部分列表）
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

    // 3. 蓝牙耳机/音箱（常见命名）
    final nameLower = name.toLowerCase();
    if (nameLower.contains('headphone') ||
        nameLower.contains('headset') ||
        nameLower.contains('earphone') ||
        nameLower.contains('earbuds') ||
        nameLower.contains('airpods') ||
        nameLower.contains('speaker') ||
        nameLower.contains('soundbar') ||
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
    _device?.disconnect();
    super.dispose();
  }
}
