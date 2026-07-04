package com.navblerelay

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.util.Log

/**
 * 接收高德地图车机版发送的导航广播
 *
 * 设计要点：
 *   1. 同时支持 AUTONAVI_STANDARD_BROADCAST_SEND 等多个 action，
 *      兼容车机版公版 APP 以及手机版
 *   2. 解析 KEY_TYPE，从 Bundle 提取数据并组装为 Map
 *   3. 通过 [onParsed] 回调把结果传递给 MainActivity，
 *      由其通过 platform channel 发送到 Flutter
 *   4. SELF_TEST 自检广播用于验证接收器是否正常工作
 */
class NavBroadcastReceiver : BroadcastReceiver() {

    companion object {
        private const val TAG = "NavBR"

        /** 自检测试 Action */
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

        // ── KEY_TYPE ─────────────────────────────────────
        const val KEY_GUIDE_INFO = 10001
        const val KEY_MAP_STATE = 10019
        const val KEY_ROUTE_INFO = 10056
        const val KEY_LOCATION = 10065
        const val KEY_TMC_SEGMENT = 13011
        const val KEY_DRIVE_WAY = 13012
    }

    /** 解析结果回调：method 为 Flutter 端方法名，args 为参数 Map */
    var onParsed: ((method: String, args: Map<String, Any?>) -> Unit)? = null

    /** 最近一次收到广播的时间戳，供自检使用 */
    var lastReceivedAt: Long = 0L
        private set

    override fun onReceive(context: Context, intent: Intent) {
        val action = intent.action ?: "null"
        val extras = intent.extras

        lastReceivedAt = System.currentTimeMillis()

        // 兼容多种 KEY_TYPE 大小写
        var keyType = intent.getIntExtra("KEY_TYPE", -1)
        if (keyType == -1) keyType = intent.getIntExtra("key_type", -1)
        if (keyType == -1) keyType = intent.getIntExtra("EXTRA_KEY_TYPE", -1)

        if (keyType == -1) {
            if (action == SELF_TEST_ACTION) {
                Log.i(TAG, "收到自检广播（action=$action）")
            } else if (extras != null && !extras.isEmpty) {
                val sb = StringBuilder("非导航广播 action=$action; extras: ")
                for (key in extras.keySet()) {
                    sb.append("$key=${extras.get(key)}, ")
                }
                Log.d(TAG, sb.toString().trimEnd(',', ' '))
            }
            onParsed?.invoke("onAction", mapOf("action" to action))
            return
        }

        when (keyType) {
            KEY_GUIDE_INFO -> parseGuideInfo(intent, action)
            KEY_MAP_STATE -> parseMapState(intent, action)
            KEY_DRIVE_WAY -> parseDriveWay(intent, action)
            KEY_TMC_SEGMENT -> parseTmcSegment(intent, action)
            KEY_LOCATION -> parseLocation(intent, action)
            else -> Log.i(TAG, "忽略的 KEY_TYPE=$keyType（不支持的协议字段）")
        }
    }

