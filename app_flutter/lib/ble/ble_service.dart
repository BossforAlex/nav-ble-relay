/// BLE GATT Client 管理
///
/// Flutter 端作为 GATT Client（中心设备）：
///   - 主动扫描名为 "AutoNavDisplay" 的 ESP32 设备
///   - 在用户配置的 MAC 白名单匹配后，连接该 ESP32
///   - 通过 writeCharacteristic / writeCharacteristicWithoutResponse
///     主动向 5 个特征值推送 JSON 数据
///
/// ESP32（外设）连接后被动接收数据即可。
///
/// 用户需求：
///   - 手机连接数较多，软件中要做 MAC 限制
///   - ESP32 端不做限制（被动接收）
library;

import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../ble/ble_constants.dart';
import '../models/nav_data.dart';
import '../protocol/amap_protocol.dart';
import '../main.dart' show prefs;

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

  bool get isRunning =>
      _status == BleStatus.scanning ||
      _status == BleStatus.connecting ||
      _status == BleStatus.connected;

  bool get isConnected => _status == BleStatus.connected;

  // ── 内部 ──────────────────────────────────────────────
  StreamSubscription<BluetoothAdapterState>? _adapterSub;
  StreamSubscription<ScanResult>? _scanSub;
  StreamSubscription<BluetoothConnectionState>? _connSub;
  bool _disposed = false;

  BluetoothDevice? _device;
  BluetoothCharacteristic? _chrGuide;
  BluetoothCharacteristic? _chrDriveWay;
  BluetoothCharacteristic? _chrTmc;
  BluetoothCharacteristic? _chrState;
  BluetoothCharacteristic? _chrLocation;

  // 当前是否允许 writeWithoutResponse（部分外设不支持）
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

    _status = BleStatus.scanning;
    _lastError = '';
    _safeNotify();

    try {
      // 1. 启动扫描
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
    _safeNotify();
  }

  /// 处理扫描结果
  Future<void> _onScanResult(ScanResult r) async {
    if (_status != BleStatus.scanning) return;
    if (!isTargetName(r.advertisementData.advName)) return;
    if (!isAllowedAddress(r.device.remoteId.str)) return;

    // 找到目标设备，停止扫描并发起连接
    try {
      await FlutterBluePlus.stopScan();
    } catch (_) {}
    _scanSub?.cancel();
    _scanSub = null;

    _device = r.device;
    _deviceAddress = r.device.remoteId.str;
    _deviceName = r.advertisementData.advName.isNotEmpty
        ? r.advertisementData.advName
        : BleConstants.deviceNamePrefix;

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

  /// 判断设备名是否匹配目标（AutoNavDisplay 前缀或以 ICA 开头的别名）
  bool isTargetName(String name) {
    if (name.isEmpty) return false;
    return name.startsWith('AutoNavDisplay') ||
        name.startsWith('NavDisplay') ||
        name == 'AutoNavDisplay' ||
        name.startsWith(BleConstants.deviceNamePrefix) ||
        name.startsWith('ESP32') ||
        name.startsWith('espressif');
  }

  /// 判断 MAC 是否在白名单（用户需求：手机端做限制）
  bool isAllowedAddress(String address) {
    final target = prefs.getString('target_device_mac')?.trim() ?? '';
    if (target.isEmpty) return true; // 未配置白名单时不限
    return address.toLowerCase() == target.toLowerCase();
  }

  /// 连接并发现服务/特征值
  Future<void> _connectAndDiscover() async {
    final device = _device;
    if (device == null) return;

    // 监听连接状态
    _connSub?.cancel();
    _connSub = device.connectionState.listen((state) {
      if (state == BluetoothConnectionState.disconnected) {
        _status = BleStatus.scanning;
        _deviceAddress = '';
        _deviceName = '';
        _chrGuide = null;
        _chrDriveWay = null;
        _chrTmc = null;
        _chrState = null;
        _chrLocation = null;
        _safeNotify();
        // 自动重连：重新启动扫描
        if (!_disposed) start();
      }
    });

    await device.connect(timeout: const Duration(seconds: 10));

    // 请求更大 MTU
    try {
      await device.requestMtu(512);
    } catch (_) {}

    // 发现服务
    List<BluetoothService> services = await device.discoverServices();

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

    for (var c in targetService.characteristics) {
      final u = c.uuid.str.toLowerCase();
      if (u == BleConstants.charGuideUuid.toLowerCase()) {
        _chrGuide = c;
      } else if (u == BleConstants.charDriveWayUuid.toLowerCase()) {
        _chrDriveWay = c;
      } else if (u == BleConstants.charTmcUuid.toLowerCase()) {
        _chrTmc = c;
      } else if (u == BleConstants.charStateUuid.toLowerCase()) {
        _chrState = c;
      } else if (u == BleConstants.charLocationUuid.toLowerCase()) {
        _chrLocation = c;
      }
      // 检查是否支持 writeWithoutResponse
      if (!c.properties.writeWithoutResponse) {
        _supportWriteNoResp = false;
      }
    }

    _status = BleStatus.connected;
    _safeNotify();
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
    await _write(_chrGuide, data);
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
    await _write(_chrDriveWay, data);
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
    await _write(_chrTmc, data);
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
    await _write(_chrState, data);
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
    await _write(_chrLocation, data);
  }

  /// 向指定特征值写入 JSON 数据
  Future<void> _write(BluetoothCharacteristic? chr, Map<String, dynamic> packet) async {
    if (!isConnected) return;
    if (chr == null) return;
    final json = jsonEncode(packet);
    final bytes = utf8.encode(json);
    try {
      if (_supportWriteNoResp && chr.properties.writeWithoutResponse) {
        await chr.write(bytes, withoutResponse: true);
      } else {
        await chr.write(bytes, withoutResponse: false);
      }
    } catch (e) {
      // 写入失败忽略，避免阻塞主流程
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
