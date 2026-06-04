package com.navblerelay.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.IBinder
import android.util.Log
import com.navblerelay.MainActivity
import com.navblerelay.R
import com.navblerelay.ble.BleGattServer
import com.navblerelay.protocol.AmapAutoProtocol
import com.navblerelay.protocol.*
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

    private lateinit var broadcastReceiver: NavBroadcastReceiver
    private lateinit var bleServer: BleGattServer

    override fun onCreate() {
        super.onCreate()
        Log.i(TAG, "Service onCreate")

        createNotificationChannel()
        startForeground(NOTIFICATION_ID, createNotification())

        bleServer = BleGattServer(this)
        bleServer.onDeviceConnected = { device ->
            Log.i(TAG, "ESP32 connected: ${device.address}")
            updateNotification("已连接: ${device.address}")
        }
        bleServer.onDeviceDisconnected = { device ->
            Log.i(TAG, "ESP32 disconnected: ${device.address}")
            updateNotification("等待 ESP32 连接...")
        }
        bleServer.onError = { msg ->
            Log.e(TAG, "BLE error: $msg")
        }

        bleServer.start()
        registerBroadcastReceiver()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopSelf()
            return START_NOT_STICKY
        }
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        Log.i(TAG, "Service onDestroy")
        try {
            unregisterReceiver(broadcastReceiver)
        } catch (e: Exception) {
            Log.w(TAG, "Failed to unregister receiver", e)
        }
        bleServer.stop()
        super.onDestroy()
    }

    // ── 广播注册 ─────────────────────────────────────────

    private fun registerBroadcastReceiver() {
        broadcastReceiver = NavBroadcastReceiver()

        broadcastReceiver.onGuideInfo = { info: GuideInfo ->
            bleServer.sendGuideInfo(info)
        }

        broadcastReceiver.onMapState = { state, crossMap ->
            bleServer.sendMapState(state, crossMap)
            when (state) {
                AmapAutoProtocol.STATE_START_NAV -> updateNotification("导航中...")
                AmapAutoProtocol.STATE_STOP_NAV -> updateNotification("导航已结束")
                AmapAutoProtocol.STATE_ARRIVE_DEST -> updateNotification("已到达目的地")
            }
        }

        broadcastReceiver.onDriveWay = { info ->
            bleServer.sendDriveWay(info)
        }

        broadcastReceiver.onTmcSegment = { info ->
            bleServer.sendTmcSegment(info)
        }

        broadcastReceiver.onLocation = { info ->
            bleServer.sendLocation(info)
        }

        val filter = IntentFilter(AmapAutoProtocol.ACTION_SEND)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(broadcastReceiver, filter, Context.RECEIVER_NOT_EXPORTED)
        } else {
            registerReceiver(broadcastReceiver, filter)
        }
        Log.i(TAG, "BroadcastReceiver registered for ${AmapAutoProtocol.ACTION_SEND}")
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
        val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        val pendingIntent = PendingIntent.getActivity(
            this, 0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val notification = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Notification.Builder(this, CHANNEL_ID)
                .setContentTitle("导航BLE转发")
                .setContentText(text)
                .setSmallIcon(R.drawable.ic_navigation)
                .setContentIntent(pendingIntent)
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
    }
}