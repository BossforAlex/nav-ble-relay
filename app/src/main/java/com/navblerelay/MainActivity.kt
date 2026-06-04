package com.navblerelay

import android.os.Bundle
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {

    private lateinit var statusText: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        statusText = findViewById(R.id.status_text)

        findViewById<android.widget.Button>(R.id.btn_start).setOnClickListener {
            NavBleService.start(this)
            statusText.text = "服务已启动，等待高德导航广播..."
        }

        findViewById<android.widget.Button>(R.id.btn_stop).setOnClickListener {
            NavBleService.stop(this)
            statusText.text = "服务已停止"
        }
    }
}