    // ── 引导信息（最核心的广播，每秒更新一次）──────────
    private fun parseGuideInfo(intent: Intent, action: String) {
        val data = mutableMapOf<String, Any?>(
            "TYPE" to intent.getIntExtra("TYPE", intent.getIntExtra("type", 0)),
            "CUR_ROAD_NAME" to (intent.getStringExtra("CUR_ROAD_NAME")
                ?: intent.getStringExtra("curRoadName") ?: ""),
            "NEXT_ROAD_NAME" to (intent.getStringExtra("NEXT_ROAD_NAME")
                ?: intent.getStringExtra("nextRoadName") ?: ""),
            "NEXT_NEXT_ROAD_NAME" to (intent.getStringExtra("NEXT_NEXT_ROAD_NAME") ?: ""),
            "ICON" to intent.getIntExtra("ICON", intent.getIntExtra("icon", -1)),
            "NEXT_NEXT_TURN_ICON" to intent.getIntExtra("NEXT_NEXT_TURN_ICON", -1),
            "ROUTE_REMAIN_DIS" to intent.getIntExtra("ROUTE_REMAIN_DIS",
                intent.getIntExtra("routeRemainDis", 0)),
            "ROUTE_REMAIN_TIME" to intent.getIntExtra("ROUTE_REMAIN_TIME",
                intent.getIntExtra("routeRemainTime", 0)),
            "ROUTE_ALL_DIS" to intent.getIntExtra("ROUTE_ALL_DIS", 0),
            "ROUTE_ALL_TIME" to intent.getIntExtra("ROUTE_ALL_TIME", 0),
            "SEG_REMAIN_DIS" to intent.getIntExtra("SEG_REMAIN_DIS", 0),
            "SEG_REMAIN_TIME" to intent.getIntExtra("SEG_REMAIN_TIME", 0),
            "NEXT_SEG_REMAIN_DIS" to intent.getIntExtra("NEXT_SEG_REMAIN_DIS", 0),
            "CAR_LATITUDE" to intent.getDoubleExtra("CAR_LATITUDE",
                intent.getDoubleExtra("carLatitude", 0.0)),
            "CAR_LONGITUDE" to intent.getDoubleExtra("CAR_LONGITUDE",
                intent.getDoubleExtra("carLongitude", 0.0)),
            "CAR_DIRECTION" to intent.getIntExtra("CAR_DIRECTION",
                intent.getIntExtra("carDirection", 0)),
            "CUR_SPEED" to intent.getIntExtra("CUR_SPEED", intent.getIntExtra("curSpeed", 0)),
            "LIMITED_SPEED" to intent.getIntExtra("LIMITED_SPEED",
                intent.getIntExtra("limitedSpeed", 0)),
            "ROAD_TYPE" to intent.getIntExtra("ROAD_TYPE", intent.getIntExtra("roadType", -1)),
            "CAMERA_DIST" to intent.getIntExtra("CAMERA_DIST", 0),
            "CAMERA_TYPE" to intent.getIntExtra("CAMERA_TYPE", -1),
            "CAMERA_SPEED" to intent.getIntExtra("CAMERA_SPEED", 0),
            "SAPA_DIST" to intent.getIntExtra("SAPA_DIST", 0),
            "SAPA_NAME" to (intent.getStringExtra("SAPA_NAME") ?: ""),
            "TRAFFIC_LIGHT_NUM" to intent.getIntExtra("TRAFFIC_LIGHT_NUM", 0),
            "ROUND_ABOUT_NUM" to intent.getIntExtra("ROUND_ABOUT_NUM", 0),
            "ROUND_ALL_NUM" to intent.getIntExtra("ROUND_ALL_NUM", 0),
            "CUR_SEG_NUM" to intent.getIntExtra("CUR_SEG_NUM", 0),
            "CUR_POINT_NUM" to intent.getIntExtra("CUR_POINT_NUM", 0)
        )
        Log.i(TAG, "[导航] 当前道路=${data["CUR_ROAD_NAME"]} 转向ICON=${data["ICON"]} " +
                "剩余=${data["SEG_REMAIN_DIS"]}m 车速=${data["CUR_SPEED"]}km/h")
        onParsed?.invoke("onGuideInfo", data)
    }

    // ── 导航状态（开始 / 结束 / 到达） ──────────────────
    private fun parseMapState(intent: Intent, action: String) {
        val state = intent.getIntExtra("EXTRA_STATE", intent.getIntExtra("extraState", -1))
        val crossMap: String? = if (intent.hasExtra("EXTRA_CROSS_MAP")) {
            intent.getIntExtra("EXTRA_CROSS_MAP", 0).toString()
        } else null
        Log.i(TAG, "[状态] state=$state 路口放大图=$crossMap")
        onParsed?.invoke("onMapState", mapOf(
            "EXTRA_STATE" to state,
            "EXTRA_CROSS_MAP" to crossMap
        ))
    }

    // ── 车道信息（临近路口时触发） ───────────────────────
    private fun parseDriveWay(intent: Intent, action: String) {
        // 原始 JSON 字符串透传给 Flutter 解析（Flutter 端 JSON 解析更便利）
        val json = intent.getStringExtra("EXTRA_DRIVE_WAY") ?: return
        onParsed?.invoke("onDriveWay", mapOf("__json__" to json))
    }

    // ── 路况光柱图 ──────────────────────────────────────
    private fun parseTmcSegment(intent: Intent, action: String) {
        val json = intent.getStringExtra("EXTRA_TMC_SEGMENT") ?: return
        onParsed?.invoke("onTmcSegment", mapOf("__json__" to json))
    }

    // ── 定位信息 ────────────────────────────────────────
    private fun parseLocation(intent: Intent, action: String) {
        val json = intent.getStringExtra("EXTRA_LOCATION_INFO") ?: return
        onParsed?.invoke("onLocation", mapOf("__json__" to json))
    }
}
