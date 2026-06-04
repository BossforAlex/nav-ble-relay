package com.navblerelay.protocol

/**
 * AmapAuto 标准广播协议定义
 * 参考：AmapAuto标准广播协议_20180813
 */
object AmapAutoProtocol {

    // ── Actions ──────────────────────────────────────────
    /** 高德发送的广播 */
    const val ACTION_SEND = "AUTONAVI_STANDARD_BROADCAST_SEND"
    /** 高德接收的广播 */
    const val ACTION_RECV = "AUTONAVI_STANDARD_BROADCAST_RECV"

    // ── KEY_TYPE ─────────────────────────────────────────
    /** 引导信息 */
    const val KEY_GUIDE_INFO = 10001
    /** 地图状态/心跳 */
    const val KEY_MAP_STATE = 10019
    /** 路线信息 */
    const val KEY_ROUTE_INFO = 10056
    /** 定位信息（车头方向） */
    const val KEY_LOCATION = 10065
    /** 实时交通光柱图 */
    const val KEY_TMC_SEGMENT = 13011
    /** 车道信息 */
    const val KEY_DRIVE_WAY = 13012

    // ── 导航状态值 (KEY_TYPE=10019) ─────────────────────
    const val STATE_START_NAV = 8
    const val STATE_STOP_NAV = 9
    const val STATE_ARRIVE_DEST = 39

    // ── 转向图标含义 ─────────────────────────────────────
    val ICON_MAP = mapOf(
        1 to "自车",
        2 to "左转",
        3 to "右转",
        4 to "左前方",
        5 to "右前方",
        6 to "左后方",
        7 to "右后方",
        8 to "左转掉头",
        9 to "直行",
        10 to "到达途经点",
        11 to "进入环岛",
        12 to "驶出环岛",
        13 to "到达服务区",
        14 to "到达收费站",
        15 to "到达目的地",
        16 to "进入隧道",
        17 to "进入环岛(左行)",
        18 to "驶出环岛(左行)",
        19 to "右转掉头",
        20 to "顺行"
    )

    /** 道路类型 */
    val ROAD_TYPE_MAP = mapOf(
        0 to "高速公路",
        1 to "国道",
        2 to "省道",
        3 to "县道",
        4 to "乡公路",
        5 to "县乡村内部道路",
        6 to "主要大街/城市快速道",
        7 to "主要道路",
        8 to "次要道路",
        9 to "普通道路",
        10 to "非导航道路"
    )

    /** 电子眼类型 */
    val CAMERA_TYPE_MAP = mapOf(
        0 to "测速摄像头",
        1 to "监控摄像头",
        2 to "闯红灯拍照",
        3 to "违章拍照",
        4 to "公交专用道摄像头"
    )
}

// ── 数据类 ──────────────────────────────────────────────

/** 引导信息 (KEY_TYPE=10001) */
data class GuideInfo(
    val type: Int = 0,                    // 0:GPS导航 1:模拟导航 2:巡航
    val curRoadName: String = "",         // 当前道路名称
    val nextRoadName: String = "",        // 下一道路名称
    val nextNextRoadName: String = "",    // 下下个道路名称
    val icon: Int = -1,                   // 转向图标
    val nextNextTurnIcon: Int = -1,       // 下下个转向图标
    val routeRemainDis: Int = 0,          // 剩余距离(米)
    val routeRemainTime: Int = 0,         // 剩余时间(秒)
    val routeAllDis: Int = 0,             // 总距离(米)
    val routeAllTime: Int = 0,            // 总时间(秒)
    val segRemainDis: Int = 0,            // 当前段剩余距离(米)
    val segRemainTime: Int = 0,           // 当前段剩余时间(秒)
    val nextSegRemainDis: Int = 0,        // 下下个路口距离(米)
    val carLatitude: Double = 0.0,        // 纬度
    val carLongitude: Double = 0.0,       // 经度
    val carDirection: Int = 0,            // 车头方向(度)
    val curSpeed: Int = 0,                // 当前车速(km/h)
    val limitedSpeed: Int = 0,            // 限速(km/h)
    val roadType: Int = -1,               // 道路类型
    val cameraDist: Int = 0,              // 电子眼距离(米)
    val cameraType: Int = -1,             // 电子眼类型
    val cameraSpeed: Int = 0,             // 电子眼限速(km/h)
    val sapaDist: Int = 0,                // 服务区距离(米)
    val sapaName: String = "",            // 服务区名称
    val trafficLightNum: Int = 0,         // 红绿灯个数
    val roundAboutNum: Int = 0,           // 环岛出口序号
    val roundAllNum: Int = 0,             // 环岛出口个数
    val curSegNum: Int = 0,
    val curPointNum: Int = 0
)

/** 车道信息 (KEY_TYPE=13012) */
data class DriveWayInfo(
    val enabled: Boolean = false,
    val size: Int = 0,
    val lanes: List<LaneInfo> = emptyList()
)

data class LaneInfo(
    val number: Int = 0,        // 车道编号
    val backIcon: Int = -1      // 车道图标 ID
)

/** 路况光柱图 (KEY_TYPE=13011) */
data class TmcSegmentInfo(
    val enabled: Boolean = false,
    val size: Int = 0,
    val totalDistance: Int = 0,
    val residualDistance: Int = 0,
    val finishDistance: Int = 0,
    val segments: List<TmcSegment> = emptyList()
)

data class TmcSegment(
    val number: Int = 0,
    val status: Int = -1,   // -1无数据 0未知 1畅通 2缓行 3拥堵 4严重拥堵
    val distance: Int = 0,
    val percent: String = "0"
)

/** 定位信息 (KEY_TYPE=10065) */
data class LocationInfo(
    val bearing: Int = 0,
    val accuracy: Int = 0,
    val speed: Int = 0,
    val time: Long = 0L,
    val provider: String = ""
)

/** BLE 传输数据包 */
data class BleDataPacket(
    val type: Int,            // KEY_TYPE
    val ts: Long,             // 时间戳
    val data: Map<String, Any>
)