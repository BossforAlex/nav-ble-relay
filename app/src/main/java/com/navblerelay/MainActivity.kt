package com.navblerelay

import android.Manifest
import android.content.*
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.View
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.navblerelay.protocol.NavDataHolder
import com.navblerelay.receiver.NavBroadcastReceiver
import com.navblerelay.service.NavBleService

class MainActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "MainActivity"
        private val CYAN = Color.parseColor("#00D4FF")
        private val AMBER = Color.parseColor("#FFB300")
        private val RED = Color.parseColor("#F44336")
        private val GREEN = Color.parseColor("#4CAF50")
        private val GRAY = Color.parseColor("#555555")
        private val WHITE = Color.parseColor("#EAEAEA")
    }

    private lateinit var statusDot: View
    private lateinit var statusText: TextView
    private lateinit var bleStatus: TextView
    private lateinit var broadcastStatusRow: View
    private lateinit var broadcastStatus: TextView
    private lateinit var btnStart: Button
    private lateinit var btnStop: Button
    private lateinit var btnTestBroadcast: Button

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

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { results ->
        val allGranted = results.values.all { it }
        if (allGranted) Log.i(TAG, "All permissions granted")
        else Toast.makeText(this, "Some permissions denied", Toast.LENGTH_SHORT).show()
    }

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
                Toast.makeText(this, "Grant permissions first", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            try {
                NavBleService.start(this)
                setServiceRunning(true)
            } catch (e: Exception) {
                Log.e(TAG, "Start service failed", e)
                Toast.makeText(this, "Failed: ${e.message}", Toast.LENGTH_SHORT).show()
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
            Toast.makeText(this, "Test broadcast sent. Check logcat: adb logcat -s NavBR:V", Toast.LENGTH_LONG).show()

            handler.postDelayed({
                if (NavDataHolder.broadcastReceived > 0) {
                    val ago = (System.currentTimeMillis() - NavDataHolder.broadcastReceived) / 1000
                    broadcastStatus.text = "RECEIVED (${ago}s ago)"
                    broadcastStatus.setTextColor(GREEN)
                    Toast.makeText(this, "Self-test PASSED!", Toast.LENGTH_SHORT).show()
                } else {
                    broadcastStatus.text = "NOT RECEIVED"
                    broadcastStatus.setTextColor(RED)
                    Toast.makeText(this, "Self-test FAILED. Check logcat.", Toast.LENGTH_LONG).show()
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
            statusText.text = "RUNNING"
            statusText.setTextColor(AMBER)
            btnStart.isEnabled = false
            btnStop.isEnabled = true
            btnTestBroadcast.visibility = View.VISIBLE
            broadcastStatusRow.visibility = View.VISIBLE
            broadcastStatus.text = "WAITING..."
            broadcastStatus.setTextColor(AMBER)
        } else {
            statusDot.setBackgroundResource(R.drawable.status_dot_red)
            statusText.text = "STOPPED"
            statusText.setTextColor(GRAY)
            btnStart.isEnabled = true
            btnStop.isEnabled = false
            btnTestBroadcast.visibility = View.GONE
            bleStatus.text = "DISCONNECTED"
            bleStatus.setTextColor(GRAY)
            broadcastStatusRow.visibility = View.GONE
            resetAllData()
        }
    }

    private fun resetAllData() {
        arrayOf(mapState, crossMap, curRoad, nextRoad, routeRemain, curSpeed,
            limitedSpeed, cameraDist, sapaDist, trafficLight, driveWaySize,
            driveWayDetail, tmcTotal, tmcRemain, tmcSegments,
            locSpeed, locBearing, locAccuracy).forEach { it.text = "—" }
    }

    // ── UI 刷新 ──────────────────────────────────────────

    private fun refreshUI() {
        // BLE
        if (NavDataHolder.bleConnected) {
            statusDot.setBackgroundResource(R.drawable.status_dot_green)
            statusText.text = "CONNECTED"
            statusText.setTextColor(GREEN)
            bleStatus.text = "CONNECTED ${NavDataHolder.bleDeviceAddress ?: ""}"
            bleStatus.setTextColor(GREEN)
        } else if (!btnStart.isEnabled) {
            bleStatus.text = "WAITING"
            bleStatus.setTextColor(AMBER)
        }

        // Broadcast
        val last = NavDataHolder.broadcastReceived
        if (last > 0) {
            val sec = (System.currentTimeMillis() - last) / 1000
            val ago = if (sec < 60) "${sec}s ago" else "${sec / 60}min ago"
            broadcastStatus.text = "RECEIVED ($ago)"
            broadcastStatus.setTextColor(GREEN)
        }

        refreshMapState()
        refreshGuideInfo()
        refreshDriveWay()
        refreshTmc()
        refreshLocation()
    }

    private fun refreshMapState() {
        val s = NavDataHolder.mapState
        mapState.text = when (s) {
            -1 -> "—"
            0 -> "IDLE"
            1 -> "NAVIGATING"
            2 -> "ARRIVED"
            3 -> "PAUSED"
            else -> "STATE:$s"
        }
        crossMap.text = NavDataHolder.crossMap ?: "—"
    }

    private fun refreshGuideInfo() {
        val i = NavDataHolder.guideInfo ?: return
        curRoad.text = i.curRoadName.ifEmpty { "—" }
        nextRoad.text = i.nextRoadName.ifEmpty { "—" }
        routeRemain.text = if (i.routeRemainDis > 0) {
            "%.1f km / %d min".format(i.routeRemainDis / 1000f, i.routeRemainTime / 60)
        } else "—"
        curSpeed.text = if (i.curSpeed > 0) "${i.curSpeed} km/h" else "—"
        limitedSpeed.text = if (i.limitedSpeed > 0) "${i.limitedSpeed} km/h" else "—"
        cameraDist.text = if (i.cameraDist > 0) "${i.cameraDist} m" else "—"
        sapaDist.text = if (i.sapaDist > 0) "${i.sapaName} ${i.sapaDist}m" else "—"
        trafficLight.text = if (i.trafficLightNum > 0) "${i.trafficLightNum}" else "—"
    }

    private fun refreshDriveWay() {
        val i = NavDataHolder.driveWayInfo ?: return
        driveWaySize.text = if (i.enabled) "${i.size} lanes" else "—"
        if (i.enabled && i.lanes.isNotEmpty()) {
            driveWayDetail.text = i.lanes.joinToString(" ") {
                when (it.backIcon) {
                    0 -> "⬆" ; 1 -> "↖" ; 2 -> "↗" ; 3 -> "←"
                    4 -> "→" ; 5 -> "↙" ; 6 -> "↘" ; 7 -> "↩"
                    else -> "•"
                }
            }
        } else driveWayDetail.text = "—"
    }

    private fun refreshTmc() {
        val i = NavDataHolder.tmcSegmentInfo ?: return
        if (i.enabled) {
            tmcTotal.text = "%.1f km".format(i.totalDistance / 1000f)
            tmcRemain.text = "%.1f km".format(i.residualDistance / 1000f)
            tmcSegments.text = "${i.size}"
        } else { tmcTotal.text = "—" ; tmcRemain.text = "—" ; tmcSegments.text = "—" }
    }

    private fun refreshLocation() {
        val i = NavDataHolder.locationInfo ?: return
        locSpeed.text = if (i.speed > 0) "${i.speed} km/h" else "—"
        locBearing.text = if (i.bearing > 0) "${i.bearing}°" else "—"
        locAccuracy.text = if (i.accuracy > 0) "${i.accuracy} m" else "—"
    }
}