package com.navblerelay.ble

import java.text.SimpleDateFormat
import java.util.Locale
import java.util.concurrent.CopyOnWriteArrayList

/**
 * BLE 日志环形缓存
 *
 * 用于在主界面 / 独立日志页展示 BLE 连接、收发、错误等关键事件，
 * 避免依赖 logcat 才能定位问题。
 */
object BleLogStore {

    private const val MAX_LOGS = 200

    data class Entry(
        val timestamp: Long = System.currentTimeMillis(),
        val level: Level,
        val tag: String,
        val message: String
    ) {
        enum class Level { VERBOSE, DEBUG, INFO, WARN, ERROR }

        private val formatter = SimpleDateFormat("HH:mm:ss.SSS", Locale.getDefault())

        fun formatTime(): String = formatter.format(timestamp)
    }

    private val logs = CopyOnWriteArrayList<Entry>()

    @Volatile
    var onNewLog: ((Entry) -> Unit)? = null

    fun log(level: Entry.Level, tag: String, message: String) {
        val entry = Entry(level = level, tag = tag, message = message)
        synchronized(logs) {
            logs.add(entry)
            if (logs.size > MAX_LOGS) logs.removeAt(0)
        }
        onNewLog?.invoke(entry)
    }

    fun v(tag: String, message: String) = log(Entry.Level.VERBOSE, tag, message)
    fun d(tag: String, message: String) = log(Entry.Level.DEBUG, tag, message)
    fun i(tag: String, message: String) = log(Entry.Level.INFO, tag, message)
    fun w(tag: String, message: String) = log(Entry.Level.WARN, tag, message)
    fun e(tag: String, message: String) = log(Entry.Level.ERROR, tag, message)

    fun getAll(): List<Entry> = synchronized(logs) { logs.toList() }

    fun clear() = synchronized(logs) { logs.clear() }
}
