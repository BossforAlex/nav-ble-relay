package com.navblerelay.ble

import android.bluetooth.*
import android.bluetooth.le.AdvertiseCallback
import android.bluetooth.le.AdvertiseData
import android.bluetooth.le.AdvertiseSettings
import android.bluetooth.le.BluetoothLeAdvertiser
import android.content.Context
import android.os.ParcelUuid
import android.util.Log
import java.util.UUID
import org.json.JSONObject
import com.navblerelay.protocol.*

/**
 * BLE GATT Server (Peripheral)
 * Android 端作为 BLE 外设，ESP32 作为中心设备连接
 */
class BleGattServer(private val context: Context) {

    companion object {
        private const val TAG = "BleGattServer"

        // ── UUID 定义 ────────────────────────────────────
        val SERVICE_UUID = UUID.fromString("0000FFE0-0000-1000-8000-00805F9B34FB")

        val CHAR_GUIDE_UUID    = UUID.fromString("0000FFE1-0000-1000-8000-00805F9B34FB")
        val CHAR_DRIVE_WAY_UUID   = UUID.fromString("0000FFE2-0000-1000-8000-00805F9B34FB")
        val CHAR_TMC_UUID         = UUID.fromString("0000FFE3-0000-1000-8000-00805F9B34FB")
        val CHAR_STATE_UUID       = UUID.fromString("0000FFE4-0000-1000-8000-00805F9B34FB")
        val CHAR_LOCATION_UUID    = UUID.fromString("0000FFE5-0000-1000-8000-00805F9B34FB")
    }

    private val bluetoothManager: BluetoothManager =
        context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private var gattServer: BluetoothGattServer? = null
    private var advertiser: BluetoothLeAdvertiser? = null

    private var guideCharacteristic: BluetoothGattCharacteristic? = null
    private var driveWayCharacteristic: BluetoothGattCharacteristic? = null
    private var tmcCharacteristic: BluetoothGattCharacteristic? = null
    private var stateCharacteristic: BluetoothGattCharacteristic? = null
    private var locationCharacteristic: BluetoothGattCharacteristic? = null

    private var connectedDevice: BluetoothDevice? = null

    var onDeviceConnected: ((BluetoothDevice) -> Unit)? = null
    var onDeviceDisconnected: ((BluetoothDevice) -> Unit)? = null
    var onError: ((String) -> Unit)? = null

    // ── 初始化 GATT Server ───────────────────────────────

    fun start() {
        val adapter = bluetoothManager.adapter
        if (adapter == null || !adapter.isEnabled) {
            onError?.invoke("蓝牙未开启")
            return
        }

        gattServer = bluetoothManager.openGattServer(context, gattServerCallback)
        addService()
        startAdvertising()
        Log.i(TAG, "BLE GATT Server started")
    }

    fun stop() {
        advertiser?.stopAdvertising(advertiseCallback)
        gattServer?.close()
        connectedDevice = null
        Log.i(TAG, "BLE GATT Server stopped")
    }

    // ── 添加 GATT 服务 ───────────────────────────────────

    private fun addService() {
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

        gattServer?.addService(service)
    }

    private fun createCharacteristic(uuid: UUID): BluetoothGattCharacteristic {
        return BluetoothGattCharacteristic(
            uuid,
            BluetoothGattCharacteristic.PROPERTY_NOTIFY or BluetoothGattCharacteristic.PROPERTY_READ,
            BluetoothGattCharacteristic.PERMISSION_READ
        )
    }

    // ── BLE 广播 ─────────────────────────────────────────

