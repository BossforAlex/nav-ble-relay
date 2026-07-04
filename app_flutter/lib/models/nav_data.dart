/// 导航数据模型
///
/// 与高德广播协议字段一一对应，方便 ESP32 端解析。

/// 引导信息（KEY_TYPE=10001）
///
/// 高德导航车机版最核心的广播，包含当前道路、下一道路、剩余距离时间、
/// 当前速度、限速、电子眼、服务区、红绿灯等信息。
class GuideInfo {
  final int type;
  final String curRoadName;
  final String nextRoadName;
  final String nextNextRoadName;
  final int icon;
  final int nextNextTurnIcon;
  final int routeRemainDis;
  final int routeRemainTime;
  final int routeAllDis;
  final int routeAllTime;
  final int segRemainDis;
  final int segRemainTime;
  final int nextSegRemainDis;
  final double carLatitude;
  final double carLongitude;
  final int carDirection;
  final int curSpeed;
  final int limitedSpeed;
  final int roadType;
  final int cameraDist;
  final int cameraType;
  final int cameraSpeed;
  final int sapaDist;
  final String sapaName;
  final int trafficLightNum;
  final int roundAboutNum;
  final int roundAllNum;
  final int curSegNum;
  final int curPointNum;

  const GuideInfo({
    this.type = 0,
    this.curRoadName = '',
    this.nextRoadName = '',
    this.nextNextRoadName = '',
    this.icon = -1,
    this.nextNextTurnIcon = -1,
    this.routeRemainDis = 0,
    this.routeRemainTime = 0,
    this.routeAllDis = 0,
    this.routeAllTime = 0,
    this.segRemainDis = 0,
    this.segRemainTime = 0,
    this.nextSegRemainDis = 0,
    this.carLatitude = 0.0,
    this.carLongitude = 0.0,
    this.carDirection = 0,
    this.curSpeed = 0,
    this.limitedSpeed = 0,
    this.roadType = -1,
    this.cameraDist = 0,
    this.cameraType = -1,
    this.cameraSpeed = 0,
    this.sapaDist = 0,
    this.sapaName = '',
    this.trafficLightNum = 0,
    this.roundAboutNum = 0,
    this.roundAllNum = 0,
    this.curSegNum = 0,
    this.curPointNum = 0,
  });

  /// 从高德广播 Bundle 解析出的 Map 构造
  factory GuideInfo.fromMap(Map<dynamic, dynamic> m) {
    int asInt(Object? v, [int def = 0]) =>
        v is int ? v : (v is num ? v.toInt() : (v is String ? int.tryParse(v) ?? def : def));
    double asDouble(Object? v, [double def = 0.0]) =>
        v is double ? v : (v is num ? v.toDouble() : (v is String ? double.tryParse(v) ?? def : def));
    String asString(Object? v, [String def = '']) =>
        v?.toString() ?? def;

    return GuideInfo(
      type: asInt(m['TYPE'] ?? m['type']),
      curRoadName: asString(m['CUR_ROAD_NAME'] ?? m['curRoadName']),
      nextRoadName: asString(m['NEXT_ROAD_NAME'] ?? m['nextRoadName']),
      nextNextRoadName: asString(m['NEXT_NEXT_ROAD_NAME']),
      icon: asInt(m['ICON'] ?? m['icon'], -1),
      nextNextTurnIcon: asInt(m['NEXT_NEXT_TURN_ICON'], -1),
      routeRemainDis: asInt(m['ROUTE_REMAIN_DIS'] ?? m['routeRemainDis']),
      routeRemainTime: asInt(m['ROUTE_REMAIN_TIME'] ?? m['routeRemainTime']),
      routeAllDis: asInt(m['ROUTE_ALL_DIS']),
      routeAllTime: asInt(m['ROUTE_ALL_TIME']),
      segRemainDis: asInt(m['SEG_REMAIN_DIS']),
      segRemainTime: asInt(m['SEG_REMAIN_TIME']),
      nextSegRemainDis: asInt(m['NEXT_SEG_REMAIN_DIS']),
      carLatitude: asDouble(m['CAR_LATITUDE'] ?? m['carLatitude']),
      carLongitude: asDouble(m['CAR_LONGITUDE'] ?? m['carLongitude']),
      carDirection: asInt(m['CAR_DIRECTION'] ?? m['carDirection']),
      curSpeed: asInt(m['CUR_SPEED'] ?? m['curSpeed']),
      limitedSpeed: asInt(m['LIMITED_SPEED'] ?? m['limitedSpeed']),
      roadType: asInt(m['ROAD_TYPE'] ?? m['roadType'], -1),
      cameraDist: asInt(m['CAMERA_DIST']),
      cameraType: asInt(m['CAMERA_TYPE'], -1),
      cameraSpeed: asInt(m['CAMERA_SPEED']),
      sapaDist: asInt(m['SAPA_DIST']),
      sapaName: asString(m['SAPA_NAME']),
      trafficLightNum: asInt(m['TRAFFIC_LIGHT_NUM']),
      roundAboutNum: asInt(m['ROUND_ABOUT_NUM']),
      roundAllNum: asInt(m['ROUND_ALL_NUM']),
      curSegNum: asInt(m['CUR_SEG_NUM']),
      curPointNum: asInt(m['CUR_POINT_NUM']),
    );
  }

