/// BLE GATT Client 管理
///
/// Flutter 端作为 GATT Client（中心设备）：
///   - 主动扫描名为 "AutoNavDisplay" 的 ESP32 设备
///   - 扫描到第一个名字匹配的 ESP32 后自动连接
///   - 通过 writeCharacteristic / writeCharacteristicWithoutResponse
///     主动向 5 个特征值推送 JSON 数据
///
/// ESP32（外设）连接后被动接收数据即可。
///
/// 用户最新需求：
///   - 去除白名单限制（用户反馈：白名单无法判断是否生效，直接去掉）
///   - 软件没添加"选择设备"功能时，直接选中第一个匹配的设备接受数据
///   - 后续如需可选设备列表，UI 层扩展即可
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

  /// 扫描过程中发现的目标设备列表（名字匹配 AutoNavDisplay 等）
  final List<ScanResult> _discoveredTargets = [];
  List<ScanResult> get discoveredTargets =>
      List.unmodifiable(_discoveredTargets);

  /// 是否已经尝试连接过发现的设备（防止重复连接）
  bool _hasConnectAttempted = false;

  bool get isRunning =>
      _status == BleStatus.scanning ||
      _status == BleStatus.connecting ||
      _status == BleStatus.connected;

  bool get isConnected => _status == BleStatus.connected;

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

  // 当前是否允许 writeWithoutResponse（部分外设不支持）
  // 注：已改为逐特征值判断，不再使用全局变量
  bool _supportWriteNoResp = true;

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
      _discoveredTargets.clear();
    }
    _safeNotify();
  }

  /// 启动 GATT Client：扫描 + 连接 + 发现服务
  Future<void> start() async {
    if (isRunning) return;

    final adapterState = FlutterBluePlus.adapterStateNow;
    if (adapterState != BluetoothAdapterState.on) {
      _status = BleStatus.bluetoothOff;
      _lastError = '蓝牙未开启';
      _safeNotify();
      return;
    }

    _discoveredTargets.clear();
    _hasConnectAttempted = false;
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

  /// 手动重扫（不依赖白名单，每次都重新扫）
  Future<void> rescan() async {
    if (isConnected) {
      debugPrint('[BLE] 已连接，无需重扫');
      return;
    }
    if (_status == BleStatus.scanning) {
      debugPrint('[BLE] 已在扫描中');
      return;
    }
    debugPrint('[BLE] 手动重扫...');
    await start();
  }

  /// 手动连接指定的扫描结果（用于 UI 设备选择功能）
  Future<void> connectTo(ScanResult r) async {
    if (isConnected || _status == BleStatus.connecting) {
      debugPrint('[BLE] 已有连接/正在连接，忽略手动 connectTo');
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

    _status = BleStatus.connecting;
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

  /// 停止 GATT Client
  Future<void> stop() async {
    try {
      _scanSub?.cancel();
      _scanSub = null;
      await FlutterBluePlus.stopScan();
    } catch (_) {}

    try {
      await _connSub?.cancel();
      _connSub = null;
      await _device?.disconnect();
    } catch (_) {}

    _device = null;
    _chrGuide = null;
    _chrDriveWay = null;
    _chrTmc = null;
    _chrState = null;
    _chrLocation = null;

    _status = BleStatus.stopped;
    _deviceAddress = '';
    _deviceName = '';
    _discoveredTargets.clear();
    _safeNotify();
  }

  /// 处理扫描结果
  Future<void> _onScanResult(List<ScanResult> results) async {
    if (_status != BleStatus.scanning) return;

    // 收集所有名字匹配的目标设备
    for (final r in results) {
      if (!isTargetName(r.advertisementData.advName)) continue;
      // 去重：按 MAC 保留
      final addr = r.device.remoteId.str;
      final exists = _discoveredTargets
          .any((e) => e.device.remoteId.str == addr);
      if (!exists) {
        _discoveredTargets.add(r);
        debugPrint('[BLE] 发现目标设备:'
            ' name=${r.advertisementData.advName} mac=$addr rssi=${r.rssi}');
      }
    }
    _safeNotify();

    // 用户需求：直接选中第一个匹配的设备接受数据（不依赖白名单）
    if (_hasConnectAttempted) return;
    if (_discoveredTargets.isEmpty) return;

    _hasConnectAttempted = true;
    final first = _discoveredTargets.first;
    debugPrint('[BLE] 自动选中第一个目标:'
        ' name=${first.advertisementData.advName} mac=${first.device.remoteId.str}');

    try {
      await FlutterBluePlus.stopScan();
    } catch (_) {}
    _scanSub?.cancel();
    _scanSub = null;

    _device = first.device;
    _deviceAddress = first.device.remoteId.str;
    _deviceName = first.advertisementData.advName.isNotEmpty
        ? first.advertisementData.advName
        : 'Unknown';

    _status = BleStatus.connecting;
    _safeNotify();

    try {
      await _connectAndDiscover();
    } catch (e) {
      _status = BleStatus.error;
      _lastError = '连接失败: $e';
      _device = null;
      _hasConnectAttempted = false; // 允许重试
      _safeNotify();
    }
  }

  /// 判断设备名是否匹配目标
  /// 用户需求：去白名单后，只匹配名字即可
  bool isTargetName(String name) {
    if (name.isEmpty) return false;
    return name.startsWith('AutoNavDisplay') ||
        name.startsWith('NavDisplay') ||
        name.startsWith('ESP32') ||
        name.startsWith('espressif');
  }

  /// 连接并发现服务/特征值
  Future<void> _connectAndDiscover() async {
    final device = _device;
    if (device == null) return;

    debugPrint('[BLE] >>> 开始连接 ${device.remoteId.str} <<<');

    // 监听连接状态
    _connSub?.cancel();
    _connSub = device.connectionState.listen((state) {
      debugPrint('[BLE] 连接状态变化: $state');
      if (state == BluetoothConnectionState.disconnected) {
        _status = BleStatus.scanning;
        _deviceAddress = '';
        _deviceName = '';
        _chrGuide = null;
        _chrDriveWay = null;
        _chrTmc = null;
        _chrState = null;
        _chrLocation = null;
        _hasConnectAttempted = false;
        _safeNotify();
        // 自动重连：重新启动扫描
        if (!_disposed) start();
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
    debugPrint('[BLE] 发现 ${services.length} 个服务:');
    for (var s in services) {
      debugPrint('  - ${s.uuid.str}');
    }

    BluetoothService? targetService;
    for (var s in services) {
      if (s.uuid.str.toLowerCase() == BleConstants.serviceUuid.toLowerCase()) {
        targetService = s;
        break;
      }
    }
    if (targetService == null) {
      throw '未找到目标服务 ${BleConstants.serviceUuid}';
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

    if (found != 5) {
      debugPrint('[BLE] ⚠ 警告：未找到全部 5 个特征值，BLE 写入可能失败');
    }

    _status = BleStatus.connected;
    _safeNotify();

    // 连接成功后立刻发送测试数据，验证双向通信正常
    await _sendTestPackets();
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
  /// 写入策略（修复"只有特定情况下才有数据"问题）：
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
      debugPrint('[BLE] ✗ $label: 特征值 ${chr.uuid.str} 不支持 write');
      return false;
    }
    // 优先 writeWithResponse（可靠），其次 writeWithoutResponse
    final useWriteResp = canWriteResp;
    debugPrint('[BLE] → $label 写 ${bytes.length}字节'
        ' [${useWriteResp ? "writeWithResp" : "writeNoResp"}]'
        ' uuid=${chr.uuid.str}');
    try {
      await chr.write(bytes, withoutResponse: !useWriteResp);
      debugPrint('[BLE] ✓ $label 写入成功');
      return true;
    } catch (e) {
      debugPrint('[BLE] ✗ $label 写入失败: $e');
      // 如果 writeWithResponse 失败，尝试 writeWithoutResponse
      if (useWriteResp && canWriteNoResp) {
        debugPrint('[BLE] → $label 重试: writeNoResp');
        try {
          await chr.write(bytes, withoutResponse: true);
          debugPrint('[BLE] ✓ $label 重试写入成功');
          return true;
        } catch (e2) {
          debugPrint('[BLE] ✗ $label 重试写入失败: $e2');
        }
      }
      return false;
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
