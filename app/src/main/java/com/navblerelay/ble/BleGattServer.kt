package com.navblerelay.ble

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

class BleGattServer(private val context: Context) {

    companion object {
        private const val TAG = "BleGatt"

        private val SERVICE_UUID = UUID.fromString("0000FFE0-0000-1000-8000-00805F9B34FB")

        private val CHAR_GUIDE_UUID =    UUID.fromString("0000FFE1-0000-1000-8000-00805F9B34FB")
        private val CHAR_DRIVE_WAY_UUID = UUID.fromString("0000FFE2-0000-1000-8000-00805F9B34FB")
        private val CHAR_TMC_UUID =       UUID.fromString("0000FFE3-0000-1000-8000-00805F9B34FB")
        private val CHAR_STATE_UUID =     UUID.fromString("0000FFE4-0000-1000-8000-00805F9B34FB")
        private val CHAR_LOCATION_UUID =  UUID.fromString("0000FFE5-0000-1000-8000-00805F9B34FB")

        private val CCCD_UUID = UUID.fromString("00002902-0000-1000-8000-00805F9B34FB")
    }

    private val bluetoothManager: BluetoothManager =
        context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private var adapter: BluetoothAdapter? = null
    private var gattServer: BluetoothGattServer? = null
    private var advertiser: BluetoothLeAdvertiser? = null

    private var guideCharacteristic: BluetoothGattCharacteristic? = null
    private var driveWayCharacteristic: BluetoothGattCharacteristic? = null
    private var tmcCharacteristic: BluetoothGattCharacteristic? = null
    private var stateCharacteristic: BluetoothGattCharacteristic? = null
    private var locationCharacteristic: BluetoothGattCharacteristic? = null

    private var connectedDevice: BluetoothDevice? = null
    private val isRunning = AtomicBoolean(false)

    var onDeviceConnected: ((BluetoothDevice) -> Unit)? = null
    var onDeviceDisconnected: ((BluetoothDevice) -> Unit)? = null
    var onError: ((String) -> Unit)? = null

    // ── 公共 API ────────────────────────────────────────

    fun start() {
        if (!isRunning.compareAndSet(false, true)) {
            Log.w(TAG, "BLE Server 已经在运行，跳过重复启动")
            return
        }

        adapter = bluetoothManager.adapter
        if (adapter == null || adapter?.isEnabled != true) {
            isRunning.set(false)
            onError?.invoke("蓝牙未开启")
            return
        }
        if (!hasBluetoothPermission()) {
            isRunning.set(false)
            onError?.invoke("缺少蓝牙权限（BLUETOOTH_CONNECT/BLUETOOTH_ADVERTISE）")
            return
        }

        try {
            gattServer = bluetoothManager.openGattServer(context, gattServerCallback)
            if (gattServer == null) {
                isRunning.set(false)
                onError?.invoke("无法打开 GATT Server")
                return
            }
            setupService()
            startAdvertising()
            Log.i(TAG, "✅ BLE GATT Server 已启动，服务 UUID=$SERVICE_UUID")
        } catch (se: SecurityException) {
            isRunning.set(false)
            Log.e(TAG, "启动 BLE 时缺少权限", se)
            onError?.invoke("启动 BLE 时缺少权限")
        } catch (t: Throwable) {
            isRunning.set(false)
            Log.e(TAG, "启动 BLE 失败", t)
            onError?.invoke("启动 BLE 失败：${t.message}")
        }
    }

    fun stop() {
        if (!isRunning.compareAndSet(true, false)) return
        try {
            if (hasBluetoothPermission()) {
                advertiser?.stopAdvertising(advertiseCallback)
            }
        } catch (ignored: Throwable) { /* 关闭时的异常忽略 */ }
        try { gattServer?.close() } catch (ignored: Throwable) { /* 同上 */ }
        gattServer = null
        advertiser = null
        connectedDevice = null
        Log.i(TAG, "BLE GATT Server 已停止")
    }

    val isConnected: Boolean get() = connectedDevice != null

    // ── 数据发送方法 ────────────────────────────────────────

    fun sendGuideInfo(info: GuideInfo) {
        val json = JSONObject().apply {
            put("type", AmapAutoProtocol.KEY_GUIDE_INFO)
            put("ts", System.currentTimeMillis())
            put("data", JSONObject().apply {
                put("CUR_ROAD_NAME", info.curRoadName)
                put("NEXT_ROAD_NAME", info.nextRoadName)
                put("ICON", info.icon)
                put("ROUTE_REMAIN_DIS", info.routeRemainDis)
                put("ROUTE_REMAIN_TIME", info.routeRemainTime)
                put("CUR_SPEED", info.curSpeed)
                put("LIMITED_SPEED", info.limitedSpeed)
                put("ROAD_TYPE", info.roadType)
                put("CAMERA_DIST", info.cameraDist)
                put("CAMERA_TYPE", info.cameraType)
                put("CAMERA_SPEED", info.cameraSpeed)
            })
        }
        notifyCharacteristic(guideCharacteristic, json.toString())
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
        notifyCharacteristic(driveWayCharacteristic, json.toString())
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
        notifyCharacteristic(tmcCharacteristic, json.toString())
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
        notifyCharacteristic(stateCharacteristic, json.toString())
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
        notifyCharacteristic(locationCharacteristic, json.toString())
    }

    // ── 内部实现 ────────────────────────────────────────