  GuideInfo copyWith({
    int? icon,
    int? segRemainDis,
    int? curSpeed,
    int? limitedSpeed,
    String? curRoadName,
    String? nextRoadName,
  }) =>
      GuideInfo(
        type: type,
        curRoadName: curRoadName ?? this.curRoadName,
        nextRoadName: nextRoadName ?? this.nextRoadName,
        nextNextRoadName: nextNextRoadName,
        icon: icon ?? this.icon,
        nextNextTurnIcon: nextNextTurnIcon,
        routeRemainDis: routeRemainDis,
        routeRemainTime: routeRemainTime,
        routeAllDis: routeAllDis,
        routeAllTime: routeAllTime,
        segRemainDis: segRemainDis ?? this.segRemainDis,
        segRemainTime: segRemainTime,
        nextSegRemainDis: nextSegRemainDis,
        carLatitude: carLatitude,
        carLongitude: carLongitude,
        carDirection: carDirection,
        curSpeed: curSpeed ?? this.curSpeed,
        limitedSpeed: limitedSpeed ?? this.limitedSpeed,
        roadType: roadType,
        cameraDist: cameraDist,
        cameraType: cameraType,
        cameraSpeed: cameraSpeed,
        sapaDist: sapaDist,
        sapaName: sapaName,
        trafficLightNum: trafficLightNum,
        roundAboutNum: roundAboutNum,
        roundAllNum: roundAllNum,
        curSegNum: curSegNum,
        curPointNum: curPointNum,
      );
}

/// 单条车道信息
class LaneInfo {
  final int number;
  final int backIcon;

  const LaneInfo({this.number = 0, this.backIcon = -1});

  factory LaneInfo.fromMap(Map<dynamic, dynamic> m) => LaneInfo(
        number: _asInt(m['drive_way_number']),
        backIcon: _asInt(m['drive_way_lane_Back_icon'], -1),
      );
}

/// 车道信息（KEY_TYPE=13012）
class DriveWayInfo {
  final bool enabled;
  final int size;
  final List<LaneInfo> lanes;

  const DriveWayInfo({
    this.enabled = false,
    this.size = 0,
    this.lanes = const [],
  });

  factory DriveWayInfo.fromMap(Map<dynamic, dynamic> m) {
    final list = <LaneInfo>[];
    final arr = m['drive_way_info'];
    if (arr is List) {
      for (final item in arr) {
        if (item is Map) list.add(LaneInfo.fromMap(item));
      }
    }
    return DriveWayInfo(
      enabled: m['drive_way_enabled'] == true,
      size: _asInt(m['drive_way_size']),
      lanes: list,
    );
  }
}

/// 单段路况
class TmcSegment {
  final int number;
  final int status;
  final int distance;
  final String percent;

  const TmcSegment({
    this.number = 0,
    this.status = -1,
    this.distance = 0,
    this.percent = '0',
  });

  factory TmcSegment.fromMap(Map<dynamic, dynamic> m) => TmcSegment(
        number: _asInt(m['tmc_segment_number']),
        status: _asInt(m['tmc_status'], -1),
        distance: _asInt(m['tmc_segment_distance']),
        percent: m['tmc_segment_percent']?.toString() ?? '0',
      );
}

/// 路况光柱图（KEY_TYPE=13011）
class TmcSegmentInfo {
  final bool enabled;
  final int size;
  final int totalDistance;
  final int residualDistance;
  final int finishDistance;
  final List<TmcSegment> segments;

  const TmcSegmentInfo({
    this.enabled = false,
    this.size = 0,
    this.totalDistance = 0,
    this.residualDistance = 0,
    this.finishDistance = 0,
    this.segments = const [],
  });

  factory TmcSegmentInfo.fromMap(Map<dynamic, dynamic> m) {
    final list = <TmcSegment>[];
    final arr = m['tmc_info'];
    if (arr is List) {
      for (final item in arr) {
        if (item is Map) list.add(TmcSegment.fromMap(item));
      }
    }
    return TmcSegmentInfo(
      enabled: m['tmc_segment_enabled'] == true,
      size: _asInt(m['tmc_segment_size']),
      totalDistance: _asInt(m['total_distance']),
      residualDistance: _asInt(m['residual_distance']),
      finishDistance: _asInt(m['finish_distance']),
      segments: list,
    );
  }
}

/// 定位信息（KEY_TYPE=10065）
class LocationInfo {
  final int bearing;
  final int accuracy;
  final int speed;
  final int time;
  final String provider;

  const LocationInfo({
    this.bearing = 0,
    this.accuracy = 0,
    this.speed = 0,
    this.time = 0,
    this.provider = '',
  });

  factory LocationInfo.fromMap(Map<dynamic, dynamic> m) => LocationInfo(
        bearing: _asInt(m['bearing']),
        accuracy: _asInt(m['accuracy']),
        speed: _asInt(m['speed']),
        time: _asInt(m['time']),
        provider: m['provider']?.toString() ?? '',
      );
}

/// 导航状态码（KEY_TYPE=10019）
class MapStateInfo {
  final int state;
  final String? crossMap;

  const MapStateInfo({this.state = -1, this.crossMap});

  factory MapStateInfo.fromMap(Map<dynamic, dynamic> m) => MapStateInfo(
        state: _asInt(m['EXTRA_STATE'] ?? m['extraState'], -1),
        crossMap: m['EXTRA_CROSS_MAP']?.toString(),
      );
}

/// BLE 传输数据包（统一结构，便于 ESP32 侧解析）
class BleDataPacket {
  final int type;
  final int ts;
  final Map<String, dynamic> data;

  const BleDataPacket({required this.type, required this.ts, required this.data});

  Map<String, dynamic> toJson() => {
        'type': type,
        'ts': ts,
        'data': data,
      };
}

// ── 内部工具 ──────────────────────────────────────────
int _asInt(Object? v, [int def = 0]) =>
    v is int ? v : (v is num ? v.toInt() : (v is String ? int.tryParse(v) ?? def : def));
