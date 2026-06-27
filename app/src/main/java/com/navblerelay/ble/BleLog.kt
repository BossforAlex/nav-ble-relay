package com.navblerelay.ble

import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * BLE 交互日志缓存
 * 供 NavBleService / BleGattServer 写入，EspBleActivity 实时展示
 */
object BleLog {

    enum class Level { INFO, WARN, ERROR }

    data class Entry(
        val time: Long = System.currentTimeMillis(),
        val level: Level,
        val tag: String,
        val message: String
    ) {
        val timeText: String
            get() = TIME_FORMAT.format(Date(time))
    }

    private const val MAX_SIZE = 200
    private val logs = mutableListOf<Entry>()

    @Volatile
    var onChanged: (() -> Unit)? = null

    private val TIME_FORMAT = SimpleDateFormat("HH:mm:ss.SSS", Locale.getDefault())

    @JvmStatic
    @JvmOverloads
    fun i(tag: String, message: String) = add(Level.INFO, tag, message)

    @JvmStatic
    @JvmOverloads
    fun w(tag: String, message: String) = add(Level.WARN, tag, message)

    @JvmStatic
    @JvmOverloads
    fun e(tag: String, message: String) = add(Level.ERROR, tag, message)

    private fun add(level: Level, tag: String, message: String) {
        synchronized(logs) {
            logs.add(Entry(level = level, tag = tag, message = message))
            if (logs.size > MAX_SIZE) logs.removeAt(0)
        }
        onChanged?.invoke()
    }

    fun getAll(): List<Entry> = synchronized(logs) { logs.toList() }

    fun clear() {
        synchronized(logs) { logs.clear() }
        onChanged?.invoke()
    }
}
