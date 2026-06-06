package com.navblerelay

import android.Manifest
import android.content.*
import android.content.pm.PackageManager
import android.graphics.Color
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.util.Log
import android.view.View
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.navblerelay.protocol.NavDataHolder
import com.navblerelay.service.NavBleService

class MainActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "MainActivity"
    }

    // ── UI 组件 ──────────────────────────────────────────
    private lateinit var statusDot: View
    private lateinit var statusText: TextView
    private lateinit var bleStatus: TextView
    private lateinit var broadcastStatusRow: View
    private lateinit var broadcastStatus: TextView
    private lateinit var btnStart: Button
    private lateinit var btnStop: Button
    private lateinit var btnTestBroadcast: Button

    // 数据卡片
    private lateinit var mapState: TextView
    private lateinit var crossMap: TextView
    private lateinit var curRoad: TextView
    private lateinit var nextRoad: TextView
    private lateinit var routeRemain: TextView
    private lateinit var curSpeed: TextView
    private lateinit var limitedSpeed: TextView
    private lateinit var cameraDist: TextView
    private lateinit var sapaDist: TextView
    private lateinit var trafficLight: TextView
    private lateinit var driveWaySize: TextView
    private lateinit var driveWayDetail: TextView
    private lateinit var tmcTotal: TextView
    private lateinit var tmcRemain: TextView
    private lateinit var tmcSegments: TextView
    private lateinit var locSpeed: TextView
    private lateinit var locBearing: TextView
    private lateinit var locAccuracy: TextView

    private val handler = Handler(Looper.getMainLooper())
    private var checkRefreshRunnable: Runnable? = null

    // ── 权限请求 ─────────────────────────────────────────
    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { results ->
        val allGranted = results.values.all { it }
        if (allGranted) {
            Log.i(TAG, "所有权限已授予")
        } else {
            Log.w(TAG, "部分权限被拒绝")
            Toast.makeText(this, "部分权限被拒绝，部分功能可能无法使用", Toast.LENGTH_LONG).show()
        }
    }

    // ── 生命周期 ─────────────────────────────────────────

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        initViews()
        setupListeners()
        setupDataObserver()
        requestPermissionsIfNeeded()
    }

    override fun onResume() {
        super.onResume()
        updateServiceState()
        refreshUI()
    }

    // ── 初始化 ───────────────────────────────────────────

    private fun initViews() {
        statusDot = findViewById(R.id.status_dot)
        statusText = findViewById(R.id.status_text)
        bleStatus = findViewById(R.id.ble_status)
        broadcastStatusRow = findViewById(R.id.broadcast_status_row)
        broadcastStatus = findViewById(R.id.broadcast_status)
        btnStart = findViewById(R.id.btn_start)
        btnStop = findViewById(R.id.btn_stop)
        btnTestBroadcast = findViewById(R.id.btn_test_broadcast)

        mapState = findViewById(R.id.map_state)
        crossMap = findViewById(R.id.cross_map)
        curRoad = findViewById(R.id.cur_road)
        nextRoad = findViewById(R.id.next_road)
        routeRemain = findViewById(R.id.route_remain)
        curSpeed = findViewById(R.id.cur_speed)
        limitedSpeed = findViewById(R.id.limited_speed)
        cameraDist = findViewById(R.id.camera_dist)
        sapaDist = findViewById(R.id.sapa_dist)
        trafficLight = findViewById(R.id.traffic_light)
        driveWaySize = findViewById(R.id.drive_way_size)
        driveWayDetail = findViewById(R.id.drive_way_detail)
        tmcTotal = findViewById(R.id.tmc_total)
        tmcRemain = findViewById(R.id.tmc_remain)
        tmcSegments = findViewById(R.id.tmc_segments)
        locSpeed = findViewById(R.id.loc_speed)
        locBearing = findViewById(R.id.loc_bearing)
        locAccuracy = findViewById(R.id.loc_accuracy)
    }

    private fun setupListeners() {
        btnStart.setOnClickListener {
            if (!hasRequiredPermissions()) {
                requestPermissionsIfNeeded()
                Toast.makeText(this, "请先授予所需权限", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            try {
                NavBleService.start(this)
                setServiceRunning(true)
                Log.i(TAG, "Service started")
            } catch (e: Exception) {
                Log.e(TAG, "Failed to start service", e)
                Toast.makeText(this, "启动服务失败: ${e.message}", Toast.LENGTH_SHORT).show()
            }
        }

        btnStop.setOnClickListener {
            NavBleService.stop(this)
            setServiceRunning(false)
            Log.i(TAG, "Service stopped")
        }

        btnTestBroadcast.setOnClickListener {
            // 发送手动测试广播
            val intent = Intent("com.navblerelay.SELF_TEST")
            intent.setPackage(packageName)
            intent.putExtra("KEY_TYPE", 0)
            sendBroadcast(intent)
            Toast.makeText(this, "测试广播已发送，请查看 logcat 中 NavBR 标签", Toast.LENGTH_SHORT).show()
            Log.i(TAG, "手动测试广播已发送")
            // 等待 1 秒后刷新 UI 检查是否收到
            handler.postDelayed({
                if (NavDataHolder.broadcastReceived > 0) {
                    val ago = (System.currentTimeMillis() - NavDataHolder.broadcastReceived) / 1000
                    broadcastStatus.text = "测试接收成功! (${ago}秒前)"
                    broadcastStatus.setTextColor(Color.parseColor("#4CAF50"))
                } else {
                    broadcastStatus.text = "测试未收到，请检查 logcat"
                    broadcastStatus.setTextColor(Color.parseColor("#F44336"))
                }
            }, 1000)
        }
    }

    private fun setupDataObserver() {
        NavDataHolder.onDataChanged = {
            scheduleRefresh()
        }
    }

    private fun scheduleRefresh() {
        checkRefreshRunnable?.let { handler.removeCallbacks(it) }
        checkRefreshRunnable = Runnable {
            refreshUI()
        }
        handler.postDelayed(checkRefreshRunnable!!, 200)
    }

    // ── 权限 ─────────────────────────────────────────────

    private fun hasRequiredPermissions(): Boolean {
        val permissions = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            permissions.add(Manifest.permission.POST_NOTIFICATIONS)
        }
        return permissions.all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun requestPermissionsIfNeeded() {
        val permissions = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT)
                != PackageManager.PERMISSION_GRANTED
            ) {
                permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
            }
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS)
                != PackageManager.PERMISSION_GRANTED
            ) {
                permissions.add(Manifest.permission.POST_NOTIFICATIONS)
            }
        }
        if (permissions.isNotEmpty()) {
            permissionLauncher.launch(permissions.toTypedArray())
        }
    }

    // ── 服务状态 ─────────────────────────────────────────

    private fun updateServiceState() {
        val running = isServiceRunning()
        setServiceRunning(running)
    }

    private fun isServiceRunning(): Boolean {
        val manager = getSystemService(Context.ACTIVITY_SERVICE) as android.app.ActivityManager
        for (service in manager.getRunningServices(Int.MAX_VALUE)) {
            if (NavBleService::class.java.name == service.service.className) {
                return true
            }
        }
        return false
    }

    private fun setServiceRunning(running: Boolean) {
        if (running) {
            statusDot.setBackgroundResource(R.drawable.status_dot_yellow)
            statusText.text = "运行中"
            statusText.setTextColor(Color.parseColor("#FFC107"))
            btnStart.isEnabled = false
            btnStop.isEnabled = true
            btnTestBroadcast.visibility = View.VISIBLE
            broadcastStatusRow.visibility = View.VISIBLE
            broadcastStatus.text = "等待高德发送..."
            broadcastStatus.setTextColor(Color.parseColor("#FFC107"))
        } else {
            statusDot.setBackgroundResource(R.drawable.status_dot_red)
            statusText.text = "未启动"
            statusText.setTextColor(Color.parseColor("#757575"))
            btnStart.isEnabled = true
            btnStop.isEnabled = false
            btnTestBroadcast.visibility = View.GONE
            bleStatus.text = "未连接"
            bleStatus.setTextColor(Color.parseColor("#90A4AE"))
            broadcastStatusRow.visibility = View.GONE
            resetAllData()
        }
    }

    private fun resetAllData() {
        mapState.text = "-"
        crossMap.text = "-"
        curRoad.text = "-"
        nextRoad.text = "-"
        routeRemain.text = "-"
        curSpeed.text = "-"
        limitedSpeed.text = "-"
        cameraDist.text = "-"
        sapaDist.text = "-"
        trafficLight.text = "-"
        driveWaySize.text = "-"
        driveWayDetail.text = "-"
        tmcTotal.text = "-"
        tmcRemain.text = "-"
        tmcSegments.text = "-"
        locSpeed.text = "-"
        locBearing.text = "-"
        locAccuracy.text = "-"
    }

    // ── UI 刷新 ──────────────────────────────────────────

    private fun refreshUI() {
        // BLE 连接状态
        val isConnected = NavDataHolder.bleConnected
        if (isConnected) {
            statusDot.setBackgroundResource(R.drawable.status_dot_green)
            statusText.text = "已连接"
            statusText.setTextColor(Color.parseColor("#4CAF50"))
            bleStatus.text = "已连接 ${NavDataHolder.bleDeviceAddress ?: ""}"
            bleStatus.setTextColor(Color.parseColor("#4CAF50"))
        } else if (!btnStart.isEnabled) {
            statusDot.setBackgroundResource(R.drawable.status_dot_yellow)
            bleStatus.text = "等待连接"
            bleStatus.setTextColor(Color.parseColor("#FFC107"))
        } else {
            bleStatus.text = "未连接"
            bleStatus.setTextColor(Color.parseColor("#90A4AE"))
        }

        // 广播接收状态
        val lastBroadcast = NavDataHolder.broadcastReceived
        if (lastBroadcast > 0) {
            val secAgo = (System.currentTimeMillis() - lastBroadcast) / 1000
            val agoStr = if (secAgo < 60) "${secAgo}秒前" else "${secAgo / 60}分钟前"
            broadcastStatus.text = "已收到 ($agoStr)"
            broadcastStatus.setTextColor(Color.parseColor("#4CAF50"))
        }

        // 导航状态
        refreshMapState()
        refreshGuideInfo()
        refreshDriveWay()
        refreshTmc()
        refreshLocation()
    }

    private fun refreshMapState() {
        val state = NavDataHolder.mapState
        mapState.text = when (state) {
            -1 -> "-"
            0 -> "空闲"
            1 -> "🟢 导航中"
            2 -> "已到达"
            3 -> "暂停"
            else -> "状态: $state"
        }
        crossMap.text = NavDataHolder.crossMap ?: "-"
    }

    private fun refreshGuideInfo() {
        val info = NavDataHolder.guideInfo ?: return
        curRoad.text = info.curRoadName.ifEmpty { "-" }
        nextRoad.text = info.nextRoadName.ifEmpty { "-" }
        routeRemain.text = if (info.routeRemainDis > 0) {
            val km = info.routeRemainDis / 1000f
            "${"%.1f".format(km)} km / ${info.routeRemainTime / 60} 分钟"
        } else "-"
        curSpeed.text = if (info.curSpeed > 0) "${info.curSpeed} km/h" else "-"
        limitedSpeed.text = if (info.limitedSpeed > 0) "${info.limitedSpeed} km/h" else "-"
        cameraDist.text = if (info.cameraDist > 0) "${info.cameraDist} m" else "-"
        sapaDist.text = if (info.sapaDist > 0) "${info.sapaName} ${info.sapaDist}m" else "-"
        trafficLight.text = if (info.trafficLightNum > 0) "${info.trafficLightNum} 个" else "-"
    }

    private fun refreshDriveWay() {
        val info = NavDataHolder.driveWayInfo ?: return
        driveWaySize.text = if (info.enabled) "${info.size} 车道" else "-"
        if (info.enabled && info.lanes.isNotEmpty()) {
            driveWayDetail.text = info.lanes.joinToString(" ") {
                val icon = when (it.backIcon) {
                    0 -> "⬆"
                    1 -> "↖"
                    2 -> "↗"
                    3 -> "←"
                    4 -> "→"
                    5 -> "↙"
                    6 -> "↘"
                    7 -> "↩"
                    else -> "•"
                }
                icon
            }
        } else {
            driveWayDetail.text = "-"
        }
    }

    private fun refreshTmc() {
        val info = NavDataHolder.tmcSegmentInfo ?: return
        if (info.enabled) {
            tmcTotal.text = "${info.totalDistance / 1000f} km"
            tmcRemain.text = "${info.residualDistance / 1000f} km"
            tmcSegments.text = "${info.size} 段"
        } else {
            tmcTotal.text = "-"
            tmcRemain.text = "-"
            tmcSegments.text = "-"
        }
    }

    private fun refreshLocation() {
        val info = NavDataHolder.locationInfo ?: return
        locSpeed.text = if (info.speed > 0) "${info.speed} km/h" else "-"
        locBearing.text = if (info.bearing > 0) "${info.bearing}°" else "-"
        locAccuracy.text = if (info.accuracy > 0) "${info.accuracy} m" else "-"
    }
}