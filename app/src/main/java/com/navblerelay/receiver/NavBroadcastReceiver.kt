package com.navblerelay.receiver

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log
import com.navblerelay.protocol.*
import org.json.JSONObject

/**
 * 监听高德地图车机版发送的导航广播
 * Action: AUTONAVI_STANDARD_BROADCAST_SEND
 */
class NavBroadcastReceiver : BroadcastReceiver() {

    companion object {
        private const val TAG = "NavBroadcastReceiver"
    }

    var onGuideInfo: ((GuideInfo) -> Unit)? = null
    var onMapState: ((Int, String?) -> Unit)? = null
    var onDriveWay: ((DriveWayInfo) -> Unit)? = null
    var onTmcSegment: ((TmcSegmentInfo) -> Unit)? = null
    var onLocation: ((LocationInfo) -> Unit)? = null

    override fun onReceive(context: Context, intent: Intent) {
        val keyType = intent.getIntExtra("KEY_TYPE", -1)
        if (keyType == -1) return

        Log.d(TAG, "Received broadcast: KEY_TYPE=$keyType")

        when (keyType) {
            AmapAutoProtocol.KEY_GUIDE_INFO -> parseGuideInfo(intent)
            AmapAutoProtocol.KEY_MAP_STATE -> parseMapState(intent)
            AmapAutoProtocol.KEY_DRIVE_WAY -> parseDriveWay(intent)
            AmapAutoProtocol.KEY_TMC_SEGMENT -> parseTmcSegment(intent)
            AmapAutoProtocol.KEY_LOCATION -> parseLocation(intent)
            else -> Log.d(TAG, "Unhandled KEY_TYPE: $keyType")
        }
    }

    // ── 引导信息解析 ─────────────────────────────────────

    private fun parseGuideInfo(intent: Intent) {
        val info = GuideInfo(
            type = intent.getIntExtra("TYPE", 0),
            curRoadName = intent.getStringExtra("CUR_ROAD_NAME") ?: "",
            nextRoadName = intent.getStringExtra("NEXT_ROAD_NAME") ?: "",
            nextNextRoadName = intent.getStringExtra("NEXT_NEXT_ROAD_NAME") ?: "",
            icon = intent.getIntExtra("ICON", -1),
            nextNextTurnIcon = intent.getIntExtra("NEXT_NEXT_TURN_ICON", -1),
            routeRemainDis = intent.getIntExtra("ROUTE_REMAIN_DIS", 0),
            routeRemainTime = intent.getIntExtra("ROUTE_REMAIN_TIME", 0),
            routeAllDis = intent.getIntExtra("ROUTE_ALL_DIS", 0),
            routeAllTime = intent.getIntExtra("ROUTE_ALL_TIME", 0),
            segRemainDis = intent.getIntExtra("SEG_REMAIN_DIS", 0),
            segRemainTime = intent.getIntExtra("SEG_REMAIN_TIME", 0),
            nextSegRemainDis = intent.getIntExtra("NEXT_SEG_REMAIN_DIS", 0),
            carLatitude = intent.getDoubleExtra("CAR_LATITUDE", 0.0),
            carLongitude = intent.getDoubleExtra("CAR_LONGITUDE", 0.0),
            carDirection = intent.getIntExtra("CAR_DIRECTION", 0),
            curSpeed = intent.getIntExtra("CUR_SPEED", 0),
            limitedSpeed = intent.getIntExtra("LIMITED_SPEED", 0),
            roadType = intent.getIntExtra("ROAD_TYPE", -1),
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
        Log.d(TAG, "GuideInfo parsed: icon=${info.icon}, road=${info.curRoadName}")
        onGuideInfo?.invoke(info)
    }

    // ── 地图状态 ─────────────────────────────────────────

    private fun parseMapState(intent: Intent) {
        val state = intent.getIntExtra("EXTRA_STATE", -1)
        val crossMap = if (intent.hasExtra("EXTRA_CROSS_MAP")) {
            intent.getIntExtra("EXTRA_CROSS_MAP", 0).toString()
        } else null
        Log.d(TAG, "MapState: state=$state")
        onMapState?.invoke(state, crossMap)
    }

    // ── 车道信息 ─────────────────────────────────────────

    private fun parseDriveWay(intent: Intent) {
        val json = intent.getStringExtra("EXTRA_DRIVE_WAY") ?: return
        try {
            val root = JSONObject(json)
            val enabled = root.optBoolean("drive_way_enabled", false)
            val size = root.optInt("drive_way_size", 0)
            val lanes = mutableListOf<LaneInfo>()
            val infoArr = root.optJSONArray("drive_way_info")
            if (infoArr != null) {
                for (i in 0 until infoArr.length()) {
                    val item = infoArr.getJSONObject(i)
                    lanes.add(
                        LaneInfo(
                            number = item.optString("drive_way_number", "0").toIntOrNull() ?: 0,
                            backIcon = item.optString("drive_way_lane_Back_icon", "-1").toIntOrNull() ?: -1
                        )
                    )
                }
            }
            onDriveWay?.invoke(DriveWayInfo(enabled, size, lanes))
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse drive way", e)
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
                    segments.add(
                        TmcSegment(
                            number = item.optString("tmc_segment_number", "0").toIntOrNull() ?: 0,
                            status = item.optString("tmc_status", "-1").toIntOrNull() ?: -1,
                            distance = item.optString("tmc_segment_distance", "0").toIntOrNull() ?: 0,
                            percent = item.optString("tmc_segment_percent", "0")
                        )
                    )
                }
            }
            onTmcSegment?.invoke(
                TmcSegmentInfo(
                    enabled = root.optBoolean("tmc_segment_enabled", false),
                    size = root.optInt("tmc_segment_size", 0),
                    totalDistance = root.optInt("total_distance", 0),
                    residualDistance = root.optInt("residual_distance", 0),
                    finishDistance = root.optInt("finish_distance", 0),
                    segments = segments
                )
            )
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse TMC segment", e)
        }
    }

    // ── 定位信息 ─────────────────────────────────────────

    private fun parseLocation(intent: Intent) {
        val json = intent.getStringExtra("EXTRA_LOCATION_INFO") ?: return
        try {
            val root = JSONObject(json)
            onLocation?.invoke(
                LocationInfo(
                    bearing = root.optInt("bearing", 0),
                    accuracy = root.optInt("accuracy", 0),
                    speed = root.optInt("speed", 0),
                    time = root.optLong("time", 0L),
                    provider = root.optString("provider", "")
                )
            )
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse location", e)
        }
    }
}