/// Android 广播接收服务（平台通道）
///
/// 通过平台通道 `com.navblerelay/broadcast` 与原生 [NavBroadcastReceiver] 通信：
///   - `start` 启动广播监听
///   - `stop` 停止广播监听
///   - 原生侧在收到高德广播后，解析 Bundle 并回调下列方法：
///     `onGuideInfo` / `onMapState` / `onDriveWay` / `onTmcSegment` / `onLocation`
///
/// 该服务作为 [ChangeNotifier]，向 UI 层暴露最新导航数据。
library;

import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import '../models/nav_data.dart';
import '../protocol/amap_protocol.dart';

class BroadcastService extends ChangeNotifier {
  static const MethodChannel _channel =
      MethodChannel('com.navblerelay/broadcast');

  BroadcastService() {
    _channel.setMethodCallHandler(_handleNativeCall);
  }

  // ── 数据字段 ──────────────────────────────────────────
  GuideInfo? guideInfo;
  DriveWayInfo? driveWayInfo;
  TmcSegmentInfo? tmcSegmentInfo;
  LocationInfo? locationInfo;
  int mapState = -1;
  String? crossMap;

  /// 最近一次收到广播的时间戳（毫秒）
  int broadcastReceived = 0;

  /// 最近一次收到广播的 Action
  String lastBroadcastAction = '';

  /// v0.5.8: relay 到 BLE 的计数（UI 可见，用于诊断）
  int _relayCount = 0;
  int get relayCount => _relayCount;
  DateTime? _lastRelayAt;
  String get lastRelayText {
    if (_lastRelayAt == null) return '—';
    final diff = DateTime.now().difference(_lastRelayAt!);
    return '${diff.inSeconds}s 前';
  }

  /// 是否正在监听
  bool _listening = false;
  bool get isListening => _listening;

  // ── 控制方法 ──────────────────────────────────────────

  /// 启动广播监听
  Future<void> start() async {
    try {
      await _channel.invokeMethod<bool>('start');
      _listening = true;
    } on PlatformException catch (e) {
      debugPrint('启动广播监听失败: ${e.message}');
    }
    notifyListeners();
  }

  /// 停止广播监听
  Future<void> stop() async {
    try {
      await _channel.invokeMethod<bool>('stop');
    } on PlatformException {
      // 忽略
    }
    _listening = false;
    notifyListeners();
  }

  /// 发送自检广播，验证接收器是否正常工作
  Future<void> sendSelfTest() async {
    try {
      await _channel.invokeMethod<bool>('selfTest');
    } on PlatformException {
      // 忽略
    }
  }

  /// v0.5.8: 模拟导航广播（绕过 Android 原生层，直接生成数据）
  /// 用于诊断 broadcast→relay→BLE 完整链路是否通畅
  void simulateNavigation() {
    guideInfo = GuideInfo(
      icon: 0, // 直行
      curRoadName: '模拟测试道路',
      nextRoadName: '模拟下一条路',
      segRemainDis: 1200,
      curSpeed: 180,
      limitedSpeed: 120,
      roadType: 1,
      cameraDist: 800,
      cameraType: 1,
      cameraSpeed: 120,
    );
    mapState = 0; // 导航中
    crossMap = null;
    lastBroadcastAction = 'SIMULATE_NAV';
    broadcastReceived = DateTime.now().millisecondsSinceEpoch;
    notifyListeners();
  }

  /// 重置所有数据
  void reset() {
    guideInfo = null;
    driveWayInfo = null;
    tmcSegmentInfo = null;
    locationInfo = null;
    mapState = -1;
    crossMap = null;
    broadcastReceived = 0;
    lastBroadcastAction = '';
    notifyListeners();
  }

  /// 距离上次收到广播的秒数
  int get secondsSinceBroadcast {
    if (broadcastReceived == 0) return -1;
    return (DateTime.now().millisecondsSinceEpoch - broadcastReceived) ~/ 1000;
  }

  /// 友好的时间描述
  String get broadcastAgoText {
    final sec = secondsSinceBroadcast;
    if (sec < 0) return '未接收';
    if (sec < 60) return '已接收 ($sec 秒前)';
    return '已接收 (${sec ~/ 60} 分钟前)';
  }

  // ── 原生回调处理 ──────────────────────────────────────

  Future<dynamic> _handleNativeCall(MethodCall call) async {
    broadcastReceived = DateTime.now().millisecondsSinceEpoch;
    final args = call.arguments;

    switch (call.method) {
      case 'onGuideInfo':
        guideInfo =
            GuideInfo.fromMap((args ?? {}) as Map);
        break;
      case 'onMapState':
        final m = (args ?? {}) as Map;
        mapState = m['EXTRA_STATE'] is int
            ? m['EXTRA_STATE'] as int
            : int.tryParse('${m['EXTRA_STATE'] ?? -1}') ?? -1;
        crossMap = m['EXTRA_CROSS_MAP']?.toString();
        break;
      case 'onDriveWay':
        driveWayInfo = DriveWayInfo.fromMap(_unwrapJson(args));
        break;
      case 'onTmcSegment':
        tmcSegmentInfo = TmcSegmentInfo.fromMap(_unwrapJson(args));
        break;
      case 'onLocation':
        locationInfo = LocationInfo.fromMap(_unwrapJson(args));
        break;
      case 'onAction':
        lastBroadcastAction = args?.toString() ?? '';
        notifyListeners();
        return null;
      default:
        return null;
    }
    notifyListeners();
    return null;
  }

  /// 原生传过来的 Map 直接返回
  Map<dynamic, dynamic> _unwrap(dynamic args) {
    if (args is Map) return args;
    return <dynamic, dynamic>{};
  }

  /// 原生侧对 JSON 结构（车道 / 路况 / 定位）以 `{"__json__": "<json>"}`
  /// 形式透传，这里解码为 Map 供 [fromMap] 解析。
  Map<dynamic, dynamic> _unwrapJson(dynamic args) {
    if (args is Map) {
      final raw = args['__json__'];
      if (raw is String && raw.isNotEmpty) {
        try {
          final decoded = jsonDecode(raw);
          if (decoded is Map) return decoded;
        } catch (e) {
          debugPrint('JSON 解析失败: $e');
        }
      }
      return args;
    }
    return <dynamic, dynamic>{};
  }

  @override
  void dispose() {
    _channel.setMethodCallHandler(null);
    super.dispose();
  }
}

/// 导航状态码文本（与原 SettingsActivity 状态映射一致）
extension MapStateText on BroadcastService {
  String get mapStateText {
    switch (mapState) {
      case -1:
        return '—';
      case 0:
        return '空闲 / IDLE';
      case 1:
        return '导航中 / NAVIGATING';
      case 2:
        return '已到达 / ARRIVED';
      case 3:
        return '已暂停 / PAUSED';
      default:
        return '状态: $mapState';
    }
  }

  /// 导航是否处于进行中
  bool get isNavigating =>
      mapState == AmapProtocol.stateStartNav;
}
