package com.navblerelay

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattServer
import android.bluetooth.BluetoothGattServerCallback
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothManager
import android.bluetooth.le.AdvertiseCallback
import android.bluetooth.le.AdvertiseData
import android.bluetooth.le.AdvertiseSettings
import android.bluetooth.le.BluetoothLeAdvertiser
import android.content.Context
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.ParcelUuid
import android.util.Log
import androidx.core.content.ContextCompat
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import java.util.UUID

/**
 * Flutter 主 Activity
 *
 * - 注册两个 platform channel：
 *   - `com.navblerelay/broadcast`：启动 / 停止高德广播监听，自检
 *   - `com.navblerelay/ble`：启动 / 停止 BLE GATT Server，notify 推送数据
 *
 * 说明：Flutter 侧 [BleService] 通过 [FlutterBluePlus] 监听适配器状态，
 * 由于 flutter_blue_plus 在 Android 上不提供 peripheral/server 模式 API，
 * GATT Server 的广播与服务注册在此原生层实现，通过 channel 暴露给 Flutter。
 */
class MainActivity : FlutterActivity() {

    companion object {
        private const val TAG = "MainActivity"
        private const val CH_BROADCAST = "com.navblerelay/broadcast"
        private const val CH_BLE = "com.navblerelay/ble"

        // BLE 协议常量（与 Flutter 端 BleConstants 保持一致）
        private val SERVICE_UUID = UUID.fromString("0000ffe0-0000-1000-8000-00805f9b34fb")
        private val CHAR_GUIDE_UUID = UUID.fromString("0000ffe1-0000-1000-8000-00805f9b34fb")
        private val CHAR_DRIVE_WAY_UUID = UUID.fromString("0000ffe2-0000-1000-8000-00805f9b34fb")
        private val CHAR_TMC_UUID = UUID.fromString("0000ffe3-0000-1000-8000-00805f9b34fb")
        private val CHAR_STATE_UUID = UUID.fromString("0000ffe4-0000-1000-8000-00805f9b34fb")
        private val CHAR_LOCATION_UUID = UUID.fromString("0000ffe5-0000-1000-8000-00805f9b34fb")
        private val CCCD_UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        private const val DEVICE_NAME_PREFIX = "ICA"
    }

    private lateinit var broadcastChannel: MethodChannel
    private lateinit var bleChannel: MethodChannel

