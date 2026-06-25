package com.navblerelay

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.navblerelay.ble.BleLogStore

/**
 * BLE 日志查看页
 *
 * 展示 BLE 连接、订阅、数据收发、错误等关键事件，
 * 方便用户在不连接电脑的情况下排查蓝牙问题。
 */
class BleLogActivity : AppCompatActivity() {

    companion object {
        fun start(context: Context) {
            context.startActivity(Intent(context, BleLogActivity::class.java))
        }
    }

    private lateinit var recyclerView: RecyclerView
    private lateinit var adapter: LogAdapter
    private lateinit var emptyView: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_ble_log)

        findViewById<ImageButton>(R.id.btn_back).setOnClickListener { finish() }
        findViewById<ImageButton>(R.id.btn_clear).setOnClickListener {
            BleLogStore.clear()
            refresh()
        }

        emptyView = findViewById(R.id.tv_empty)
        recyclerView = findViewById(R.id.recycler_logs)
        recyclerView.layoutManager = LinearLayoutManager(this).apply {
            stackFromEnd = true
        }
        adapter = LogAdapter(BleLogStore.getAll())
        recyclerView.adapter = adapter

        BleLogStore.onNewLog = {
            runOnUiThread {
                adapter.setData(BleLogStore.getAll())
                updateEmptyState()
                recyclerView.scrollToPosition(adapter.itemCount - 1)
            }
        }

        refresh()
    }

    override fun onDestroy() {
        BleLogStore.onNewLog = null
        super.onDestroy()
    }

    private fun refresh() {
        adapter.setData(BleLogStore.getAll())
        updateEmptyState()
    }

    private fun updateEmptyState() {
        emptyView.visibility = if (adapter.itemCount == 0) View.VISIBLE else View.GONE
    }

    private class LogAdapter(private var items: List<BleLogStore.Entry>) :
        RecyclerView.Adapter<LogAdapter.VH>() {

        fun setData(newItems: List<BleLogStore.Entry>) {
            items = newItems
            notifyDataSetChanged()
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
            val view = LayoutInflater.from(parent.context)
                .inflate(R.layout.item_ble_log, parent, false)
            return VH(view)
        }

        override fun onBindViewHolder(holder: VH, position: Int) {
            holder.bind(items[position])
        }

        override fun getItemCount(): Int = items.size

        class VH(itemView: View) : RecyclerView.ViewHolder(itemView) {
            private val tvTime: TextView = itemView.findViewById(R.id.tv_time)
            private val tvLevel: TextView = itemView.findViewById(R.id.tv_level)
            private val tvTag: TextView = itemView.findViewById(R.id.tv_tag)
            private val tvMsg: TextView = itemView.findViewById(R.id.tv_msg)

            fun bind(entry: BleLogStore.Entry) {
                tvTime.text = entry.formatTime()
                tvLevel.text = entry.level.name
                tvTag.text = entry.tag
                tvMsg.text = entry.message

                val colorRes = when (entry.level) {
                    BleLogStore.Entry.Level.ERROR -> R.color.md_error
                    BleLogStore.Entry.Level.WARN -> R.color.md_warning
                    BleLogStore.Entry.Level.INFO -> R.color.md_status_connected
                    else -> R.color.md_on_surface_variant
                }
                tvLevel.setTextColor(itemView.context.getColor(colorRes))
            }
        }
    }
}
