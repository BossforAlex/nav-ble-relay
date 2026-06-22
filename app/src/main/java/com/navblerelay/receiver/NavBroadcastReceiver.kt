package com.navblerelay.receiver

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log
import com.navblerelay.protocol.AmapAutoProtocol
import com.navblerelay.protocol.AmapAutoProtocol.iconLabel
import com.navblerelay.protocol.AmapAutoProtocol.laneBackIconLabel
import com.navblerelay.protocol.AmapAutoProtocol.roadLabel
import com.navblerelay.protocol.AmapAutoProtocol.cameraLabel
import com.navblerelay.protocol.AmapAutoProtocol.tmcLabel
import com.navblerelay.protocol.DriveWayInfo
import com.navblerelay.protocol.GuideInfo
import com.navblerelay.protocol.LaneInfo
import com.navblerelay.protocol.LocationInfo
import com.navblerelay.protocol.NavDataHolder
import com.navblerelay.protocol.TmcSegment
import com.navblerelay.protocol.TmcSegmentInfo
import org.json.JSONObject

/**
 * 监听高德地图车机版发送的导航广播
 *
 * 设计要点：
 *   1. 同时支持 ACTION_SEND 和 ACTION_RECV，兼容车机版公版 APP 以及手机版
 *   2. 以中文 log 输出关键字段（转向、道路类型、电子眼、路况等），便于调试
 *   3. 解析完成后通过回调把对象传递给 NavBleService，由其负责 BLE 转发
 *   4. 提供 SELF_TEST 自检测试广播，方便在无导航场景下验证链路正常
 */
class NavBroadcastReceiver : BroadcastReceiver() {

    companion object {
        private const val TAG = "NavBR"

        /** 自检测试 Action —— 用于验证接收器是否正常工作 */
        const val SELF_TEST_ACTION = "com.navblerelay.SELF_TEST"

        /** 所有监听的高德广播 Action（在 Manifest 与运行时同时注册） */
        val ALL_ACTIONS = arrayOf(
            // 车机版标准 Action
            "AUTONAVI_STANDARD_BROADCAST_SEND",
            "AUTONAVI_STANDARD_BROADCAST_RECV",
            // 可能的带包名前缀 Action
            "com.autonavi.amapauto.ACTION_STANDARD_BROADCAST_SEND",
            "com.autonavi.amapauto.ACTION_STANDARD_BROADCAST_RECV",
            "com.autonavi.amapauto.action.STANDARD_BROADCAST",
            "com.autonavi.action.STANDARD_BROADCAST_SEND",
            // 高德地图手机版（非车机版）可能的广播
            "com.autonavi.minimap.ACTION_BROADCAST",
            "com.autonavi.minimap.action.NAV_INFO",
            "com.autonavi.action.NAVIGATION_INFO",
            "AUTONAVI_NAVI_INFO",
            "AutonaviNaviInfo",
            "com.autonavi.autonavi.action.BROADCAST_SEND",
            // 自检
            SELF_TEST_ACTION
        )
    }

    var onGuideInfo: ((GuideInfo) -> Unit)? = null
    var onMapState: ((Int, String?) -> Unit)? = null
    var onDriveWay: ((DriveWayInfo) -> Unit)? = null
    var onTmcSegment: ((TmcSegmentInfo) -> Unit)? = null
    var onLocation: ((LocationInfo) -> Unit)? = null

    override fun onReceive(context: Context, intent: Intent) {
        val action = intent.action ?: "null"
        val pkg = intent.`package` ?: "-"

        // 兼容多种 KEY_TYPE 大小写
        var keyType = intent.getIntExtra("KEY_TYPE", -1)
        if (keyType == -1) keyType = intent.getIntExtra("key_type", -1)
        if (keyType == -1) keyType = intent.getIntExtra("EXTRA_KEY_TYPE", -1)

        NavDataHolder.broadcastReceived = System.currentTimeMillis()
        NavDataHolder.lastBroadcastAction = action

        if (keyType == -1) {
            if (action == SELF_TEST_ACTION) {
                Log.i(TAG, "✅ 收到自检广播（action=$action）")
            } else {
                val extras = intent.extras
                if (extras != null && !extras.isEmpty) {
                    val sb = StringBuilder("非导航广播 action=$action pkg=$pkg; extras: ")
                    for (key in extras.keySet()) {
                        sb.append("$key=${extras.get(key)}, ")
                    }
                    Log.d(TAG, sb.toString().trimEnd(',', ' '))
                }
            }
            return
        }

        when (keyType) {
            AmapAutoProtocol.KEY_GUIDE_INFO -> parseGuideInfo(intent)
            AmapAutoProtocol.KEY_MAP_STATE -> parseMapState(intent)
            AmapAutoProtocol.KEY_DRIVE_WAY -> parseDriveWay(intent)
            AmapAutoProtocol.KEY_TMC_SEGMENT -> parseTmcSegment(intent)
            AmapAutoProtocol.KEY_LOCATION -> parseLocation(intent)
            else -> Log.i(TAG, "忽略的 KEY_TYPE=$keyType（不支持的协议字段）")
        }
    }