    private var navReceiver: NavBroadcastReceiver? = null
    private var bleServer: BleServer? = null

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        broadcastChannel = MethodChannel(flutterEngine.dartExecutor.binaryMessenger, CH_BROADCAST)
            .apply { setMethodCallHandler(::onBroadcastCall) }
        bleChannel = MethodChannel(flutterEngine.dartExecutor.binaryMessenger, CH_BLE)
            .apply { setMethodCallHandler(::onBleCall) }
    }

    // ── 广播通道：start / stop / selfTest ──────────────

    private fun onBroadcastCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "start" -> {
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
                // 在主线程回调 Flutter
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
            // Android 13+ 需要声明导出标志
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

    // ── BLE 通道：start / stop / notify ────────────────

    private fun onBleCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "start" -> {
                startBleServer(result)
            }
            "stop" -> {
                bleServer?.stop()
                result.success(true)
            }
            "notify" -> {
                val charUuid = call.argument<String>("char_uuid")
                val value = call.argument<String>("value")
                bleServer?.notify(charUuid, value)
                result.success(true)
            }
            else -> result.notImplemented()
        }
    }

    private fun startBleServer(result: MethodChannel.Result) {
        if (!hasBluetoothPermission()) {
            bleChannel.invokeMethod("onError", "缺少蓝牙权限（BLUETOOTH_CONNECT/BLUETOOTH_ADVERTISE）")
            result.success(false)
            return
        }
        if (bleServer == null) {
            bleServer = BleServer(this, ::onBleEvent)
        }
        bleServer?.start()
        result.success(true)
    }

    /** BLE 事件回调 → 转发到 Flutter */
    private fun onBleEvent(method: String, args: Map<String, Any?>) {
        runOnUiThread { bleChannel.invokeMethod(method, args) }
    }

    // ── 权限 ─────────────────────────────────────────────

    private fun hasBluetoothPermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) ==
                    PackageManager.PERMISSION_GRANTED &&
                    ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_ADVERTISE) ==
                    PackageManager.PERMISSION_GRANTED
        } else true
    }

    override fun onDestroy() {
        unregisterNavReceiver()
        bleServer?.stop()
        bleServer = null
        super.onDestroy()
    }

    // ── BLE GATT Server（紧凑实现）──────────────────────

    /**
     * 原生 BLE GATT Server：广播 ICA 名称，等待 ESP32 连接，
     * 并向 5 个特征值 notify 推送 JSON 数据。
     */
    private class BleServer(
        private val context: Context,
        private val onEvent: (String, Map<String, Any?>) -> Unit
    ) {
        private val bluetoothManager: BluetoothManager =
            context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        private var adapter: BluetoothAdapter? = bluetoothManager.adapter
        private var gattServer: BluetoothGattServer? = null
        private var advertiser: BluetoothLeAdvertiser? = null
        private var connectedDevice: BluetoothDevice? = null
        private val characteristics = mutableMapOf<UUID, BluetoothGattCharacteristic>()

        private val advertiseCallback = object : AdvertiseCallback() {
            override fun onStartSuccess(settingsInEffect: AdvertiseSettings?) {
                Log.i(TAG, "BLE 广播已启动，等待 ESP32 连接...")
            }

            override fun onStartFailure(errorCode: Int) {
                onEvent("onError", mapOf("message" to "BLE 广播失败：$errorCode"))
            }
        }

        private val serverCallback = object : BluetoothGattServerCallback() {
            override fun onConnectionStateChange(device: BluetoothDevice, status: Int, newState: Int) {
                val address = device.address
                if (newState == BluetoothGatt.STATE_CONNECTED) {
                    // 仅接受名称以 ICA 开头的设备，防止附近其他 BLE 设备误连
                    val remoteName = try {
                        if (context.hasBluetoothPermission()) device.name else null
                    } catch (t: Throwable) { null }

                    if (remoteName.isNullOrBlank() || !remoteName.startsWith(DEVICE_NAME_PREFIX)) {
                        Log.w(TAG, "拒绝非 ICA 设备连接：$address / 名称=${remoteName ?: "未知"}")
                        try {
                            if (context.hasBluetoothPermission()) gattServer?.cancelConnection(device)
                        } catch (t: Throwable) { }
                        return
                    }

                    // MAC 白名单（读取 Flutter SharedPreferences）
                    if (!isTargetDeviceAllowed(address)) {
                        Log.w(TAG, "拒绝未授权设备连接：$address")
                        try {
                            if (context.hasBluetoothPermission()) gattServer?.cancelConnection(device)
                        } catch (t: Throwable) { }
                        return
                    }

                    connectedDevice = device
                    Log.i(TAG, "ESP32 已连接：$address / 名称=$remoteName")
                    onEvent("onDeviceConnected", mapOf(
                        "address" to address,
                        "name" to (remoteName ?: DEVICE_NAME_PREFIX)
                    ))
                } else if (newState == BluetoothGatt.STATE_DISCONNECTED) {
                    Log.i(TAG, "ESP32 已断开：$address")
                    if (device.address == connectedDevice?.address) connectedDevice = null
                    onEvent("onDeviceDisconnected", mapOf("address" to address))
                }
            }

            override fun onServiceAdded(status: Int, service: BluetoothGattService?) {
                Log.i(TAG, "服务已添加：${service?.uuid} status=$status")
            }

            override fun onDescriptorWriteRequest(
                device: BluetoothDevice, requestId: Int, descriptor: BluetoothGattDescriptor,
                preparedWrite: Boolean, responseNeeded: Boolean, offset: Int, value: ByteArray?
            ) {
                try {
                    if (context.hasBluetoothPermission()) {
                        gattServer?.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, value)
                    }
                    Log.d(TAG, "CCCD 写入请求：${descriptor.characteristic?.uuid}")
                } catch (t: Throwable) {
                    Log.e(TAG, "处理 CCCD 写入失败", t)
                }
            }

            override fun onCharacteristicReadRequest(
                device: BluetoothDevice, requestId: Int, offset: Int,
                characteristic: BluetoothGattCharacteristic
            ) {
                try {
                    if (context.hasBluetoothPermission()) {
                        gattServer?.sendResponse(
                            device, requestId, BluetoothGatt.GATT_SUCCESS, offset, characteristic.value
                        )
                    }
                } catch (t: Throwable) {
                    Log.e(TAG, "读取特征值请求失败", t)
                }
            }

            override fun onMtuChanged(device: BluetoothDevice?, mtu: Int) {
                Log.i(TAG, "MTU 协商完成：$mtu 字节")
            }
        }

        fun start() {
            adapter = bluetoothManager.adapter
            if (adapter == null || adapter?.isEnabled != true) {
                onEvent("onError", mapOf("message" to "蓝牙未开启"))
                return
            }
            try {
                gattServer = bluetoothManager.openGattServer(context, serverCallback)
                if (gattServer == null) {
                    onEvent("onError", mapOf("message" to "无法打开 GATT Server"))
                    return
                }

                // 设置本机蓝牙名为 ICA，便于 ESP32 识别
                if (context.hasBluetoothPermission() && adapter?.name != DEVICE_NAME_PREFIX) {
                    try { adapter?.name = DEVICE_NAME_PREFIX } catch (t: Throwable) { }
                }

                setupService()
                startAdvertising()
                Log.i(TAG, "BLE GATT Server 已启动，设备名=$DEVICE_NAME_PREFIX")
            } catch (se: SecurityException) {
                onEvent("onError", mapOf("message" to "启动 BLE 时缺少权限: ${se.message}"))
            }
        }

        fun stop() {
            try {
                if (context.hasBluetoothPermission()) advertiser?.stopAdvertising(advertiseCallback)
            } catch (t: Throwable) { }
            try { gattServer?.close() } catch (t: Throwable) { }
            gattServer = null
            advertiser = null
            connectedDevice = null
            Log.i(TAG, "BLE GATT Server 已停止")
        }

        /** 向指定特征值 notify 推送 JSON */
        fun notify(charUuidStr: String?, value: String?) {
            val device = connectedDevice ?: return
            val server = gattServer ?: return
            if (charUuidStr == null || value == null) return
            val uuid = try { UUID.fromString(charUuidStr) } catch (t: Throwable) { return }
            val ch = characteristics[uuid] ?: return

            val bytes = value.toByteArray(Charsets.UTF_8)
            if (bytes.size > 500) {
                Log.w(TAG, "特征值 $uuid 数据较大(${bytes.size}B)，请确保 ESP32 已协商足够 MTU")
            }
            try {
                if (!context.hasBluetoothPermission()) return
                @Suppress("DEPRECATION")
                ch.value = bytes
                val success = server.notifyCharacteristicChanged(device, ch, false)
                if (!success) Log.w(TAG, "notifyCharacteristicChanged 返回 false：$uuid")
            } catch (se: SecurityException) {
                Log.e(TAG, "缺少蓝牙权限，无法发送通知", se)
            }
        }

        private fun setupService() {
            val service = BluetoothGattService(SERVICE_UUID, BluetoothGattService.SERVICE_TYPE_PRIMARY)
            listOf(CHAR_GUIDE_UUID, CHAR_DRIVE_WAY_UUID, CHAR_TMC_UUID,
                CHAR_STATE_UUID, CHAR_LOCATION_UUID).forEach { uuid ->
                val ch = BluetoothGattCharacteristic(
                    uuid,
                    BluetoothGattCharacteristic.PROPERTY_NOTIFY or BluetoothGattCharacteristic.PROPERTY_READ,
                    BluetoothGattCharacteristic.PERMISSION_READ
                )
                val cccd = BluetoothGattDescriptor(
                    CCCD_UUID,
                    BluetoothGattDescriptor.PERMISSION_READ or BluetoothGattDescriptor.PERMISSION_WRITE
                )
                @Suppress("DEPRECATION")
                cccd.value = BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE
                ch.addDescriptor(cccd)
                characteristics[uuid] = ch
                service.addCharacteristic(ch)
            }
            try {
                gattServer?.addService(service)
            } catch (se: SecurityException) {
                Log.e(TAG, "添加服务缺少权限", se)
            }
        }

        private fun startAdvertising() {
            advertiser = adapter?.bluetoothLeAdvertiser
            if (advertiser == null) {
                onEvent("onError", mapOf("message" to "设备不支持 BLE 广播"))
                return
            }
            val settings = AdvertiseSettings.Builder()
                .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
                .setConnectable(true)
                .setTimeout(0)
                .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_HIGH)
                .build()
            val data = AdvertiseData.Builder()
                .setIncludeDeviceName(true)
                .addServiceUuid(ParcelUuid(SERVICE_UUID))
                .build()
            try {
                advertiser?.startAdvertising(settings, data, advertiseCallback)
            } catch (se: SecurityException) {
                Log.e(TAG, "启动广播缺少权限", se)
            }
        }

        /** 读取 Flutter SharedPreferences 中的目标设备 MAC，进行白名单校验 */
        private fun isTargetDeviceAllowed(address: String): Boolean {
            val prefs = context.getSharedPreferences("FlutterSharedPreferences", Context.MODE_PRIVATE)
            val target = prefs.getString("flutter.target_device_mac", "")?.trim() ?: ""
            if (target.isEmpty()) return true
            return address.equals(target, ignoreCase = true)
        }
    }
}

/** 上下文蓝牙权限检查扩展 */
private fun Context.hasBluetoothPermission(): Boolean {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) ==
                PackageManager.PERMISSION_GRANTED &&
                ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_ADVERTISE) ==
                PackageManager.PERMISSION_GRANTED
    } else true
}
