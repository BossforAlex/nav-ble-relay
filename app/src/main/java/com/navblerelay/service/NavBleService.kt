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
import com.navblerelay.ble.BleLog
import com.navblerelay.protocol.AmapAutoProtocol
import com.navblerelay.protocol.NavDataHolder
import com.navblerelay.receiver.NavBroadcastReceiver

/**
 * 前台 Service：持续监听高德导航广播并通过 BLE 转发
 *
 * - Service 启动后注册广播接收器 NavBroadcastReceiver（动态注册）
 * - 启动 BLE GattServer，等待 ESP32 连接
 * - 前台服务保证应用在后台时依然活跃
 * - 支持 ACTION_STOP 主动停止
 */
class NavBleService : Service() {

    companion object {
        private const val TAG = "NavBleSvc"
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

        /**
         * 判断服务是否正在运行。
         *
         * 不使用已废弃的 ActivityManager.getRunningServices（Android 8+ 对第三方应用不可靠）。
         * 改用静态字段的方式记录服务状态，保证跨组件可见且稳定。
         */
        @Volatile
        private var running: Boolean = false

        fun isRunning(): Boolean = running
    }

    private var broadcastReceiver: NavBroadcastReceiver? = null
    private var bleServer: BleGattServer? = null

    override fun onCreate() {
        super.onCreate()
        running = true
        Log.i(TAG, "Service onCreate")
        BleLog.i(TAG, "前台服务 onCreate")

        try {
            createNotificationChannel()
            startForeground(NOTIFICATION_ID, createNotification("等待 ESP32 连接..."))
            val startedMsg = "前台服务已启动，通知 ID=$NOTIFICATION_ID"
            Log.i(TAG, "✅ $startedMsg")
            BleLog.i(TAG, startedMsg)

            bleServer = BleGattServer(this).apply {
                onDeviceConnected = { device ->
                    val msg = "ESP32 连接：${device.address}"
                    Log.i(TAG, "🟢 $msg")
                    BleLog.i(TAG, msg)
                    NavDataHolder.bleConnected = true
                    NavDataHolder.bleDeviceAddress = device.address
                    updateNotification("ESP32 已连接：${device.address}")
                }
                onDeviceDisconnected = { device ->
                    val msg = "ESP32 断开：${device.address}"
                    Log.i(TAG, "🔴 $msg")
                    BleLog.i(TAG, msg)
                    NavDataHolder.bleConnected = false
                    NavDataHolder.bleDeviceAddress = null
                    updateNotification("等待 ESP32 连接...")
                }
                onError = { msg ->
                    Log.e(TAG, "BLE 错误：$msg")
                    BleLog.e(TAG, "BLE 错误：$msg")
                }
            }

            if (hasBluetoothPermission()) {
                bleServer?.start()
            } else {
                Log.w(TAG, "缺少 BLUETOOTH_CONNECT/BLUETOOTH_ADVERTISE 权限，暂不启动 BLE")
            }

            registerBroadcastReceiver()
        } catch (t: Throwable) {
            Log.e(TAG, "Service onCreate 异常", t)
            stopSelf()
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            Log.i(TAG, "收到停止命令")
            stopSelf()
            return START_NOT_STICKY
        }
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        running = false
        Log.i(TAG, "Service onDestroy")
        BleLog.i(TAG, "前台服务 onDestroy")
        try {
            broadcastReceiver?.let { unregisterReceiver(it) }
            broadcastReceiver = null
        } catch (t: Throwable) {
            Log.w(TAG, "注销广播接收器失败", t)
        }
        try { bleServer?.stop() } catch (t: Throwable) {
            Log.w(TAG, "停止 BLE Server 失败", t)
        }
        bleServer = null
        super.onDestroy()
    }

    // ── 权限检查 ────────────────────────────────────────

    private fun hasBluetoothPermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) ==
                    PackageManager.PERMISSION_GRANTED &&
                    ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_ADVERTISE) ==
                    PackageManager.PERMISSION_GRANTED
        } else {
            true
        }
    }

    // ── 广播注册 ────────────────────────────────────────

    private fun registerBroadcastReceiver() {
        val receiver = NavBroadcastReceiver().apply {
            onGuideInfo = { info ->
                NavDataHolder.guideInfo = info
                bleServer?.sendGuideInfo(info)
            }
            onMapState = { state, crossMap ->
                NavDataHolder.mapState = state
                NavDataHolder.crossMap = crossMap
                bleServer?.sendMapState(state, crossMap)
                when (state) {
                    AmapAutoProtocol.STATE_START_NAV -> updateNotification("导航进行中")
                    AmapAutoProtocol.STATE_STOP_NAV -> updateNotification("导航已结束")
                    AmapAutoProtocol.STATE_ARRIVE_DEST -> updateNotification("已到达目的地")
                }
            }
            onDriveWay = { info ->
                NavDataHolder.driveWayInfo = info
                bleServer?.sendDriveWay(info)
            }
            onTmcSegment = { info ->
                NavDataHolder.tmcSegmentInfo = info
                bleServer?.sendTmcSegment(info)
            }
            onLocation = { info ->
                NavDataHolder.locationInfo = info
                bleServer?.sendLocation(info)
            }
        }
        broadcastReceiver = receiver

        val filter = IntentFilter()
        for (action in NavBroadcastReceiver.ALL_ACTIONS) {
            filter.addAction(action)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            // Android 13+ 需要 RECEIVER_EXPORTED / RECEIVER_NOT_EXPORTED
            registerReceiver(receiver, filter, Context.RECEIVER_EXPORTED)
        } else {
            registerReceiver(receiver, filter)
        }
        Log.i(TAG, "✅ 广播接收器已注册（${NavBroadcastReceiver.ALL_ACTIONS.size} 个 Action）")

        // 发送自检广播 —— 验证接收器工作正常
        sendSelfTestBroadcast()
    }

    private fun sendSelfTestBroadcast() {
        try {
            val intent = Intent(NavBroadcastReceiver.SELF_TEST_ACTION)
            intent.setPackage(packageName)
            intent.putExtra("KEY_TYPE", 0)
            sendBroadcast(intent)
            Log.i(TAG, "🔍 自检广播已发送")
        } catch (t: Throwable) {
            Log.w(TAG, "自检广播发送失败", t)
        }
    }

    // ── 通知 / 前台服务 ───────────────────────────────

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "导航 BLE 转发",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "蓝牙导航广播转发服务运行中"
                setShowBadge(false)
            }
            val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            manager.createNotificationChannel(channel)
        }
    }

    private fun createNotification(text: String): Notification {
        val contentIntent = PendingIntent.getActivity(
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
                .setContentTitle("导航 BLE 转发")
                .setContentText(text)
                .setSmallIcon(R.drawable.ic_navigation)
                .setContentIntent(contentIntent)
                .addAction(android.R.drawable.ic_media_pause, "停止", stopIntent)
                .setOngoing(true)
                .build()
        } else {
            @Suppress("DEPRECATION")
            Notification.Builder(this)
                .setContentTitle("导航 BLE 转发")
                .setContentText(text)
                .setSmallIcon(R.drawable.ic_navigation)
                .setContentIntent(contentIntent)
                .setOngoing(true)
                .build()
        }
    }

    private fun updateNotification(text: String) {
        try {
            val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            manager.notify(NOTIFICATION_ID, createNotification(text))
        } catch (t: Throwable) {
            Log.w(TAG, "更新通知失败", t)
        }
    }
}
