/// AmapAuto 标准广播协议定义
///
/// 参考高德导航车机版公版 APP 对外暴露的字段集合：
///   - KEY_TYPE=10001 引导信息（主广播，约 1 秒更新一次）
///   - KEY_TYPE=10019 导航状态（开始 / 结束 / 到达）
///   - KEY_TYPE=13012 车道信息（临近路口时触发）
///   - KEY_TYPE=13011 实时路况光柱图
///   - KEY_TYPE=10065 定位信息（车头方向、精度等）
class AmapProtocol {
  AmapProtocol._();

  // ── Actions ──────────────────────────────────────────
  /// 高德发送的广播 —— 第三方应用侧接收
  static const String actionSend = 'AUTONAVI_STANDARD_BROADCAST_SEND';

  /// 高德接收的广播 —— 第三方应用发送给高德时使用
  static const String actionRecv = 'AUTONAVI_STANDARD_BROADCAST_RECV';

  /// 自检测试 Action —— 用于验证接收器是否正常工作
  static const String selfTestAction = 'com.navblerelay.SELF_TEST';

  /// 所有监听的高德广播 Action（在 Manifest 与运行时同时注册）
  static const List<String> allActions = [
    // 车机版标准 Action
    'AUTONAVI_STANDARD_BROADCAST_SEND',
    'AUTONAVI_STANDARD_BROADCAST_RECV',
    // 可能的带包名前缀 Action
    'com.autonavi.amapauto.ACTION_STANDARD_BROADCAST_SEND',
    'com.autonavi.amapauto.ACTION_STANDARD_BROADCAST_RECV',
    'com.autonavi.amapauto.action.STANDARD_BROADCAST',
    'com.autonavi.action.STANDARD_BROADCAST_SEND',
    // 高德地图手机版（非车机版）可能的广播
    'com.autonavi.minimap.ACTION_BROADCAST',
    'com.autonavi.minimap.action.NAV_INFO',
    'com.autonavi.action.NAVIGATION_INFO',
    'AUTONAVI_NAVI_INFO',
    'AutonaviNaviInfo',
    'com.autonavi.autonavi.action.BROADCAST_SEND',
    // 自检
    selfTestAction,
  ];

  // ── KEY_TYPE ─────────────────────────────────────────
  static const int keyGuideInfo = 10001;
  static const int keyMapState = 10019;
  static const int keyRouteInfo = 10056;
  static const int keyLocation = 10065;
  static const int keyTmcSegment = 13011;
  static const int keyDriveWay = 13012;

  // ── 导航状态值 (KEY_TYPE=10019) ─────────────────────
  static const int stateStartNav = 8;
  static const int stateStopNav = 9;
  static const int stateArriveDest = 39;

  // ── 转向图标含义（guide_info 中的 ICON 字段）───
  static const Map<int, String> iconMap = {
    0: '未定义',
    1: '直行',
    2: '左转',
    3: '右转',
    4: '左前方',
    5: '右前方',
    6: '左后方',
    7: '右后方',
    8: '左转掉头',
    9: '直行',
    10: '到达途经点',
    11: '进入环岛',
    12: '驶出环岛',
    13: '到达服务区',
    14: '到达收费站',
    15: '到达目的地',
    16: '进入隧道',
    17: '进入环岛(左行)',
    18: '驶出环岛(左行)',
    19: '右转掉头',
    20: '顺行',
  };

  /// 道路类型（ROAD_TYPE 字段）
  static const Map<int, String> roadTypeMap = {
    0: '高速公路',
    1: '国道',
    2: '省道',
    3: '县道',
    4: '乡道',
    5: '县乡村内部道路',
    6: '主要大街/城市快速道',
    7: '主要道路',
    8: '次要道路',
    9: '普通道路',
    10: '非导航道路',
  };

  /// 电子眼类型（CAMERA_TYPE 字段）
  static const Map<int, String> cameraTypeMap = {
    0: '测速摄像头',
    1: '监控摄像头',
    2: '闯红灯拍照',
    3: '违章拍照',
    4: '公交专用道摄像头',
  };

  /// TMC 路况状态
  static const Map<int, String> tmcStatusMap = {
    -1: '无数据',
    0: '未知',
    1: '畅通',
    2: '缓行',
    3: '拥堵',
    4: '严重拥堵',
  };

  /// 转向图标 → 箭头旋转角度（0 表示直行向上）
  static const Map<int, int> iconRotation = {
    0: 0,
    1: 0,
    2: -90,
    3: 90,
    4: -45,
    5: 45,
    6: -135,
    7: 135,
    8: 180,
    9: 0,
    10: 0,
    11: 0,
    12: 0,
    13: 0,
    14: 0,
    15: 0,
    16: 0,
    17: 0,
    18: 0,
    19: -180,
    20: 0,
  };

  /// 转向图标 → 简短方向标签（用于小屏显示）
  static const Map<int, String> iconShort = {
    0: '直行',
    1: '直行',
    2: '左转',
    3: '右转',
    4: '左前方',
    5: '右前方',
    6: '左后方',
    7: '右后方',
    8: '掉头',
    9: '直行',
    10: '途经点',
    11: '环岛',
    12: '出环岛',
    13: '服务区',
    14: '收费站',
    15: '到达',
    16: '隧道',
    17: '人行横道',
    18: '过街天桥',
    19: '地下通道',
    20: '顺行',
  };

  /// 车道指引图标含义（drive_way_info 中的 backIcon 字段）
  static const Map<int, String> laneBackIconMap = {
    0: '直行',
    1: '左转',
    2: '直行和左转',
    3: '右转',
    4: '直行和右转',
    5: '左转掉头',
    6: '左转和右转',
    7: '直行和左转和右转',
  };

  /// 车道图标 → 显示符号
  static const Map<int, String> laneSymbol = {
    0: '↑',
    1: '↖',
    2: '↗',
    3: '←',
    4: '→',
    5: '↙',
    6: '↘',
    7: '↩',
  };

  // ── 便捷方法 ─────────────────────────────────────────
  /// 获取转向图标中文描述，不存在时返回 "未知(值)"
  static String iconLabel(int id) => iconMap[id] ?? '未知($id)';

  static String iconShortLabel(int id) => iconShort[id] ?? '未知($id)';

  static int iconRotationAngle(int id) => iconRotation[id] ?? 0;

  static String laneBackIconLabel(int id) =>
      laneBackIconMap[id] ?? '未知($id)';

  static String roadLabel(int id) => roadTypeMap[id] ?? '未知($id)';

  static String cameraLabel(int id) => cameraTypeMap[id] ?? '未知($id)';

  static String tmcLabel(int id) => tmcStatusMap[id] ?? '未知($id)';

  /// 车道方向符号
  static String laneSymbolLabel(int id) => laneSymbol[id] ?? '•';
}
