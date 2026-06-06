package com.navblerelay

import android.Manifest
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.navblerelay.protocol.AmapAutoProtocol
import com.navblerelay.protocol.DriveWayInfo
import com.navblerelay.protocol.GuideInfo
import com.navblerelay.protocol.LocationInfo
import com.navblerelay.protocol.NavDataHolder
import com.navblerelay.protocol.TmcSegmentInfo
import com.navblerelay.service.NavBleService

class MainActivity : AppCompatActivity() {

    private lateinit var statusDot: View
    private lateinit var statusText: TextView
    private lateinit var bleStatus: TextView
    private lateinit var btnStart: Button
    private lateinit var btnStop: Button

    private lateinit var tvMapState: TextView
    private lateinit var tvRoad: TextView
    private lateinit var tvTurn: TextView
    private lateinit var tvDistance: TextView
    private lateinit var tvTime: TextView
    private lateinit var tvSpeed: TextView
    private lateinit var tvSpeedLimit: TextView
    private lateinit var tvRoadType: TextView
    private lateinit var tvCamera: TextView
    private lateinit var tvTrafficLight: TextView
    private lateinit var tvSapa: TextView
    private lateinit var tvLane: TextView
    private lateinit var tvTmc: TextView
    private lateinit var tmcBar: LinearLayout
    private lateinit var tvBearing: TextView
    private lateinit var tvAccuracy: TextView
    private lateinit var tvProvider: TextView

    private val handler = Handler(Looper.getMainLooper())
    private val refreshRunnable = object : Runnable {
        override fun run() {
            refreshUI()
            handler.postDelayed(this, 1000)
        }
    }

