/// BLE GATT Server 管理
///
/// Flutter 端作为 GATT Server（外设）：
///   - 通过 [FlutterBluePlus] 监听蓝牙适配器状态
///   - 通过平台通道 `com.navblerelay/ble` 调用原生 Android GATT Server，
///     完成 BLE 广播启动 / 服务注册 / 特征值 notify 推送
///
/// ESP32（中心设备）连接后会订阅以下特征值：
///   - FFE1 引导信息
///   - FFE2 车道信息
///   - FFE3 路况光柱
///   - FFE4 导航状态
///   - FFE5 定位信息
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
  /// 已启动，等待 ESP32 连接
  advertising,
  /// ESP32 已连接
  connected,
  /// 启动出错
  error,
}

class BleService extends ChangeNotifier {
  /// 平台通道：与原生 Android GATT Server 通信
  static const MethodChannel _channel = MethodChannel('com.navblerelay/ble');

  BleService() {
    _adapterSub = FlutterBluePlus.adapterState.listen(_onAdapterChanged);
    // 监听原生层事件：设备连接 / 断开 / 错误
    _channel.setMethodCallHandler(_handleNativeCall);
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
      _status == BleStatus.advertising || _status == BleStatus.connected;

  bool get isConnected => _status == BleStatus.connected;

  // ── 内部 ──────────────────────────────────────────────
  StreamSubscription<BluetoothAdapterState>? _adapterSub;
  bool _disposed = false;

  /// 监听适配器状态变化
  void _onAdapterChanged(BluetoothAdapterState state) {
    if (state == BluetoothAdapterState.on) {
      // 蓝牙打开后，如果之前在运行则重新启动
      if (_status == BleStatus.bluetoothOff) {
        _status = BleStatus.stopped;
      }
    } else if (state == BluetoothAdapterState.off) {
      _status = BleStatus.bluetoothOff;
      _deviceAddress = '';
      _deviceName = '';
    }
    notifyListeners();
  }

  /// 启动 GATT Server：注册服务、开始广播
  Future<void> start() async {
    if (isRunning) return;

    // 检查蓝牙是否可用
    final adapterState = FlutterBluePlus.adapterStateNow;
    if (adapterState != BluetoothAdapterState.on) {
      _status = BleStatus.bluetoothOff;
      _lastError = '蓝牙未开启';
      notifyListeners();
      return;
    }

    try {
      await _channel.invokeMethod<bool>('start');
      _status = BleStatus.advertising;
      _lastError = '';
    } on PlatformException catch (e) {
      _status = BleStatus.error;
      _lastError = e.message ?? e.code;
    }
    notifyListeners();
  }

  /// 停止 GATT Server
  Future<void> stop() async {
    try {
      await _channel.invokeMethod<bool>('stop');
    } on PlatformException {
      // 忽略停止异常
    }
    _status = BleStatus.stopped;
    _deviceAddress = '';
    _deviceName = '';
    notifyListeners();
  }

  /// 原生层事件回调：设备连接 / 断开 / 错误
  Future<dynamic> _handleNativeCall(MethodCall call) async {
    switch (call.method) {
      case 'onDeviceConnected':
        final args = (call.arguments ?? {}) as Map;
        _deviceAddress = args['address']?.toString() ?? '';
        _deviceName = args['name']?.toString() ?? BleConstants.deviceName;
        _status = BleStatus.connected;
        notifyListeners();
        break;
      case 'onDeviceDisconnected':
        _deviceAddress = '';
        _deviceName = '';
        _status = BleStatus.advertising;
        notifyListeners();
        break;
      case 'onError':
        _lastError = call.arguments?.toString() ?? '未知错误';
        _status = BleStatus.error;
        notifyListeners();
        break;
    }
    return null;
  }

  // ── 数据发送 ──────────────────────────────────────────

  /// 发送引导信息（FFE1）
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
        // 预格式化显示字段，方便 ESP32-C3 等小内存设备直接显示
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
    await _notify(BleConstants.charGuideUuid, data);
  }

  /// 发送车道信息（FFE2）
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
    await _notify(BleConstants.charDriveWayUuid, data);
  }

  /// 发送路况光柱（FFE3）
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
    await _notify(BleConstants.charTmcUuid, data);
  }

  /// 发送导航状态（FFE4）
  Future<void> sendMapState(int state, String? crossMap) async {
    final data = <String, dynamic>{
      'type': AmapProtocol.keyMapState,
      'ts': DateTime.now().millisecondsSinceEpoch,
      'data': {
        'EXTRA_STATE': state,
        if (crossMap != null) 'EXTRA_CROSS_MAP': crossMap,
      },
    };
    await _notify(BleConstants.charStateUuid, data);
  }

  /// 发送定位信息（FFE5）
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
    await _notify(BleConstants.charLocationUuid, data);
  }

  /// 调用原生层向指定特征值 notify 推送 JSON
  Future<void> _notify(String charUuid, Map<String, dynamic> packet) async {
    if (!isConnected) return;
    final json = jsonEncode(packet);
    final bytes = utf8.encode(json);
    if (bytes.length > BleConstants.maxPacketBytes) {
      // 数据过大时仅截断提示，仍尝试发送
    }
    try {
      await _channel.invokeMethod<bool>('notify', {
        'char_uuid': charUuid,
        'value': json,
      });
    } on PlatformException {
      // 通知失败忽略，避免阻塞主流程
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

  @override
  void dispose() {
    _disposed = true;
    _adapterSub?.cancel();
    _channel.setMethodCallHandler(null);
    super.dispose();
  }
}
