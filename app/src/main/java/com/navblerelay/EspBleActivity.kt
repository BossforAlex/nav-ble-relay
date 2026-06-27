package com.navblerelay

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.navblerelay.ble.BleLog
import com.navblerelay.protocol.NavDataHolder

/**
 * ESP 蓝牙调试界面
 *
 * - 实时展示 BLE 连接状态、设备名称/地址、ESP32 识别结果、连接时长
 * - 展示 BLE 交互日志（连接、MTU、CCCD、notify 等），方便硬件联调
 */
class EspBleActivity : AppCompatActivity() {

    companion object {
        fun start(context: Context) {
            context.startActivity(Intent(context, EspBleActivity::class.java))
        }
    }

    private lateinit var rvLogs: RecyclerView
    private lateinit var logAdapter: BleLogAdapter
    private val rows = mutableMapOf<String, Pair<TextView, TextView>>()
    private val handler = Handler(Looper.getMainLooper())
    private val uptimeRunnable = object : Runnable {
        override fun run() {
            refreshStatus()
            handler.postDelayed(this, 1000)
        }
    }
    private var previousNavCallback: (() -> Unit)? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_esp_ble)

        findViewById<ImageButton>(R.id.btn_back).setOnClickListener { finish() }
        findViewById<TextView>(R.id.btn_clear_logs).setOnClickListener {
            BleLog.clear()
        }

        rows["connection"] = bindRow(R.id.row_connection, R.string.esp_connected)
        rows["device_name"] = bindRow(R.id.row_device_name, R.string.esp_device_name)
        rows["device_address"] = bindRow(R.id.row_device_address, R.string.esp_device_address)
        rows["recognized"] = bindRow(R.id.row_recognized, R.string.esp_recognized)
        rows["uptime"] = bindRow(R.id.row_uptime, R.string.esp_uptime)

        rvLogs = findViewById(R.id.rv_logs)
        logAdapter = BleLogAdapter()
        rvLogs.layoutManager = LinearLayoutManager(this)
        rvLogs.adapter = logAdapter
        logAdapter.submit(BleLog.getAll())

        BleLog.onChanged = {
            runOnUiThread {
                logAdapter.submit(BleLog.getAll())
                if (logAdapter.itemCount > 0) {
                    rvLogs.scrollToPosition(logAdapter.itemCount - 1)
                }
            }
        }
        previousNavCallback = NavDataHolder.onDataChanged
        NavDataHolder.onDataChanged = {
            previousNavCallback?.invoke()
            runOnUiThread { refreshStatus() }
        }

        refreshStatus()
        handler.post(uptimeRunnable)
    }

    override fun onDestroy() {
        super.onDestroy()
        handler.removeCallbacks(uptimeRunnable)
        BleLog.onChanged = null
        NavDataHolder.onDataChanged = previousNavCallback
    }

    private fun bindRow(viewId: Int, labelRes: Int): Pair<TextView, TextView> {
        val root = findViewById<View>(viewId)
        val label = root.findViewById<TextView>(R.id.item_label)
        val value = root.findViewById<TextView>(R.id.item_value)
        label.text = getString(labelRes)
        return label to value
    }

    private fun refreshStatus() {
        val connected = NavDataHolder.bleConnected
        rows["connection"]?.second?.apply {
            text = if (connected) getString(R.string.esp_connected) else getString(R.string.esp_disconnected)
            setTextColor(
                ContextCompat.getColor(
                    this@EspBleActivity,
                    if (connected) R.color.md_status_connected else R.color.md_status_disconnected
                )
            )
        }
        rows["device_name"]?.second?.text = NavDataHolder.bleDeviceName ?: getString(R.string.unknown)
        rows["device_address"]?.second?.text = NavDataHolder.bleDeviceAddress ?: getString(R.string.unknown)
        rows["recognized"]?.second?.text = if (NavDataHolder.isEsp32) "是" else "否"
        rows["uptime"]?.second?.text = formatUptime()
    }

    private fun formatUptime(): String {
        val start = NavDataHolder.bleConnectedTime
        if (!NavDataHolder.bleConnected || start <= 0L) return "--"
        val sec = (System.currentTimeMillis() - start) / 1000
        return String.format("%02d:%02d:%02d", sec / 3600, (sec % 3600) / 60, sec % 60)
    }

    // ── RecyclerView Adapter ─────────────────────────────────

    private class BleLogAdapter : RecyclerView.Adapter<BleLogAdapter.VH>() {

        private var items: List<BleLog.Entry> = emptyList()

        fun submit(list: List<BleLog.Entry>) {
            items = list
            notifyDataSetChanged()
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
            val view = LayoutInflater.from(parent.context)
                .inflate(R.layout.item_log, parent, false)
            return VH(view)
        }

        override fun getItemCount(): Int = items.size

        override fun onBindViewHolder(holder: VH, position: Int) {
            holder.bind(items[position])
        }

        class VH(view: View) : RecyclerView.ViewHolder(view) {
            private val bar: View = view.findViewById(R.id.log_level_bar)
            private val timeTag: TextView = view.findViewById(R.id.tv_log_time_tag)
            private val message: TextView = view.findViewById(R.id.tv_log_message)

            fun bind(entry: BleLog.Entry) {
                val ctx = itemView.context
                timeTag.text = "${entry.timeText} [${entry.tag}]"
                message.text = entry.message
                val colorRes = when (entry.level) {
                    BleLog.Level.INFO -> R.color.md_primary
                    BleLog.Level.WARN -> R.color.md_warning
                    BleLog.Level.ERROR -> R.color.md_error
                }
                bar.setBackgroundColor(ContextCompat.getColor(ctx, colorRes))
            }
        }
    }
}
