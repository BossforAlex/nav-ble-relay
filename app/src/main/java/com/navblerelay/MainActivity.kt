package com.navblerelay

import android.Manifest
import android.content.*
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.View
import android.widget.Button
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import com.navblerelay.ui.LaneGuideView
import com.navblerelay.ui.TrafficBarView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.navblerelay.protocol.AmapAutoProtocol
import com.navblerelay.protocol.NavDataHolder
import com.navblerelay.receiver.NavBroadcastReceiver
import com.navblerelay.service.NavBleService

class MainActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "MainActivity"
    }

    private lateinit var statusDot: View
    private lateinit var statusText: TextView
    private lateinit var bleStatus: TextView
    private lateinit var broadcastStatusRow: LinearLayout
    private lateinit var broadcastStatus: TextView
    private lateinit var btnStart: Button
    private lateinit var btnStop: Button
    private lateinit var btnTestBroadcast: Button

    private lateinit var navArrow: ImageView
    private lateinit var navDistance: TextView
    private lateinit var navNextRoad: TextView
    private lateinit var navIconLabel: TextView
    private lateinit var laneGuideView: LaneGuideView
    private lateinit var trafficBarView: TrafficBarView
    private lateinit var btnTheme: ImageButton

    private val rows = mutableMapOf<String, Pair<TextView, TextView>>()

    private val handler = Handler(Looper.getMainLooper())
    private var checkRefreshRunnable: Runnable? = null

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { results ->
        if (results.values.all { it }) Log.i(TAG, "All permissions granted")
        else Toast.makeText(this, "部分权限被拒绝", Toast.LENGTH_SHORT).show()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        initViews()
        setupListeners()
        setupDataObserver()
        applyEdgeToEdgeInsets()
        requestPermissionsIfNeeded()
    }

    override fun onResume() {
        super.onResume()
        if (SettingsActivity.isKeepScreenOn(this)) {
            window.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        } else {
            window.clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        }
        updateServiceState()
        refreshUI()
        updateThemeIcon()
    }

    /**
     * Android 15+ 强制边缘到边缘显示，使用 WindowInsetsCompat 安全处理系统栏内边距。
     */
    private fun applyEdgeToEdgeInsets() {
        val rootView = findViewById<View>(android.R.id.content)
        ViewCompat.setOnApplyWindowInsetsListener(rootView) { view, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            val displayCutout = insets.getInsets(WindowInsetsCompat.Type.displayCutout())
            view.setPadding(
                maxOf(systemBars.left, displayCutout.left),
                maxOf(systemBars.top, displayCutout.top),
                maxOf(systemBars.right, displayCutout.right),
                maxOf(systemBars.bottom, displayCutout.bottom)
            )
            WindowInsetsCompat.CONSUMED
        }
        ViewCompat.requestApplyInsets(rootView)
    }

    private fun initViews() {
        statusDot = findViewById(R.id.status_dot)
        statusText = findViewById(R.id.status_text)
        bleStatus = findViewById(R.id.ble_status)
        broadcastStatusRow = findViewById(R.id.broadcast_status_row)
        broadcastStatus = findViewById(R.id.broadcast_status)
        btnStart = findViewById(R.id.btn_start)
        btnStop = findViewById(R.id.btn_stop)
        btnTestBroadcast = findViewById(R.id.btn_test_broadcast)

        navArrow = findViewById(R.id.nav_arrow)
        navDistance = findViewById(R.id.nav_distance)
        navNextRoad = findViewById(R.id.nav_next_road)
        navIconLabel = findViewById(R.id.nav_icon_label)
        laneGuideView = findViewById(R.id.lane_guide_view)
        trafficBarView = findViewById(R.id.traffic_bar_view)
        btnTheme = findViewById(R.id.btn_theme)
        updateThemeIcon()

        rows["map_state"] = bindRow(R.id.row_map_state, R.string.state)
        rows["cross_map"] = bindRow(R.id.row_cross_map, R.string.cross)
        rows["cur_road"] = bindRow(R.id.row_cur_road, R.string.current_road)
        rows["next_road"] = bindRow(R.id.row_next_road, R.string.next_road)
        rows["route_remain"] = bindRow(R.id.row_route_remain, R.string.route_remain)
        rows["cur_speed"] = bindRow(R.id.row_cur_speed, R.string.current_speed)
        rows["limited_speed"] = bindRow(R.id.row_limited_speed, R.string.speed_limit)
        rows["camera_dist"] = bindRow(R.id.row_camera_dist, R.string.camera)
        rows["sapa_dist"] = bindRow(R.id.row_sapa_dist, R.string.sapa)
        rows["traffic_light"] = bindRow(R.id.row_traffic_light, R.string.traffic_light)
        rows["drive_way_size"] = bindRow(R.id.row_drive_way_size, R.string.lane_count)
        rows["drive_way_detail"] = bindRow(R.id.row_drive_way_detail, R.string.lane_detail)
        rows["tmc_total"] = bindRow(R.id.row_tmc_total, R.string.total)
        rows["tmc_remain"] = bindRow(R.id.row_tmc_remain, R.string.remain)
        rows["tmc_segments"] = bindRow(R.id.row_tmc_segments, R.string.segments)
        rows["loc_speed"] = bindRow(R.id.row_loc_speed, R.string.current_speed)
        rows["loc_bearing"] = bindRow(R.id.row_loc_bearing, R.string.bearing)
        rows["loc_accuracy"] = bindRow(R.id.row_loc_accuracy, R.string.accuracy)
    }

    private fun bindRow(rowId: Int, labelRes: Int): Pair<TextView, TextView> {
        val row = findViewById<LinearLayout>(rowId)
        val label = row.findViewById<TextView>(R.id.item_label)
        val value = row.findViewById<TextView>(R.id.item_value)
        label.setText(labelRes)
        return label to value
    }

    private fun setupListeners() {
        findViewById<ImageButton>(R.id.btn_settings).setOnClickListener {
            SettingsActivity.start(this)
        }
        btnTheme.setOnClickListener { cycleThemeMode() }
        findViewById<ImageButton>(R.id.btn_about).setOnClickListener {
            AboutActivity.start(this)
        }

        btnStart.setOnClickListener {
            if (!hasRequiredPermissions()) {
                requestPermissionsIfNeeded()
                Toast.makeText(this, "请先授予权限", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            try {
                NavBleService.start(this)
                setServiceRunning(true)
            } catch (e: Exception) {
                Log.e(TAG, "Start service failed", e)
                Toast.makeText(this, "启动失败：${e.message}", Toast.LENGTH_SHORT).show()
            }
        }

        btnStop.setOnClickListener {
            NavBleService.stop(this)
            setServiceRunning(false)
        }

        btnTestBroadcast.setOnClickListener {
            val intent = Intent(NavBroadcastReceiver.SELF_TEST_ACTION)
            intent.setPackage(packageName)
            intent.putExtra("KEY_TYPE", 0)
            sendBroadcast(intent)
            Log.i(TAG, "Manual test broadcast sent")
            Toast.makeText(this, "已发送测试广播，查看 logcat: adb logcat -s NavBR:V", Toast.LENGTH_LONG).show()

            handler.postDelayed({
                if (NavDataHolder.broadcastReceived > 0) {
                    val ago = (System.currentTimeMillis() - NavDataHolder.broadcastReceived) / 1000
                    broadcastStatus.text = "已接收 (${ago}秒前)"
                    broadcastStatus.setTextColor(getColor(R.color.md_success))
                    Toast.makeText(this, "自检通过", Toast.LENGTH_SHORT).show()
                } else {
                    broadcastStatus.text = "未接收"
                    broadcastStatus.setTextColor(getColor(R.color.md_error))
                    Toast.makeText(this, "自检失败，请检查 logcat", Toast.LENGTH_LONG).show()
                }
            }, 1500)
        }
    }

    private fun setupDataObserver() {
        NavDataHolder.onDataChanged = { scheduleRefresh() }
    }

    private fun scheduleRefresh() {
        checkRefreshRunnable?.let { handler.removeCallbacks(it) }
        checkRefreshRunnable = Runnable { refreshUI() }
        handler.postDelayed(checkRefreshRunnable!!, 200)
    }

    // ── 权限 ─────────────────────────────────────────────

    private fun hasRequiredPermissions(): Boolean {
        val permissions = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
            permissions.add(Manifest.permission.POST_NOTIFICATIONS)
        return permissions.all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun requestPermissionsIfNeeded() {
        val permissions = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
            ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED)
            permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED)
            permissions.add(Manifest.permission.POST_NOTIFICATIONS)
        if (permissions.isNotEmpty()) permissionLauncher.launch(permissions.toTypedArray())
    }

    // ── 服务状态 ─────────────────────────────────────────

    private fun updateServiceState() = setServiceRunning(NavBleService.isRunning())

    private fun setServiceRunning(running: Boolean) {
        if (running) {
            statusDot.setBackgroundResource(R.drawable.status_dot_yellow)
            statusText.text = getString(R.string.status_running)
            statusText.setTextColor(getColor(R.color.md_status_running))
            btnStart.isEnabled = false
            btnStop.isEnabled = true
            btnTestBroadcast.visibility = View.VISIBLE
            broadcastStatusRow.visibility = View.VISIBLE
            broadcastStatus.text = getString(R.string.status_waiting)
            broadcastStatus.setTextColor(getColor(R.color.md_warning))
        } else {
            statusDot.setBackgroundResource(R.drawable.status_dot_red)
            statusText.text = getString(R.string.status_stopped)
            statusText.setTextColor(getColor(R.color.md_on_surface_variant))
            btnStart.isEnabled = true
            btnStop.isEnabled = false
            btnTestBroadcast.visibility = View.GONE
            bleStatus.text = getString(R.string.status_disconnected)
            bleStatus.setTextColor(getColor(R.color.md_status_disconnected))
            broadcastStatusRow.visibility = View.GONE
            resetAllData()
        }
    }

    private fun resetAllData() {
        rows.values.forEach { it.second.text = getString(R.string.not_available) }
        navArrow.setImageResource(R.drawable.ic_nav_straight)
        navArrow.rotation = 0f
        navDistance.text = getString(R.string.not_available)
        navNextRoad.text = getString(R.string.not_available)
        navIconLabel.text = getString(R.string.not_available)
        laneGuideView.visibility = View.GONE
        trafficBarView.visibility = View.GONE
    }

    // ── UI 刷新 ──────────────────────────────────────────

    private fun refreshUI() {
        if (NavDataHolder.bleConnected) {
            statusDot.setBackgroundResource(R.drawable.status_dot_green)
            statusText.text = getString(R.string.status_connected)
            statusText.setTextColor(getColor(R.color.md_status_connected))
            bleStatus.text = "已连接 ${NavDataHolder.bleDeviceAddress ?: ""}"
            bleStatus.setTextColor(getColor(R.color.md_status_connected))
        } else if (!btnStart.isEnabled) {
            bleStatus.text = getString(R.string.status_waiting)
            bleStatus.setTextColor(getColor(R.color.md_warning))
        }

        val last = NavDataHolder.broadcastReceived
        if (last > 0) {
            val sec = (System.currentTimeMillis() - last) / 1000
            val ago = if (sec < 60) "${sec}秒前" else "${sec / 60}分钟前"
            broadcastStatus.text = "已接收 ($ago)"
            broadcastStatus.setTextColor(getColor(R.color.md_success))
        }

        refreshMapState()
        refreshGuideInfo()
        refreshDriveWay()
        refreshTmc()
        refreshLocation()
    }

    private fun refreshMapState() {
        val s = NavDataHolder.mapState
        rows["map_state"]?.second?.text = when (s) {
            -1 -> getString(R.string.not_available)
            0 -> "空闲 / IDLE"
            1 -> "导航中 / NAVIGATING"
            2 -> "已到达 / ARRIVED"
            3 -> "已暂停 / PAUSED"
            else -> "状态: $s"
        }
        rows["cross_map"]?.second?.text = NavDataHolder.crossMap ?: getString(R.string.not_available)
    }

    private fun refreshGuideInfo() {
        val i = NavDataHolder.guideInfo ?: return
        rows["cur_road"]?.second?.text = i.curRoadName.ifEmpty { getString(R.string.not_available) }
        rows["next_road"]?.second?.text = i.nextRoadName.ifEmpty { getString(R.string.not_available) }
        rows["route_remain"]?.second?.text = if (i.routeRemainDis > 0) {
            "%.1f 公里 / %d 分钟".format(i.routeRemainDis / 1000f, i.routeRemainTime / 60)
        } else getString(R.string.not_available)
        rows["cur_speed"]?.second?.text = if (i.curSpeed > 0) "${i.curSpeed} km/h" else getString(R.string.not_available)
        rows["limited_speed"]?.second?.text = if (i.limitedSpeed > 0) "${i.limitedSpeed} km/h" else getString(R.string.not_available)
        rows["camera_dist"]?.second?.text = if (i.cameraDist > 0) "${i.cameraDist} m" else getString(R.string.not_available)
        rows["sapa_dist"]?.second?.text = if (i.sapaDist > 0) "${i.sapaName} ${i.sapaDist}m" else getString(R.string.not_available)
        rows["traffic_light"]?.second?.text = if (i.trafficLightNum > 0) "${i.trafficLightNum}" else getString(R.string.not_available)

        navArrow.setImageResource(AmapAutoProtocol.iconDrawableRes(i.icon))
        navArrow.rotation = 0f
        navDistance.text = if (i.segRemainDis > 0) {
            if (i.segRemainDis >= 1000) "%.1f km".format(i.segRemainDis / 1000f)
            else "${i.segRemainDis} m"
        } else getString(R.string.not_available)
        navNextRoad.text = i.nextRoadName.ifEmpty { getString(R.string.not_available) }
        navIconLabel.text = AmapAutoProtocol.iconShort(i.icon)
    }

    private fun refreshDriveWay() {
        val i = NavDataHolder.driveWayInfo ?: return
        rows["drive_way_size"]?.second?.text = if (i.enabled) "${i.size} 车道" else getString(R.string.not_available)
        if (i.enabled && i.lanes.isNotEmpty()) {
            laneGuideView.setLanes(i.lanes)
            laneGuideView.visibility = View.VISIBLE
            rows["drive_way_detail"]?.second?.text = i.lanes.joinToString(" ") {
                when (it.backIcon) {
                    0 -> "↑" ; 1 -> "↖" ; 2 -> "↗" ; 3 -> "←"
                    4 -> "→" ; 5 -> "↙" ; 6 -> "↘" ; 7 -> "↩"
                    else -> "•"
                }
            }
        } else {
            laneGuideView.visibility = View.GONE
            rows["drive_way_detail"]?.second?.text = getString(R.string.not_available)
        }
    }

    private fun refreshTmc() {
        val i = NavDataHolder.tmcSegmentInfo ?: return
        if (i.enabled) {
            rows["tmc_total"]?.second?.text = "%.1f 公里".format(i.totalDistance / 1000f)
            rows["tmc_remain"]?.second?.text = "%.1f 公里".format(i.residualDistance / 1000f)
            rows["tmc_segments"]?.second?.text = "${i.size}"
            trafficBarView.setData(i.segments, i.totalDistance.coerceAtLeast(1))
            trafficBarView.visibility = View.VISIBLE
        } else {
            rows["tmc_total"]?.second?.text = getString(R.string.not_available)
            rows["tmc_remain"]?.second?.text = getString(R.string.not_available)
            rows["tmc_segments"]?.second?.text = getString(R.string.not_available)
            trafficBarView.visibility = View.GONE
        }
    }

    private fun refreshLocation() {
        val i = NavDataHolder.locationInfo ?: return
        rows["loc_speed"]?.second?.text = if (i.speed > 0) "${i.speed} km/h" else getString(R.string.not_available)
        rows["loc_bearing"]?.second?.text = if (i.bearing > 0) "${i.bearing}°" else getString(R.string.not_available)
        rows["loc_accuracy"]?.second?.text = if (i.accuracy > 0) "${i.accuracy} m" else getString(R.string.not_available)
    }

    private fun cycleThemeMode() {
        val current = SettingsActivity.getThemeMode(this)
        val next = (current + 1) % 3
        SettingsActivity.setThemeMode(this, next)
        Toast.makeText(this, getString(R.string.theme_changed, themeLabelFor(next)), Toast.LENGTH_SHORT).show()
        recreate()
    }

    private fun updateThemeIcon() {
        val mode = SettingsActivity.getThemeMode(this)
        val (icon, label) = when (mode) {
            SettingsActivity.THEME_LIGHT -> R.drawable.ic_theme_light to R.string.theme_light
            SettingsActivity.THEME_DARK -> R.drawable.ic_theme_dark to R.string.theme_dark
            else -> R.drawable.ic_theme_system to R.string.theme_system
        }
        btnTheme.setImageResource(icon)
        btnTheme.contentDescription = getString(label)
    }

    private fun themeLabelFor(mode: Int): String = when (mode) {
        SettingsActivity.THEME_LIGHT -> getString(R.string.theme_light)
        SettingsActivity.THEME_DARK -> getString(R.string.theme_dark)
        else -> getString(R.string.theme_system)
    }
}
