package com.navblerelay.receiver

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log
import com.navblerelay.protocol.*
import org.json.JSONObject

/**
 * 监听高德地图车机版发送的导航广播
 *
 * 支持两种注册方式：
 * 1. 代码动态注册（通过回调传递给 Service）
 * 2. Manifest 静态注册（直接写入 NavDataHolder 单例）
 *
 * 支持多种广播 Action，兼容不同版本的高德地图。
 */
class NavBroadcastReceiver : BroadcastReceiver() {

    companion object {
        private const val TAG = "NavBR"

        /** 已知的高德广播 Action（含车机版和可能存在的手机版 Action） */
        val ALL_ACTIONS = arrayOf(
            // ── 车机版标准 Action ──
            "AUTONAVI_STANDARD_BROADCAST_SEND",
            "AUTONAVI_STANDARD_BROADCAST_RECV",
            // ── 可能存在的包名前缀 Action ──
            "com.autonavi.amapauto.ACTION_STANDARD_BROADCAST_SEND",
            "com.autonavi.amapauto.ACTION_STANDARD_BROADCAST_RECV",
            "com.autonavi.amapauto.action.STANDARD_BROADCAST",
            "com.autonavi.action.STANDARD_BROADCAST_SEND",
            // ── 高德地图手机版可能的广播 Action ──
            "com.autonavi.minimap.ACTION_BROADCAST",
            "com.autonavi.minimap.action.NAV_INFO",
            "com.autonavi.action.NAVIGATION_INFO",
            // ── 通用高德广播 ──
            "AUTONAVI_NAVI_INFO",
            "AutonaviNaviInfo",
            "com.autonavi.autonavi.action.BROADCAST_SEND"
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

        // 尝试多种方式获取 KEY_TYPE
        var keyType = intent.getIntExtra("KEY_TYPE", -1)
        if (keyType == -1) keyType = intent.getIntExtra("key_type", -1)
        if (keyType == -1) keyType = intent.getIntExtra("EXTRA_KEY_TYPE", -1)

        Log.i(TAG, "📡 收到广播: action=$action pkg=$pkg KEY_TYPE=$keyType")

        // 记录接收时间
        NavDataHolder.broadcastReceived = System.currentTimeMillis()
        NavDataHolder.lastBroadcastAction = action

        if (keyType == -1) {
            // 没有 KEY_TYPE，打印所有 extras 用于诊断
            val extras = intent.extras
            if (extras != null && !extras.isEmpty) {
                val sb = StringBuilder("extras: ")
                for (key in extras.keySet()) {
                    sb.append("$key=${extras.get(key)}, ")
                }
                Log.i(TAG, sb.toString().trimEnd(',', ' '))
            } else {
                Log.d(TAG, "无 extras，可能是空广播")
            }
            return
        }

        when (keyType) {
            AmapAutoProtocol.KEY_GUIDE_INFO -> parseGuideInfo(intent)
            AmapAutoProtocol.KEY_MAP_STATE -> parseMapState(intent)
            AmapAutoProtocol.KEY_DRIVE_WAY -> parseDriveWay(intent)
            AmapAutoProtocol.KEY_TMC_SEGMENT -> parseTmcSegment(intent)
            AmapAutoProtocol.KEY_LOCATION -> parseLocation(intent)
            else -> Log.i(TAG, "未处理的 KEY_TYPE=$keyType (可作为扩展)")
        }
    }

    // ── 引导信息解析 ─────────────────────────────────────

    private fun parseGuideInfo(intent: Intent) {
        val info = GuideInfo(
            type = intent.getIntExtra("TYPE", intent.getIntExtra("type", 0)),
            curRoadName = intent.getStringExtra("CUR_ROAD_NAME") ?: intent.getStringExtra("curRoadName") ?: "",
            nextRoadName = intent.getStringExtra("NEXT_ROAD_NAME") ?: intent.getStringExtra("nextRoadName") ?: "",
            nextNextRoadName = intent.getStringExtra("NEXT_NEXT_ROAD_NAME") ?: "",
            icon = intent.getIntExtra("ICON", intent.getIntExtra("icon", -1)),
            nextNextTurnIcon = intent.getIntExtra("NEXT_NEXT_TURN_ICON", -1),
            routeRemainDis = intent.getIntExtra("ROUTE_REMAIN_DIS", intent.getIntExtra("routeRemainDis", 0)),
            routeRemainTime = intent.getIntExtra("ROUTE_REMAIN_TIME", intent.getIntExtra("routeRemainTime", 0)),
            routeAllDis = intent.getIntExtra("ROUTE_ALL_DIS", 0),
            routeAllTime = intent.getIntExtra("ROUTE_ALL_TIME", 0),
            segRemainDis = intent.getIntExtra("SEG_REMAIN_DIS", 0),
            segRemainTime = intent.getIntExtra("SEG_REMAIN_TIME", 0),
            nextSegRemainDis = intent.getIntExtra("NEXT_SEG_REMAIN_DIS", 0),
            carLatitude = intent.getDoubleExtra("CAR_LATITUDE", intent.getDoubleExtra("carLatitude", 0.0)),
            carLongitude = intent.getDoubleExtra("CAR_LONGITUDE", intent.getDoubleExtra("carLongitude", 0.0)),
            carDirection = intent.getIntExtra("CAR_DIRECTION", intent.getIntExtra("carDirection", 0)),
            curSpeed = intent.getIntExtra("CUR_SPEED", intent.getIntExtra("curSpeed", 0)),
            limitedSpeed = intent.getIntExtra("LIMITED_SPEED", intent.getIntExtra("limitedSpeed", 0)),
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
        Log.i(TAG, "GuideInfo: icon=${info.icon} road=${info.curRoadName} speed=${info.curSpeed}")
        if (onGuideInfo != null) onGuideInfo?.invoke(info)
        else NavDataHolder.guideInfo = info
    }

    // ── 地图状态 ─────────────────────────────────────────

    private fun parseMapState(intent: Intent) {
        val state = intent.getIntExtra("EXTRA_STATE", intent.getIntExtra("extraState", -1))
        val crossMap = if (intent.hasExtra("EXTRA_CROSS_MAP")) {
            intent.getIntExtra("EXTRA_CROSS_MAP", 0).toString()
        } else null
        Log.i(TAG, "MapState: state=$state")
        if (onMapState != null) onMapState?.invoke(state, crossMap)
        else { NavDataHolder.mapState = state; NavDataHolder.crossMap = crossMap }
    }

    // ── 车道信息 ─────────────────────────────────────────

    private fun parseDriveWay(intent: Intent) {
        val json = intent.getStringExtra("EXTRA_DRIVE_WAY") ?: run {
            Log.w(TAG, "DriveWay: 无 EXTRA_DRIVE_WAY")
            return
        }
        try {
            val root = JSONObject(json)
            val enabled = root.optBoolean("drive_way_enabled", false)
            val size = root.optInt("drive_way_size", 0)
            val lanes = mutableListOf<LaneInfo>()
            val infoArr = root.optJSONArray("drive_way_info")
            if (infoArr != null) {
                for (i in 0 until infoArr.length()) {
                    val item = infoArr.getJSONObject(i)
                    lanes.add(LaneInfo(
                        number = item.optString("drive_way_number", "0").toIntOrNull() ?: 0,
                        backIcon = item.optString("drive_way_lane_Back_icon", "-1").toIntOrNull() ?: -1
                    ))
                }
            }
            val info = DriveWayInfo(enabled, size, lanes)
            if (onDriveWay != null) onDriveWay?.invoke(info)
            else NavDataHolder.driveWayInfo = info
        } catch (e: Exception) {
            Log.e(TAG, "DriveWay parse error", e)
        }
    }

    // ── 路况光柱图 ───────────────────────────────────────

    private fun parseTmcSegment(intent: Intent) {
        val json = intent.getStringExtra("EXTRA_TMC_SEGMENT") ?: return
        try {
            val root = JSONObject(json)
            val segments = mutableListOf<TmcSegment>()
            val infoArr = root.optJSONArray("tmc_info")
            if (infoArr != null) {
                for (i in 0 until infoArr.length()) {
                    val item = infoArr.getJSONObject(i)
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
            if (onTmcSegment != null) onTmcSegment?.invoke(info)
            else NavDataHolder.tmcSegmentInfo = info
        } catch (e: Exception) {
            Log.e(TAG, "TMC parse error", e)
        }
    }

    // ── 定位信息 ─────────────────────────────────────────

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
            if (onLocation != null) onLocation?.invoke(info)
            else NavDataHolder.locationInfo = info
        } catch (e: Exception) {
            Log.e(TAG, "Location parse error", e)
        }
    }
}