    private fun hasBluetoothPermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT) ==
                    PackageManager.PERMISSION_GRANTED &&
                    ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_ADVERTISE) ==
                    PackageManager.PERMISSION_GRANTED
        } else {
            true
        }
    }

    private fun setupService() {
        val service = BluetoothGattService(SERVICE_UUID, BluetoothGattService.SERVICE_TYPE_PRIMARY)

        guideCharacteristic = createCharacteristic(CHAR_GUIDE_UUID)
        driveWayCharacteristic = createCharacteristic(CHAR_DRIVE_WAY_UUID)
        tmcCharacteristic = createCharacteristic(CHAR_TMC_UUID)
        stateCharacteristic = createCharacteristic(CHAR_STATE_UUID)
        locationCharacteristic = createCharacteristic(CHAR_LOCATION_UUID)

        service.addCharacteristic(guideCharacteristic)
        service.addCharacteristic(driveWayCharacteristic)
        service.addCharacteristic(tmcCharacteristic)
        service.addCharacteristic(stateCharacteristic)
        service.addCharacteristic(locationCharacteristic)

        try {
            gattServer?.addService(service)
        } catch (se: SecurityException) {
            Log.e(TAG, "添加服务缺少权限", se)
        }
    }

    private fun createCharacteristic(uuid: UUID): BluetoothGattCharacteristic {
        val ch = BluetoothGattCharacteristic(
            uuid,
            BluetoothGattCharacteristic.PROPERTY_NOTIFY
                    or BluetoothGattCharacteristic.PROPERTY_READ,
            BluetoothGattCharacteristic.PERMISSION_READ
        )
        val cccd = BluetoothGattDescriptor(
            CCCD_UUID,
            BluetoothGattDescriptor.PERMISSION_READ
                    or BluetoothGattDescriptor.PERMISSION_WRITE
        )
        cccd.value = BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE
        ch.addDescriptor(cccd)
        return ch
    }

    private fun startAdvertising() {
        advertiser = adapter?.bluetoothLeAdvertiser
        if (advertiser == null) {
            onError?.invoke("设备不支持 BLE 广播")
            isRunning.set(false)
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

    private val advertiseCallback = object : AdvertiseCallback() {
        override fun onStartSuccess(settingsInEffect: AdvertiseSettings?) {
            Log.i(TAG, "BLE 广播已启动，等待 ESP32 连接...")
        }
        override fun onStartFailure(errorCode: Int) {
            val msg = when (errorCode) {
                ADVERTISE_FAILED_DATA_TOO_LARGE -> "广播数据过大"
                ADVERTISE_FAILED_TOO_MANY_ADVERTISERS -> "广播实例过多"
                ADVERTISE_FAILED_ALREADY_STARTED -> "广播已启动"
                ADVERTISE_FAILED_INTERNAL_ERROR -> "内部错误"
                ADVERTISE_FAILED_FEATURE_UNSUPPORTED -> "设备不支持"
                else -> "未知错误($errorCode)"
            }
            Log.e(TAG, "BLE 广播失败：$msg")
            onError?.invoke("BLE 广播失败：$msg")
        }
    }

    private val gattServerCallback = object : BluetoothGattServerCallback() {

        override fun onConnectionStateChange(device: BluetoothDevice, status: Int, newState: Int) {
            if (newState == BluetoothGatt.STATE_CONNECTED) {
                connectedDevice = device
                Log.i(TAG, "🟢 ESP32 已连接：${device.address}")
                onDeviceConnected?.invoke(device)
            } else if (newState == BluetoothGatt.STATE_DISCONNECTED) {
                Log.i(TAG, "🔴 ESP32 已断开：${device.address}")
                if (device.address == connectedDevice?.address) {
                    connectedDevice = null
                }
                onDeviceDisconnected?.invoke(device)
            }
        }

        override fun onServiceAdded(status: Int, service: BluetoothGattService?) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.i(TAG, "服务已添加：${service?.uuid}")
            }
        }

        override fun onDescriptorWriteRequest(
            device: BluetoothDevice,
            requestId: Int,
            descriptor: BluetoothGattDescriptor,
            preparedWrite: Boolean,
            responseNeeded: Boolean,
            offset: Int,
            value: ByteArray?
        ) {
            try {
                if (hasBluetoothPermission()) {
                    gattServer?.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, value)
                }
                val hex = value?.joinToString("") { "%02X".format(it) } ?: "null"
                Log.i(TAG, "CCCD 写入请求：characteristic=${descriptor.characteristic.uuid} value=$hex")
            } catch (t: Throwable) {
                Log.e(TAG, "处理 CCCD 写入失败", t)
            }
        }

        override fun onCharacteristicReadRequest(
            device: BluetoothDevice,
            requestId: Int,
            offset: Int,
            characteristic: BluetoothGattCharacteristic
        ) {
            try {
                if (hasBluetoothPermission()) {
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

    private fun notifyCharacteristic(ch: BluetoothGattCharacteristic?, value: String) {
        val device = connectedDevice ?: return
        if (ch == null) return
        val server = gattServer ?: return

        val bytes = value.toByteArray(Charsets.UTF_8)
        if (bytes.size > 500) {
            Log.w(TAG, "特征值 ${ch.uuid} 数据较大(${bytes.size}B)，请确保 ESP32 已协商足够 MTU")
        }

        ch.value = bytes
        try {
            if (!hasBluetoothPermission()) {
                Log.w(TAG, "缺少 BLUETOOTH_CONNECT 权限，跳过 notifyCharacteristicChanged")
                return
            }
            val success = server.notifyCharacteristicChanged(device, ch, false)
            if (!success) {
                Log.w(TAG, "notifyCharacteristicChanged 返回 false：${ch.uuid}")
            } else {
                Log.d(TAG, "📡 BLE 发送 ${ch.uuid}: ${bytes.size}B")
            }
        } catch (se: SecurityException) {
            Log.e(TAG, "缺少蓝牙权限，无法发送通知", se)
        } catch (t: Throwable) {
            Log.e(TAG, "发送通知异常", t)
        }
    }
}