    private val requiredPermissions = buildList {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            add(Manifest.permission.BLUETOOTH_CONNECT)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            add(Manifest.permission.POST_NOTIFICATIONS)
        }
    }

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { grants ->
        val allGranted = grants.values.all { it }
        if (allGranted) {
            startService()
        } else {
            Toast.makeText(this, "需要蓝牙和通知权限才能启动服务", Toast.LENGTH_LONG).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        statusDot = findViewById(R.id.status_dot)
        statusText = findViewById(R.id.status_text)
        bleStatus = findViewById(R.id.ble_status)
        btnStart = findViewById(R.id.btn_start)
        btnStop = findViewById(R.id.btn_stop)

        tvMapState = findViewById(R.id.tv_map_state)
        tvRoad = findViewById(R.id.tv_road)
        tvTurn = findViewById(R.id.tv_turn)
        tvDistance = findViewById(R.id.tv_distance)
        tvTime = findViewById(R.id.tv_time)
        tvSpeed = findViewById(R.id.tv_speed)
        tvSpeedLimit = findViewById(R.id.tv_speed_limit)
        tvRoadType = findViewById(R.id.tv_road_type)
        tvCamera = findViewById(R.id.tv_camera)
        tvTrafficLight = findViewById(R.id.tv_traffic_light)
        tvSapa = findViewById(R.id.tv_sapa)
        tvLane = findViewById(R.id.tv_lane)
        tvTmc = findViewById(R.id.tv_tmc)
        tmcBar = findViewById(R.id.tmc_bar)
        tvBearing = findViewById(R.id.tv_bearing)
        tvAccuracy = findViewById(R.id.tv_accuracy)
        tvProvider = findViewById(R.id.tv_provider)

        btnStart.setOnClickListener {
            Toast.makeText(this, "正在启动服务...", Toast.LENGTH_SHORT).show()
            if (hasAllPermissions()) {
                startService()
            } else {
                permissionLauncher.launch(requiredPermissions.toTypedArray())
            }
        }

        btnStop.setOnClickListener {
            Toast.makeText(this, "正在停止服务...", Toast.LENGTH_SHORT).show()
            NavBleService.stop(this)
            setServiceRunning(false)
        }
    }

    override fun onResume() {
        super.onResume()
        handler.post(refreshRunnable)
    }

    override fun onPause() {
        super.onPause()
        handler.removeCallbacks(refreshRunnable)
    }

    private fun hasAllPermissions(): Boolean {
        return requiredPermissions.all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun startService() {
        try {
            NavBleService.start(this)
            setServiceRunning(true)
        } catch (e: Exception) {
            Toast.makeText(this, "启动失败: ${e.message}", Toast.LENGTH_LONG).show()
        }
    }

    private fun setServiceRunning(running: Boolean) {
        if (running) {
            statusDot.setBackgroundResource(R.drawable.status_dot_green)
            statusText.text = "服务运行中"
            btnStart.isEnabled = false
            btnStop.isEnabled = true
        } else {
            statusDot.setBackgroundResource(R.drawable.status_dot_red)
            statusText.text = "服务未启动"
            btnStart.isEnabled = true
            btnStop.isEnabled = false
            resetAllData()
        }
    }

    private fun refreshUI() {
        // BLE 连接状态
        if (NavDataHolder.bleConnected) {
            bleStatus.text = "BLE: 已连接"
            bleStatus.setTextColor(Color.parseColor("#4CAF50"))
        } else {
            bleStatus.text = "BLE: 未连接"
            bleStatus.setTextColor(Color.parseColor("#90A4AE"))
        }

        // 导航状态
        refreshMapState()

        // 引导信息
        NavDataHolder.guideInfo?.let { refreshGuideInfo(it) }

        // 车道信息
        NavDataHolder.driveWayInfo?.let { refreshDriveWay(it) }

        // 路况光柱
        NavDataHolder.tmcSegmentInfo?.let { refreshTmc(it) }

        // 定位信息
        NavDataHolder.locationInfo?.let { refreshLocation(it) }
    }

    private fun refreshMapState() {
        val state = NavDataHolder.mapState
        tvMapState.text = when (state) {
            AmapAutoProtocol.STATE_START_NAV -> "🟢 导航中"
            AmapAutoProtocol.STATE_STOP_NAV -> "🔴 导航已结束"
            AmapAutoProtocol.STATE_ARRIVE_DEST -> "🏁 已到达目的地"
            else -> "等待导航数据..."
        }
    }

    private fun refreshGuideInfo(info: GuideInfo) {
        // 道路名称
        val road = if (info.curRoadName.isNotEmpty()) info.curRoadName else "--"
        val nextRoad = if (info.nextRoadName.isNotEmpty()) " → ${info.nextRoadName}" else ""
        tvRoad.text = "当前道路: $road$nextRoad"

        // 转向图标
        val iconName = AmapAutoProtocol.ICON_MAP[info.icon] ?: "--"
        tvTurn.text = "转向: $iconName"

        // 距离
        if (info.routeRemainDis > 0) {
            tvDistance.text = if (info.routeRemainDis >= 1000)
                "剩余: ${info.routeRemainDis / 1000}.${(info.routeRemainDis % 1000) / 100} km"
            else
                "剩余: ${info.routeRemainDis} m"
        }

        // 时间
        if (info.routeRemainTime > 0) {
            val min = info.routeRemainTime / 60
            tvTime.text = "预计: ${min} 分钟"
        }

        // 速度
        if (info.curSpeed > 0) {
            tvSpeed.text = "车速: ${info.curSpeed} km/h"
        }

        // 限速
        if (info.limitedSpeed > 0) {
            tvSpeedLimit.text = "限速: ${info.limitedSpeed} km/h"
        }

        // 道路类型
        if (info.roadType >= 0) {
            tvRoadType.text = "道路类型: ${AmapAutoProtocol.ROAD_TYPE_MAP[info.roadType] ?: "未知"}"
        }

        // 电子眼
        if (info.cameraDist > 0 && info.cameraType >= 0) {
            val cameraType = AmapAutoProtocol.CAMERA_TYPE_MAP[info.cameraType] ?: "电子眼"
            val dist = if (info.cameraDist >= 1000)
                "${info.cameraDist / 1000}.${(info.cameraDist % 1000) / 100} km"
            else
                "${info.cameraDist} m"
            var camText = "$cameraType 前方 $dist"
            if (info.cameraSpeed > 0) camText += " 限速${info.cameraSpeed}km/h"
            tvCamera.text = "电子眼: $camText"
        }

        // 红绿灯
        if (info.trafficLightNum > 0) {
            tvTrafficLight.text = "红绿灯: 前方 ${info.trafficLightNum} 个"
        }

        // 服务区
        if (info.sapaDist > 0) {
            val dist = if (info.sapaDist >= 1000)
                "${info.sapaDist / 1000}.${(info.sapaDist % 1000) / 100} km"
            else
                "${info.sapaDist} m"
            val name = if (info.sapaName.isNotEmpty()) " (${info.sapaName})" else ""
            tvSapa.text = "服务区: 前方 $dist$name"
        }
    }

    private fun refreshDriveWay(info: DriveWayInfo) {
        if (!info.enabled || info.lanes.isEmpty()) {
            tvLane.text = "等待数据..."
            return
        }
        val sb = StringBuilder("车道数: ${info.size}\n")
        info.lanes.forEach { lane ->
            val iconName = AmapAutoProtocol.ICON_MAP[lane.backIcon] ?: "?"
            sb.append("  车道${lane.number}: $iconName\n")
        }
        tvLane.text = sb.toString().trimEnd()
    }

    private fun refreshTmc(info: TmcSegmentInfo) {
        if (!info.enabled || info.segments.isEmpty()) {
            tvTmc.text = "等待数据..."
            tmcBar.removeAllViews()
            return
        }

        val total = info.totalDistance
        val statusNames = mapOf(-1 to "无数据", 0 to "未知", 1 to "畅通", 2 to "缓行", 3 to "拥堵", 4 to "严重拥堵")
        val statusColors = mapOf(-1 to "#BDBDBD", 0 to "#BDBDBD", 1 to "#4CAF50", 2 to "#FFC107", 3 to "#FF9800", 4 to "#F44336")

        tvTmc.text = "总距离: ${formatDist(total)} | 剩余: ${formatDist(info.residualDistance)}"

        tmcBar.removeAllViews()
        if (total <= 0) return

        info.segments.forEach { seg ->
            val pct = seg.percent.toFloatOrNull() ?: 0f
            val view = View(this)
            val params = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, pct)
            view.setBackgroundColor(Color.parseColor(statusColors[seg.status] ?: "#BDBDBD"))
            view.contentDescription = statusNames[seg.status] ?: "?"
            tmcBar.addView(view, params)
        }
    }

    private fun refreshLocation(info: LocationInfo) {
        tvBearing.text = "方位角: ${info.bearing}°"
        tvAccuracy.text = "精度: ${info.accuracy}m"
        tvProvider.text = "定位来源: ${info.provider}"
    }

    private fun resetAllData() {
        tvMapState.text = "等待导航数据..."
        tvRoad.text = "当前道路: --"
        tvTurn.text = "转向: --"
        tvDistance.text = "剩余: --"
        tvTime.text = "预计: --"
        tvSpeed.text = "车速: --"
        tvSpeedLimit.text = "限速: --"
        tvRoadType.text = "道路类型: --"
        tvCamera.text = "电子眼: --"
        tvTrafficLight.text = "红绿灯: --"
        tvSapa.text = "服务区: --"
        tvLane.text = "等待数据..."
        tvTmc.text = "等待数据..."
        tmcBar.removeAllViews()
        tvBearing.text = "方位角: --"
        tvAccuracy.text = "精度: --"
        tvProvider.text = "定位来源: --"
    }

    private fun formatDist(meters: Int): String {
        return if (meters >= 1000) "${meters / 1000}.${(meters % 1000) / 100} km" else "${meters} m"
    }
}