    private fun startAdvertising() {
        advertiser = bluetoothManager.adapter.bluetoothLeAdvertiser ?: run {
            onError?.invoke("设备不支持 BLE 广播")
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

        advertiser?.startAdvertising(settings, data, advertiseCallback)
    }

    private val advertiseCallback = object : AdvertiseCallback() {
        override fun onStartSuccess(settingsInEffect: AdvertiseSettings?) {
            Log.i(TAG, "BLE advertising started successfully")
        }

        override fun onStartFailure(errorCode: Int) {
            val msg = when (errorCode) {
                AdvertiseCallback.ADVERTISE_FAILED_DATA_TOO_LARGE -> "广播数据过大"
                AdvertiseCallback.ADVERTISE_FAILED_TOO_MANY_ADVERTISERS -> "广播实例过多"
                AdvertiseCallback.ADVERTISE_FAILED_ALREADY_STARTED -> "广播已启动"
                AdvertiseCallback.ADVERTISE_FAILED_INTERNAL_ERROR -> "内部错误"
                AdvertiseCallback.ADVERTISE_FAILED_FEATURE_UNSUPPORTED -> "设备不支持"
                else -> "未知错误($errorCode)"
            }
            onError?.invoke("BLE 广播失败: $msg")
        }
    }

    // ── GATT Server 回调 ────────────────────────────────

    private val gattServerCallback = object : BluetoothGattServerCallback() {

        override fun onConnectionStateChange(device: BluetoothDevice, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.i(TAG, "Device connected: ${device.address}")
                connectedDevice = device
                // 协商更大的 MTU (不跨方法调用，ESP32 侧发起)
                onDeviceConnected?.invoke(device)
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.i(TAG, "Device disconnected: ${device.address}")
                connectedDevice = null
                onDeviceDisconnected?.invoke(device)
            }
        }

        override fun onMtuChanged(device: BluetoothDevice?, mtu: Int) {
            Log.i(TAG, "MTU changed: $mtu")
        }
    }

    // ── 数据发送 ─────────────────────────────────────────

    fun sendGuideInfo(info: GuideInfo) {
        val json = JSONObject().apply {
            put("type", AmapAutoProtocol.KEY_GUIDE_INFO)
            put("ts", System.currentTimeMillis())
            put("data", JSONObject().apply {
                put("TYPE", info.type)
                put("ICON", info.icon)
                put("CUR_ROAD_NAME", info.curRoadName)
                put("NEXT_ROAD_NAME", info.nextRoadName)
                put("NEXT_NEXT_ROAD_NAME", info.nextNextRoadName)
                put("NEXT_NEXT_TURN_ICON", info.nextNextTurnIcon)
                put("ROUTE_REMAIN_DIS", info.routeRemainDis)
                put("ROUTE_REMAIN_TIME", info.routeRemainTime)
                put("ROUTE_ALL_DIS", info.routeAllDis)
                put("ROUTE_ALL_TIME", info.routeAllTime)
                put("SEG_REMAIN_DIS", info.segRemainDis)
                put("SEG_REMAIN_TIME", info.segRemainTime)
                put("NEXT_SEG_REMAIN_DIS", info.nextSegRemainDis)
                put("CUR_SPEED", info.curSpeed)
                put("LIMITED_SPEED", info.limitedSpeed)
                put("ROAD_TYPE", info.roadType)
                put("CAMERA_DIST", info.cameraDist)
                put("CAMERA_TYPE", info.cameraType)
                put("CAMERA_SPEED", info.cameraSpeed)
                put("SAPA_DIST", info.sapaDist)
                put("SAPA_NAME", info.sapaName)
                put("TRAFFIC_LIGHT_NUM", info.trafficLightNum)
                put("ROUND_ABOUT_NUM", info.roundAboutNum)
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
                            put("drive_way_number", lane.number.toString())
                            put("drive_way_lane_Back_icon", lane.backIcon.toString())
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
                put("tmc_segment_enabled", info.enabled)
                put("tmc_segment_size", info.size)
                put("total_distance", info.totalDistance)
                put("residual_distance", info.residualDistance)
                put("finish_distance", info.finishDistance)
                put("tmc_info", org.json.JSONArray().apply {
                    info.segments.forEach { seg ->
                        put(JSONObject().apply {
                            put("tmc_segment_number", seg.number.toString())
                            put("tmc_status", seg.status.toString())
                            put("tmc_segment_distance", seg.distance.toString())
                            put("tmc_segment_percent", seg.percent)
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
                put("time", info.time)
                put("provider", info.provider)
            })
        }
        notifyCharacteristic(locationCharacteristic, json.toString())
    }

    // ── Notify 发送 ──────────────────────────────────────

    private fun notifyCharacteristic(ch: BluetoothGattCharacteristic?, value: String) {
        if (ch == null || connectedDevice == null) return
        ch.value = value.toByteArray(Charsets.UTF_8)
        val success = gattServer?.notifyCharacteristicChanged(connectedDevice, ch, false) ?: false
        if (!success) {
            Log.w(TAG, "Failed to notify characteristic: ${ch.uuid}")
        }
    }

    val isConnected: Boolean get() = connectedDevice != null
}