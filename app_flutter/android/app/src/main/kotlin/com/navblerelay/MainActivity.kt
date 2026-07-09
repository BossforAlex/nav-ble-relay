package com.navblerelay

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.util.Log
import androidx.core.content.ContextCompat
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel

/**
 * Flutter 主 Activity
 *
 * 架构（用户最新需求）：
 *   - ESP32 = GATT Server（外设），被动接收手机写入的数据
 *   - 手机 = GATT Client（Flutter flutter_blue_plus），主动连接 ESP32 MAC 后写入
 *   - 手机端做 MAC 白名单限制（本端通过 SettingsService.targetMac 配置）
 *
 * 平台通道：
 *   - `com.navblerelay/broadcast`：启动 / 停止高德广播监听
 *   - `com.navblerelay/service`：启动 / 停止 BLE 前台服务（防止后台断联）
 *
 * 原生层不再实现 BLE GATT Server（避免双重角色冲突）。
 * flutter_blue_plus 已提供 GATT Client 能力，无需原生扩展。
 */
class MainActivity : FlutterActivity() {

    companion object {
        private const val TAG = "MainActivity"
        private const val CH_BROADCAST = "com.navblerelay/broadcast"
        private const val CH_SERVICE = "com.navblerelay/service"
    }

    private lateinit var broadcastChannel: MethodChannel
    private lateinit var serviceChannel: MethodChannel

    private var navReceiver: NavBroadcastReceiver? = null

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        broadcastChannel = MethodChannel(flutterEngine.dartExecutor.binaryMessenger, CH_BROADCAST)
            .apply { setMethodCallHandler(::onBroadcastCall) }
        serviceChannel = MethodChannel(flutterEngine.dartExecutor.binaryMessenger, CH_SERVICE)
            .apply { setMethodCallHandler(::onServiceCall) }
    }

    // ── 前台服务通道：start / stop ──────────────────────

    private fun onServiceCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "start" -> {
                startForegroundService()
                result.success(true)
            }
            "stop" -> {
                stopForegroundService()
                result.success(true)
            }
            else -> result.notImplemented()
        }
    }

    private fun startForegroundService() {
        val intent = Intent(this, BleForegroundService::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(intent)
        } else {
            startService(intent)
        }
        Log.i(TAG, "BLE 前台服务已请求启动")
    }

    private fun stopForegroundService() {
        val intent = Intent(this, BleForegroundService::class.java).apply {
            action = BleForegroundService.ACTION_STOP
        }
        startService(intent) // 发送 STOP 指令
        Log.i(TAG, "BLE 前台服务已请求停止")
    }

    // ── 广播通道：start / stop / selfTest ──────────────

    private fun onBroadcastCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "start" -> {
                if (!hasBroadcastPermission()) {
                    result.error("PERM", "缺少高德广播权限", null)
                    return
                }
                registerNavReceiver()
                result.success(true)
            }
            "stop" -> {
                unregisterNavReceiver()
                result.success(true)
            }
            "selfTest" -> {
                sendSelfTestBroadcast()
                result.success(true)
            }
            else -> result.notImplemented()
        }
    }

    /** 动态注册高德广播接收器，并将解析结果回传 Flutter */
    private fun registerNavReceiver() {
        if (navReceiver != null) return
        val receiver = NavBroadcastReceiver().apply {
            onParsed = { method, args ->
                runOnUiThread {
                    broadcastChannel.invokeMethod(method, args)
                }
            }
        }
        navReceiver = receiver

        val filter = IntentFilter()
        for (action in NavBroadcastReceiver.ALL_ACTIONS) {
            filter.addAction(action)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(receiver, filter, Context.RECEIVER_EXPORTED)
        } else {
            registerReceiver(receiver, filter)
        }
        Log.i(TAG, "高德广播接收器已注册（${NavBroadcastReceiver.ALL_ACTIONS.size} 个 Action）")
    }

    private fun unregisterNavReceiver() {
        navReceiver?.let {
            try { unregisterReceiver(it) } catch (t: Throwable) {
                Log.w(TAG, "注销广播接收器失败", t)
            }
        }
        navReceiver = null
    }

    private fun sendSelfTestBroadcast() {
        try {
            val intent = android.content.Intent(NavBroadcastReceiver.SELF_TEST_ACTION).apply {
                setPackage(packageName)
                putExtra("KEY_TYPE", 0)
            }
            sendBroadcast(intent)
            Log.i(TAG, "自检广播已发送")
        } catch (t: Throwable) {
            Log.w(TAG, "自检广播发送失败", t)
        }
    }

    // ── 权限 ─────────────────────────────────────────────

    private fun hasBroadcastPermission(): Boolean {
        // Android 13+ 通知权限；本应用只接收高德内部广播，无需额外权限
        return true
    }

    override fun onDestroy() {
        unregisterNavReceiver()
        super.onDestroy()
    }
}
