package com.navblerelay.protocol

/**
 * 导航数据共享单例
 * NavBleService 写入，MainActivity 读取并展示
 */
object NavDataHolder {

    /** 引导信息 */
    var guideInfo: GuideInfo? = null
        set(value) {
            field = value
            if (value != null) notifyChange()
        }

    /** 车道信息 */
    var driveWayInfo: DriveWayInfo? = null
        set(value) {
            field = value
            if (value != null) notifyChange()
        }

    /** 路况光柱图 */
    var tmcSegmentInfo: TmcSegmentInfo? = null
        set(value) {
            field = value
            if (value != null) notifyChange()
        }

    /** 定位信息 */
    var locationInfo: LocationInfo? = null
        set(value) {
            field = value
            if (value != null) notifyChange()
        }

    /** 地图状态码 */
    var mapState: Int = -1
        set(value) {
            field = value
            notifyChange()
        }

    /** 路口放大图 */
    var crossMap: String? = null
        set(value) {
            field = value
            notifyChange()
        }

    /** BLE 连接状态 */
    var bleConnected: Boolean = false
        set(value) {
            field = value
            notifyChange()
        }

    /** BLE 设备地址 */
    var bleDeviceAddress: String? = null
        set(value) {
            field = value
            notifyChange()
        }

    /** 最近一次收到广播的时间戳（用于判断广播接收器是否正常工作） */
    var broadcastReceived: Long = 0L
        set(value) {
            field = value
            notifyChange()
        }

    /** 最近一次收到广播的 Action */
    var lastBroadcastAction: String? = null
        set(value) {
            field = value
            notifyChange()
        }

    /** 数据变化回调（主线程调用） */
    var onDataChanged: (() -> Unit)? = null

    private fun notifyChange() {
        onDataChanged?.invoke()
    }
}