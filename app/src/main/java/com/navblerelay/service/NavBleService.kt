package com.navblerelay.service

import android.Manifest
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.IBinder
import android.util.Log
import androidx.core.content.ContextCompat
import com.navblerelay.MainActivity
import com.navblerelay.R
import com.navblerelay.ble.BleGattServer
import com.navblerelay.protocol.AmapAutoProtocol
import com.navblerelay.protocol.*
import com.navblerelay.protocol.NavDataHolder
import com.navblerelay.receiver.NavBroadcastReceiver

/**
 * 前台 Service：持续监听高德导航广播并通过 BLE 转发
 * 参考文档 §3.1-3.5 AmapAuto 标准广播协议
 */
class NavBleService : Service() {

    companion object {
        private const val TAG = "NavBleService"
        private const val CHANNEL_ID = "nav_ble_channel"
        private const val NOTIFICATION_ID = 1001
        const val ACTION_STOP = "com.navblerelay.action.STOP_SERVICE"

        fun start(context: Context) {
            val intent = Intent(context, NavBleService::class.java)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }

        fun stop(context: Context) {
            val intent = Intent(context, NavBleService::class.java).apply {
                action = ACTION_STOP
            }
            context.startService(intent)
        }
    }

    private var broadcastReceiver: NavBroadcastReceiver? = null
    private var bleServer: BleGattServer? = null

    override fun onCreate() {
        super.onCreate()
        Log.i(TAG, "Service onCreate")

        try {
            createNotificationChannel()
            startForeground(NOTIFICATION_ID, createNotification())
            Log.i(TAG, "Foreground notification started")

            bleServer = BleGattServer(this).apply {
                onDeviceConnected = { device ->
                    Log.i(TAG, "ESP32 connected: ${device.address}")
                    NavDataHolder.bleConnected = true
                    NavDataHolder.bleDeviceAddress = device.address
                    updateNotification("已连接: ${device.address}")
                }
                onDeviceDisconnected = { device ->
                    Log.i(TAG, "ESP32 disconnected: ${device.address}")
                    NavDataHolder.bleConnected = false
                    NavDataHolder.bleDeviceAddress = null
                    updateNotification("等待 ESP32 连接...")
                }
                onError = { msg ->
                    Log.e(TAG, "BLE error: $msg")
                }
            }

            // 检查蓝牙权限后再启动 BLE
            if (hasBluetoothPermission()) {
                bleServer?.start()
            } else {
                Log.w(TAG, "缺少 BLUETOOTH_CONNECT 权限，跳过 BLE 启动")
            }

            registerBroadcastReceiver()
        } catch (e: Exception) {
            Log.e(TAG, "Service onCreate failed", e)
            stopSelf()
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            Log.i(TAG, "Received stop action")
            stopSelf()
            return START_NOT_STICKY
        }
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        Log.i(TAG, "Service onDestroy")
        try {
            broadcastReceiver?.let { unregisterReceiver(it) }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to unregister receiver", e)
        }
        try {
            bleServer?.stop()
        } catch (e: Exception) {
            Log.w(TAG, "Failed to stop BLE server", e)
        }
        super.onDestroy()
    }

