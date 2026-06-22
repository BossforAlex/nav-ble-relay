package com.navblerelay.protocol

/**
 * AmapAuto 标准广播协议定义
 *
 * 参考文档 §3 AmapAuto 标准广播协议：
 *   - KEY_TYPE=10001 为引导信息（主广播，持续 1 秒左右更新一次）
 *   - KEY_TYPE=10019 为导航状态（开始 / 结束 / 到达）
 *   - KEY_TYPE=13012 为车道信息（临近路口时触发）
 *   - KEY_TYPE=13011 为实时路况光柱图
 *   - KEY_TYPE=10065 为定位信息（车头方向、精度等）
 *
 * 以上字段是 AmapAuto 公版 APP 对外暴露的、可在第三方应用中解析的字段集合。
 */
object AmapAutoProtocol {

    // ── Actions ──────────────────────────────────────────
    /** 高德发送的广播 —— 第三方应用侧接收 */
    const val ACTION_SEND = "AUTONAVI_STANDARD_BROADCAST_SEND"
    /** 高德接收的广播 —— 第三方应用发送给高德时使用 */
    const val ACTION_RECV = "AUTONAVI_STANDARD_BROADCAST_RECV"

    // ── KEY_TYPE ─────────────────────────────────────────
    const val KEY_GUIDE_INFO = 10001
    const val KEY_MAP_STATE = 10019
    const val KEY_ROUTE_INFO = 10056
    const val KEY_LOCATION = 10065
    const val KEY_TMC_SEGMENT = 13011
    const val KEY_DRIVE_WAY = 13012

    // ── 导航状态值 (KEY_TYPE=10019) ─────────────────────
    const val STATE_START_NAV = 8
    const val STATE_STOP_NAV = 9
    const val STATE_ARRIVE_DEST = 39