    // ── 引导信息（最核心的广播，每秒更新一次）──────────
    private fun parseGuideInfo(intent: Intent) {
        val info = GuideInfo(
            type = intent.getIntExtra("TYPE", intent.getIntExtra("type", 0)),
            curRoadName = intent.getStringExtra("CUR_ROAD_NAME")
                ?: intent.getStringExtra("curRoadName") ?: "",
            nextRoadName = intent.getStringExtra("NEXT_ROAD_NAME")
                ?: intent.getStringExtra("nextRoadName") ?: "",
            nextNextRoadName = intent.getStringExtra("NEXT_NEXT_ROAD_NAME") ?: "",
            icon = intent.getIntExtra("ICON", intent.getIntExtra("icon", -1)),
            nextNextTurnIcon = intent.getIntExtra("NEXT_NEXT_TURN_ICON", -1),
            routeRemainDis = intent.getIntExtra("ROUTE_REMAIN_DIS",
                intent.getIntExtra("routeRemainDis", 0)),
            routeRemainTime = intent.getIntExtra("ROUTE_REMAIN_TIME",
                intent.getIntExtra("routeRemainTime", 0)),
            routeAllDis = intent.getIntExtra("ROUTE_ALL_DIS", 0),
            routeAllTime = intent.getIntExtra("ROUTE_ALL_TIME", 0),
            segRemainDis = intent.getIntExtra("SEG_REMAIN_DIS", 0),
            segRemainTime = intent.getIntExtra("SEG_REMAIN_TIME", 0),
            nextSegRemainDis = intent.getIntExtra("NEXT_SEG_REMAIN_DIS", 0),
            carLatitude = intent.getDoubleExtra("CAR_LATITUDE",
                intent.getDoubleExtra("carLatitude", 0.0)),
            carLongitude = intent.getDoubleExtra("CAR_LONGITUDE",
                intent.getDoubleExtra("carLongitude", 0.0)),
            carDirection = intent.getIntExtra("CAR_DIRECTION",
                intent.getIntExtra("carDirection", 0)),
            curSpeed = intent.getIntExtra("CUR_SPEED", intent.getIntExtra("curSpeed", 0)),
            limitedSpeed = intent.getIntExtra("LIMITED_SPEED",
                intent.getIntExtra("limitedSpeed", 0)),
            roadType = intent.getIntExtra("ROAD_TYPE", intent.getIntExtra("roadType", -1)),
            cameraDist = intent.getIntExtra("CAMERA_DIST", 0),
            cameraType = intent.getIntExtra("CAMERA_TYPE", -1),
            cameraSpeed = intent.getIntExtra("CAMERA_SPEED", 0),
            sapaDist = intent.getIntExtra("SAPA_DIST", 0),
            sapaName = intent.getStringExtra("SAPA_NAME") ?: "",
            trafficLightNum = intent.getIntExtra("TRAFFIC_LIGHT_NUM", 0),
            roundAboutNum = intent.getIntExtra("ROUND_ABOUT_NUM", 0),
            roundAllNum = intent.getIntExtra("ROUND_ALL_NUM", 0),
            curSegNum = intent.getIntExtra("CUR_SEG_NUM", 0),
            curPointNum = intent.getIntExtra("CUR_POINT_NUM", 0)
        )

        Log.i(TAG, buildString {
            append("[导航] 当前道路=\"${info.curRoadName}\" ")
            append("转向=${iconLabel(info.icon)}(${info.icon}) ")
            append("下一道路=\"${info.nextRoadName}\" ")
            append("剩余距离=${info.routeRemainDis}m ")
            append("剩余时间=${info.routeRemainTime}s ")
            append("当前车速=${info.curSpeed}km/h ")
            append("限速=${info.limitedSpeed}km/h ")
            append("道路类型=${roadLabel(info.roadType)} ")
            if (info.cameraDist > 0)
                append("前方${info.cameraDist}m=${cameraLabel(info.cameraType)}(${info.cameraSpeed}km/h) ")
            if (info.sapaDist > 0)
                append("前方${info.sapaDist}m=服务区\"${info.sapaName}\" ")
            if (info.trafficLightNum > 0) append("红绿灯=${info.trafficLightNum}个 ")
            append("车头方向=${info.carDirection}°")
        })

        onGuideInfo?.invoke(info) ?: run { NavDataHolder.guideInfo = info }
    }

