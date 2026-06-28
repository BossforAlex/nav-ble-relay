package com.navblerelay.ble

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.ParcelUuid
import android.util.Log
import androidx.core.content.ContextCompat
import com.navblerelay.protocol.AmapAutoProtocol
import com.navblerelay.protocol.DriveWayInfo
import com.navblerelay.protocol.GuideInfo
import com.navblerelay.protocol.LocationInfo
import com.navblerelay.protocol.TmcSegmentInfo
import org.json.JSONObject
import java.util.UUID
import java.util.concurrent.atomic.AtomicBoolean

class BleGattClient(private val context: Context) {

    companion object {
        private const val TAG = "BleGattClient"

        private val SERVICE_UUID = UUID.fromString("0000FFE0-0000-1000-8000-00805F9B34FB")

        private val CHAR_GUIDE_UUID =    UUID.fromString("0000FFE1-0000-1000-8000-00805F9B34FB")
        private val CHAR_DRIVE_WAY_UUID = UUID.fromString("0000FFE2-0000-1000-8000-00805F9B34FB")
        private val CHAR_TMC_UUID =       UUID.fromString("0000FFE3-0000-1000-8000-00805F9B34FB")
        private val CHAR_STATE_UUID =     UUID.fromString("0000FFE4-0000-1000-8000-00805F9B34FB")
        private val CHAR_LOCATION_UUID =  UUID.fromString("0000FFE5-0000-1000-8000-00805F9B34FB")

        // 设备名前缀：ESP32 广播名以此前缀开头，手机只连接这类设备
        private const val DEVICE_NAME_PREFIX = "ICA"
    }

    private val bluetoothManager: BluetoothManager =
        context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private var adapter: BluetoothAdapter? = null
    private var scanner: BluetoothLeScanner? = null
    private var gatt: BluetoothGatt? = null

    private val characteristics = mutableMapOf<UUID, BluetoothGattCharacteristic>()
    private val isRunning = AtomicBoolean(false)
    private val isScanning = AtomicBoolean(false)

    var onDeviceConnected: ((BluetoothDevice) -> Unit)? = null
    var onDeviceDisconnected: ((BluetoothDevice?) -> Unit)? = null
    var onError: ((String) -> Unit)? = null

    private fun blog(level: BleLogStore.Entry.Level, msg: String) {
        BleLogStore.log(level, "BleGatt", msg)
    }

    fun start() {
        if (!isRunning.compareAndSet(false, true)) {
            Log.w(TAG, "BLE Client 已经在运行，跳过重复启动")
            return
        }

        adapter = bluetoothManager.adapter
        if (adapter == null || adapter?.isEnabled != true) {
            isRunning.set(false)
            val msg = "蓝牙未开启"
            blog(BleLogStore.Entry.Level.ERROR, msg)
            onError?.invoke(msg)
            return
        }
        if (!hasBluetoothPermission()) {
            isRunning.set(false)
            val msg = "缺少蓝牙权限（BLUETOOTH_SCAN/BLUETOOTH_CONNECT）"
            blog(BleLogStore.Entry.Level.ERROR, msg)
            onError?.invoke(msg)
            return
        }

        startScan()
        Log.i(TAG, "✅ BLE GATT Client 已启动，扫描 $DEVICE_NAME_PREFIX* 设备...")
        blog(BleLogStore.Entry.Level.INFO, "BLE Client 已启动，扫描 $DEVICE_NAME_PREFIX* 设备...")
    }

    fun stop() {
        if (!isRunning.compareAndSet(true, false)) return
        try {
            if (hasBluetoothPermission()) {
                stopScan()
                gatt?.disconnect()
                gatt?.close()
            }
        } catch (ignored: Throwable) { }
        gatt = null
        characteristics.clear()
        Log.i(TAG, "BLE GATT Client 已停止")
        blog(BleLogStore.Entry.Level.INFO, "BLE Client 已停止")
    }

    val isConnected: Boolean get() = gatt != null && characteristics.isNotEmpty()