    private fun hasBluetoothPermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) ==
                    PackageManager.PERMISSION_GRANTED
        } else {
            true
        }
    }

    // ── 广播注册 ─────────────────────────────────────────

    private fun registerBroadcastReceiver() {
        val receiver = NavBroadcastReceiver()

        receiver.onGuideInfo = { info: GuideInfo ->
            NavDataHolder.guideInfo = info
            bleServer?.sendGuideInfo(info)
        }

        receiver.onMapState = { state, crossMap ->
            NavDataHolder.mapState = state
            NavDataHolder.crossMap = crossMap
            bleServer?.sendMapState(state, crossMap)
            when (state) {
                AmapAutoProtocol.STATE_START_NAV -> updateNotification("导航中...")
                AmapAutoProtocol.STATE_STOP_NAV -> updateNotification("导航已结束")
                AmapAutoProtocol.STATE_ARRIVE_DEST -> updateNotification("已到达目的地")
            }
        }

        receiver.onDriveWay = { info ->
            NavDataHolder.driveWayInfo = info
            bleServer?.sendDriveWay(info)
        }

        receiver.onTmcSegment = { info ->
            NavDataHolder.tmcSegmentInfo = info
            bleServer?.sendTmcSegment(info)
        }

        receiver.onLocation = { info ->
            NavDataHolder.locationInfo = info
            bleServer?.sendLocation(info)
        }

        broadcastReceiver = receiver

        // 注册所有已知的高德广播 Action
        val filter = IntentFilter()
        for (action in NavBroadcastReceiver.ALL_ACTIONS) {
            filter.addAction(action)
        }
        filter.priority = 999
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(receiver, filter, Context.RECEIVER_EXPORTED)
        } else {
            registerReceiver(receiver, filter)
        }
        Log.i(TAG, "✅ BroadcastReceiver registered for ${NavBroadcastReceiver.ALL_ACTIONS.size} actions (priority=999)")

        // 发送自检广播，验证接收器是否正常工作
        sendSelfTestBroadcast()
    }

    /**
     * 发送一条自检广播，验证 NavBroadcastReceiver 是否已正确注册。
     * 如果 logcat 中出现 "收到广播: action=com.navblerelay.SELF_TEST" 则说明接收器正常。
     */
    private fun sendSelfTestBroadcast() {
        try {
            val intent = Intent("com.navblerelay.SELF_TEST")
            intent.setPackage(packageName)
            intent.putExtra("KEY_TYPE", 0)
            sendBroadcast(intent)
            Log.i(TAG, "🔍 自检广播已发送 (action=com.navblerelay.SELF_TEST, pkg=$packageName)")
        } catch (e: Exception) {
            Log.w(TAG, "自检广播发送失败", e)
        }
    }

    // ── 通知栏 ──────────────────────────────────────────

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "导航BLE转发",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "蓝牙导航广播转发服务运行中"
            }
            val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            manager.createNotificationChannel(channel)
        }
    }

    private fun createNotification(): Notification {
        val pendingIntent = PendingIntent.getActivity(
            this, 0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val stopIntent = PendingIntent.getService(
            this, 0,
            Intent(this, NavBleService::class.java).apply { action = ACTION_STOP },
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Notification.Builder(this, CHANNEL_ID)
                .setContentTitle("导航BLE转发")
                .setContentText("等待 ESP32 连接...")
                .setSmallIcon(R.drawable.ic_navigation)
                .setContentIntent(pendingIntent)
                .addAction(android.R.drawable.ic_media_pause, "停止", stopIntent)
                .setOngoing(true)
                .build()
        } else {
            @Suppress("DEPRECATION")
            Notification.Builder(this)
                .setContentTitle("导航BLE转发")
                .setContentText("等待 ESP32 连接...")
                .setSmallIcon(R.drawable.ic_navigation)
                .setContentIntent(pendingIntent)
                .setOngoing(true)
                .build()
        }
    }

    private fun updateNotification(text: String) {
        try {
            val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            val pendingIntent = PendingIntent.getActivity(
                this, 0,
                Intent(this, MainActivity::class.java),
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )

            val stopIntent = PendingIntent.getService(
                this, 0,
                Intent(this, NavBleService::class.java).apply { action = ACTION_STOP },
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )

            val notification = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                Notification.Builder(this, CHANNEL_ID)
                    .setContentTitle("导航BLE转发")
                    .setContentText(text)
                    .setSmallIcon(R.drawable.ic_navigation)
                    .setContentIntent(pendingIntent)
                    .addAction(android.R.drawable.ic_media_pause, "停止", stopIntent)
                    .setOngoing(true)
                    .build()
            } else {
                @Suppress("DEPRECATION")
                Notification.Builder(this)
                    .setContentTitle("导航BLE转发")
                    .setContentText(text)
                    .setSmallIcon(R.drawable.ic_navigation)
                    .setContentIntent(pendingIntent)
                    .setOngoing(true)
                    .build()
            }
            manager.notify(NOTIFICATION_ID, notification)
        } catch (e: Exception) {
            Log.w(TAG, "Failed to update notification", e)
        }
    }
}