    // ── 导航状态（开始 / 结束 / 到达） ──────────────────
    private fun parseMapState(intent: Intent) {
        val state = intent.getIntExtra("EXTRA_STATE",
            intent.getIntExtra("extraState", -1))
        val crossMap = if (intent.hasExtra("EXTRA_CROSS_MAP")) {
            intent.getIntExtra("EXTRA_CROSS_MAP", 0).toString()
        } else null

        val stateLabel = when (state) {
            AmapAutoProtocol.STATE_START_NAV -> "导航开始"
            AmapAutoProtocol.STATE_STOP_NAV -> "导航结束"
            AmapAutoProtocol.STATE_ARRIVE_DEST -> "到达目的地"
            -1 -> "空状态"
            else -> "状态($state)"
        }
        Log.i(TAG, "[状态] $stateLabel（路口放大图=$crossMap）")

        onMapState?.invoke(state, crossMap) ?: run {
            NavDataHolder.mapState = state
            NavDataHolder.crossMap = crossMap
        }
    }

    // ── 车道信息（临近路口时触发） ───────────────────────
    private fun parseDriveWay(intent: Intent) {
        val json = intent.getStringExtra("EXTRA_DRIVE_WAY") ?: return
        try {
            val root = JSONObject(json)
            val lanes = mutableListOf<LaneInfo>()
            root.optJSONArray("drive_way_info")?.let { arr ->
                for (i in 0 until arr.length()) {
                    val item = arr.getJSONObject(i)
                    lanes.add(LaneInfo(
                        number = item.optString("drive_way_number", "0").toIntOrNull() ?: 0,
                        backIcon = item.optString("drive_way_lane_Back_icon", "-1").toIntOrNull() ?: -1
                    ))
                }
            }
            val info = DriveWayInfo(
                enabled = root.optBoolean("drive_way_enabled", false),
                size = root.optInt("drive_way_size", 0),
                lanes = lanes
            )
            Log.i(TAG, "[车道] enabled=${info.enabled} size=${info.size} " +
                    "车道图标=${lanes.joinToString(",") { laneBackIconLabel(it.backIcon) }}")
            onDriveWay?.invoke(info) ?: run { NavDataHolder.driveWayInfo = info }
        } catch (e: Exception) {
            Log.e(TAG, "车道信息解析失败", e)
        }
    }

    // ── 路况光柱图 ──────────────────────────────────────
    private fun parseTmcSegment(intent: Intent) {
        val json = intent.getStringExtra("EXTRA_TMC_SEGMENT") ?: return
        try {
            val root = JSONObject(json)
            val segments = mutableListOf<TmcSegment>()
            root.optJSONArray("tmc_info")?.let { arr ->
                for (i in 0 until arr.length()) {
                    val item = arr.getJSONObject(i)
                    segments.add(TmcSegment(
                        number = item.optString("tmc_segment_number", "0").toIntOrNull() ?: 0,
                        status = item.optString("tmc_status", "-1").toIntOrNull() ?: -1,
                        distance = item.optString("tmc_segment_distance", "0").toIntOrNull() ?: 0,
                        percent = item.optString("tmc_segment_percent", "0")
                    ))
                }
            }
            val info = TmcSegmentInfo(
                enabled = root.optBoolean("tmc_segment_enabled", false),
                size = root.optInt("tmc_segment_size", 0),
                totalDistance = root.optInt("total_distance", 0),
                residualDistance = root.optInt("residual_distance", 0),
                finishDistance = root.optInt("finish_distance", 0),
                segments = segments
            )
            Log.i(TAG, "[路况] 总距离=${info.totalDistance}m " +
                    "剩余=${info.residualDistance}m " +
                    "分段状态=${segments.joinToString(",") { tmcLabel(it.status) }}")
            onTmcSegment?.invoke(info) ?: run { NavDataHolder.tmcSegmentInfo = info }
        } catch (e: Exception) {
            Log.e(TAG, "路况解析失败", e)
        }
    }

    // ── 定位信息 ────────────────────────────────────────
    private fun parseLocation(intent: Intent) {
        val json = intent.getStringExtra("EXTRA_LOCATION_INFO") ?: return
        try {
            val root = JSONObject(json)
            val info = LocationInfo(
                bearing = root.optInt("bearing", 0),
                accuracy = root.optInt("accuracy", 0),
                speed = root.optInt("speed", 0),
                time = root.optLong("time", 0L),
                provider = root.optString("provider", "")
            )
            Log.i(TAG, "[定位] 方向=${info.bearing}° 精度=${info.accuracy}m " +
                    "速度=${info.speed}km/h provider=${info.provider}")
            onLocation?.invoke(info) ?: run { NavDataHolder.locationInfo = info }
        } catch (e: Exception) {
            Log.e(TAG, "定位信息解析失败", e)
        }
    }
}