    private fun hasBluetoothPermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_SCAN) ==
                    PackageManager.PERMISSION_GRANTED &&
                    ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT) ==
                    PackageManager.PERMISSION_GRANTED
        } else {
            ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH) ==
                    PackageManager.PERMISSION_GRANTED &&
                    ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_ADMIN) ==
                    PackageManager.PERMISSION_GRANTED
        }
    }

    // ── 扫描 ───────────────────────────────────────────

    private fun startScan() {
        if (isScanning.get()) return
        val s = adapter?.bluetoothLeScanner
        if (s == null) {
            onError?.invoke("无法获取 BLE Scanner")
            return
        }
        scanner = s

        val filter = ScanFilter.Builder()
            .setDeviceName(DEVICE_NAME_PREFIX) // 平台会匹配以该名称开头的设备（部分版本需自行过滤）
            .build()

        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        try {
            s.startScan(listOf(filter), settings, scanCallback)
            isScanning.set(true)
            Log.i(TAG, "开始扫描 BLE 设备...")
            blog(BleLogStore.Entry.Level.INFO, "开始扫描 BLE 设备...")
        } catch (se: SecurityException) {
            Log.e(TAG, "启动扫描缺少权限", se)
            blog(BleLogStore.Entry.Level.ERROR, "启动扫描缺少权限")
        }
    }

    private fun stopScan() {
        if (!isScanning.getAndSet(false)) return
        try {
            if (hasBluetoothPermission()) scanner?.stopScan(scanCallback)
        } catch (t: Throwable) {
            Log.w(TAG, "停止扫描异常", t)
        }
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult?) {
            result ?: return
            val device = result.device
            val name = try {
                if (hasBluetoothPermission()) device.name else null
            } catch (t: Throwable) { null
            }
            Log.d(TAG, "扫描结果: ${device.address} name=${name ?: "(none)"} rssi=${result.rssi}")
            blog(BleLogStore.Entry.Level.DEBUG, "扫描结果: ${device.address} name=${name ?: "(none)"}")

            if (name.isNullOrBlank() || !name.startsWith(DEVICE_NAME_PREFIX)) return
            stopScan()
            connect(device)
        }

        override fun onScanFailed(errorCode: Int) {
            val msg = "扫描失败: $errorCode"
            Log.e(TAG, msg)
            blog(BleLogStore.Entry.Level.ERROR, msg)
            onError?.invoke(msg)
            isScanning.set(false)
        }
    }

    // ── 连接 ───────────────────────────────────────────

    private fun connect(device: BluetoothDevice) {
        Log.i(TAG, "发现目标设备 ${device.address}，正在连接...")
        blog(BleLogStore.Entry.Level.INFO, "发现目标设备 ${device.address}，正在连接...")
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
            } else {
                gatt = device.connectGatt(context, false, gattCallback)
            }
        } catch (se: SecurityException) {
            Log.e(TAG, "连接缺少权限", se)
            blog(BleLogStore.Entry.Level.ERROR, "连接缺少权限")
            restartScanLater()
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.i(TAG, "🟢 已连接到 ${gatt.device?.address}")
                blog(BleLogStore.Entry.Level.INFO, "已连接到 ${gatt.device?.address}")
                try {
                    if (hasBluetoothPermission()) gatt.discoverServices()
                } catch (se: SecurityException) {
                    Log.e(TAG, "发现服务缺少权限", se)
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.i(TAG, "🔴 已断开连接")
                blog(BleLogStore.Entry.Level.INFO, "已断开连接")
                this@BleGattClient.gatt?.close()
                this@BleGattClient.gatt = null
                characteristics.clear()
                onDeviceDisconnected?.invoke(gatt.device)
                restartScanLater()
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e(TAG, "服务发现失败: $status")
                blog(BleLogStore.Entry.Level.ERROR, "服务发现失败: $status")
                return
            }
            val service = gatt.getService(SERVICE_UUID)
            if (service == null) {
                Log.e(TAG, "未找到目标服务")
                blog(BleLogStore.Entry.Level.ERROR, "未找到目标服务")
                gatt.disconnect()
                return
            }
            characteristics[CHAR_GUIDE_UUID] = service.getCharacteristic(CHAR_GUIDE_UUID)
            characteristics[CHAR_DRIVE_WAY_UUID] = service.getCharacteristic(CHAR_DRIVE_WAY_UUID)
            characteristics[CHAR_TMC_UUID] = service.getCharacteristic(CHAR_TMC_UUID)
            characteristics[CHAR_STATE_UUID] = service.getCharacteristic(CHAR_STATE_UUID)
            characteristics[CHAR_LOCATION_UUID] = service.getCharacteristic(CHAR_LOCATION_UUID)

            Log.i(TAG, "✅ 服务发现完成，特征值数量=${characteristics.size}")
            blog(BleLogStore.Entry.Level.INFO, "服务发现完成，特征值数量=${characteristics.size}")
            gatt.device?.let { onDeviceConnected?.invoke(it) }
        }
    }

    private fun restartScanLater() {
        if (!isRunning.get()) return
        android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
            if (isRunning.get()) startScan()
        }, 3000)
    }

    // ── 数据发送方法 ──────────────────────────────────────

    fun sendGuideInfo(info: GuideInfo) {
        val compact = com.navblerelay.SettingsActivity.isCompactMode(context)
        val json = JSONObject().apply {
            put("type", AmapAutoProtocol.KEY_GUIDE_INFO)
            put("ts", System.currentTimeMillis())
            put("data", JSONObject().apply {
                put("ICON", info.icon)
                put("CUR_ROAD_NAME", info.curRoadName)
                put("NEXT_ROAD_NAME", info.nextRoadName)
                put("SEG_REMAIN_DIS", info.segRemainDis)
                put("turn_label", AmapAutoProtocol.iconShort(info.icon))
                put("distance_text", formatDistanceText(info.segRemainDis))
                put("intersection", formatIntersection(info.curRoadName, info.nextRoadName))
                if (!compact) {
                    put("ROUTE_REMAIN_DIS", info.routeRemainDis)
                    put("ROUTE_REMAIN_TIME", info.routeRemainTime)
                    put("CUR_SPEED", info.curSpeed)
                    put("LIMITED_SPEED", info.limitedSpeed)
                    put("ROAD_TYPE", info.roadType)
                    put("CAMERA_DIST", info.cameraDist)
                    put("CAMERA_TYPE", info.cameraType)
                    put("CAMERA_SPEED", info.cameraSpeed)
                }
            })
        }
        writeCharacteristic(CHAR_GUIDE_UUID, json.toString())
    }

    private fun formatDistanceText(meters: Int): String = when {
        meters <= 0 -> ""
        meters >= 1000 -> "%.1f km".format(meters / 1000f)
        else -> "$meters m"
    }

    private fun formatIntersection(cur: String, next: String): String {
        val c = cur.ifBlank { context.getString(com.navblerelay.R.string.unknown_road) }
        val n = next.ifBlank { context.getString(com.navblerelay.R.string.unknown_road) }
        return "$c → $n"
    }

    fun sendDriveWay(info: DriveWayInfo) {
        val json = JSONObject().apply {
            put("type", AmapAutoProtocol.KEY_DRIVE_WAY)
            put("ts", System.currentTimeMillis())
            put("data", JSONObject().apply {
                put("drive_way_enabled", info.enabled)
                put("drive_way_size", info.size)
                put("drive_way_info", org.json.JSONArray().apply {
                    info.lanes.forEach { lane ->
                        put(JSONObject().apply {
                            put("drive_way_number", lane.number)
                            put("drive_way_lane_Back_icon", lane.backIcon)
                        })
                    }
                })
            })
        }
        writeCharacteristic(CHAR_DRIVE_WAY_UUID, json.toString())
    }

    fun sendTmcSegment(info: TmcSegmentInfo) {
        val json = JSONObject().apply {
            put("type", AmapAutoProtocol.KEY_TMC_SEGMENT)
            put("ts", System.currentTimeMillis())
            put("data", JSONObject().apply {
                put("total_distance", info.totalDistance)
                put("residual_distance", info.residualDistance)
                put("tmc_info", org.json.JSONArray().apply {
                    info.segments.forEach { seg ->
                        put(JSONObject().apply {
                            put("tmc_segment_number", seg.number)
                            put("tmc_status", seg.status)
                            put("tmc_segment_distance", seg.distance)
                        })
                    }
                })
            })
        }
        writeCharacteristic(CHAR_TMC_UUID, json.toString())
    }

    fun sendMapState(state: Int, crossMap: String?) {
        val json = JSONObject().apply {
            put("type", AmapAutoProtocol.KEY_MAP_STATE)
            put("ts", System.currentTimeMillis())
            put("data", JSONObject().apply {
                put("EXTRA_STATE", state)
                if (crossMap != null) put("EXTRA_CROSS_MAP", crossMap)
            })
        }
        writeCharacteristic(CHAR_STATE_UUID, json.toString())
    }

    fun sendLocation(info: LocationInfo) {
        val json = JSONObject().apply {
            put("type", AmapAutoProtocol.KEY_LOCATION)
            put("ts", System.currentTimeMillis())
            put("data", JSONObject().apply {
                put("bearing", info.bearing)
                put("accuracy", info.accuracy)
                put("speed", info.speed)
                put("provider", info.provider)
            })
        }
        writeCharacteristic(CHAR_LOCATION_UUID, json.toString())
    }

    private fun writeCharacteristic(uuid: UUID, value: String) {
        val g = gatt ?: return
        val ch = characteristics[uuid] ?: return
        val bytes = value.toByteArray(Charsets.UTF_8)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            try {
                if (hasBluetoothPermission()) {
                    g.writeCharacteristic(ch, bytes, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
                }
            } catch (se: SecurityException) {
                Log.e(TAG, "写入缺少权限", se)
            }
        } else {
            ch.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            ch.value = bytes
            try {
                if (hasBluetoothPermission()) g.writeCharacteristic(ch)
            } catch (se: SecurityException) {
                Log.e(TAG, "写入缺少权限", se)
            }
        }

        if (com.navblerelay.SettingsActivity.isLogDetailEnabled(context)) {
            blog(BleLogStore.Entry.Level.DEBUG, "写入 ${uuid.toString().substring(4, 8)}: ${bytes.size}B")
        }
    }
}