    // ── 转向图标含义（§3.1.3 guide_info 中的 ICON 字段）───
    val ICON_MAP = mapOf(
        0 to "未定义",
        1 to "直行",
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

    /** 道路类型（ROAD_TYPE 字段） */
    val ROAD_TYPE_MAP = mapOf(
        0 to "高速公路",
        1 to "国道",
        2 to "省道",
        3 to "县道",
        4 to "乡道",
        5 to "县乡村内部道路",
        6 to "主要大街/城市快速道",
        7 to "主要道路",
        8 to "次要道路",
        9 to "普通道路",
        10 to "非导航道路"
    )

    /** 电子眼类型（CAMERA_TYPE 字段） */
    val CAMERA_TYPE_MAP = mapOf(
        0 to "测速摄像头",
        1 to "监控摄像头",
        2 to "闯红灯拍照",
        3 to "违章拍照",
        4 to "公交专用道摄像头"
    )

    /** TMC 路况状态 */
    val TMC_STATUS_MAP = mapOf(
        -1 to "无数据",
        0 to "未知",
        1 to "畅通",
        2 to "缓行",
        3 to "拥堵",
        4 to "严重拥堵"
    )

    /** 转向图标 → 箭头旋转角度（0 表示直行向上） */
    val ICON_ROTATION = mapOf(
        0 to 0,
        1 to 0,
        2 to -90,
        3 to 90,
        4 to -45,
        5 to 45,
        6 to -135,
        7 to 135,
        8 to 180,
        9 to 0,
        10 to 0,
        11 to 0,
        12 to 0,
        13 to 0,
        14 to 0,
        15 to 0,
        16 to 0,
        17 to 0,
        18 to 0,
        19 to -180,
        20 to 0
    )

    /** 转向图标 → 简短方向标签（用于小屏显示） */
    val ICON_SHORT = mapOf(
        0 to "直行",
        1 to "直行",
        2 to "左转",
        3 to "右转",
        4 to "左前方",
        5 to "右前方",
        6 to "左后方",
        7 to "右后方",
        8 to "掉头",
        9 to "直行",
        10 to "途经点",
        11 to "环岛",
        12 to "出环岛",
        13 to "服务区",
        14 to "收费站",
        15 to "到达",
        16 to "隧道",
        17 to "人行横道",
        18 to "过街天桥",
        19 to "地下通道",
        20 to "顺行"
    )

    /**
     * 车道指引图标含义（drive_way_info 中的 backIcon 字段）。
     * 参考高德 iOS 导航 SDK 的 CreateLaneInfoImageWithLaneInfo 规范。
     */
    val LANE_BACK_ICON_MAP = mapOf(
        0 to "直行",
        1 to "左转",
        2 to "直行和左转",
        3 to "右转",
        4 to "直行和右转",
        5 to "左转掉头",
        6 to "左转和右转",
        7 to "直行和左转和右转"
    )

    /** 便捷方法：获取中文描述，不存在时返回 "未知(值)" */
    fun iconLabel(id: Int): String = ICON_MAP[id] ?: "未知($id)"
    fun iconShort(id: Int): String = ICON_SHORT[id] ?: "未知($id)"
    fun iconRotation(id: Int): Int = ICON_ROTATION[id] ?: 0
    fun laneBackIconLabel(id: Int): String = LANE_BACK_ICON_MAP[id] ?: "未知($id)"
    fun roadLabel(id: Int): String = ROAD_TYPE_MAP[id] ?: "未知($id)"
    fun cameraLabel(id: Int): String = CAMERA_TYPE_MAP[id] ?: "未知($id)"
    fun tmcLabel(id: Int): String = TMC_STATUS_MAP[id] ?: "未知($id)"
}

// ── 数据类 ──────────────────────────────────────────────

/**
 * 引导信息（KEY_TYPE=10001）
 * 这是高德导航车机版最核心的广播，包含当前道路、下一道路、剩余距离时间、当前速度、限速、
 * 电子眼、服务区、红绿灯等信息。
 */
data class GuideInfo(
    val type: Int = 0,
    val curRoadName: String = "",
    val nextRoadName: String = "",
    val nextNextRoadName: String = "",
    val icon: Int = -1,
    val nextNextTurnIcon: Int = -1,
    val routeRemainDis: Int = 0,
    val routeRemainTime: Int = 0,
    val routeAllDis: Int = 0,
    val routeAllTime: Int = 0,
    val segRemainDis: Int = 0,
    val segRemainTime: Int = 0,
    val nextSegRemainDis: Int = 0,
    val carLatitude: Double = 0.0,
    val carLongitude: Double = 0.0,
    val carDirection: Int = 0,
    val curSpeed: Int = 0,
    val limitedSpeed: Int = 0,
    val roadType: Int = -1,
    val cameraDist: Int = 0,
    val cameraType: Int = -1,
    val cameraSpeed: Int = 0,
    val sapaDist: Int = 0,
    val sapaName: String = "",
    val trafficLightNum: Int = 0,
    val roundAboutNum: Int = 0,
    val roundAllNum: Int = 0,
    val curSegNum: Int = 0,
    val curPointNum: Int = 0
)

/** 车道信息（KEY_TYPE=13012），JSON 结构解析 */
data class DriveWayInfo(
    val enabled: Boolean = false,
    val size: Int = 0,
    val lanes: List<LaneInfo> = emptyList()
)

data class LaneInfo(
    val number: Int = 0,
    val backIcon: Int = -1
)

/** 路况光柱图（KEY_TYPE=13011），JSON 结构解析 */
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
    val status: Int = -1,
    val distance: Int = 0,
    val percent: String = "0"
)

/** 定位信息（KEY_TYPE=10065），JSON 结构解析 */
data class LocationInfo(
    val bearing: Int = 0,
    val accuracy: Int = 0,
    val speed: Int = 0,
    val time: Long = 0L,
    val provider: String = ""
)

/** BLE 传输数据包（统一结构，便于 ESP32 侧解析） */
data class BleDataPacket(
    val type: Int,
    val ts: Long,
    val data: Map<String, Any>
)
