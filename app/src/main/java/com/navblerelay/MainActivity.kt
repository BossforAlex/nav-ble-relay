package com.navblerelay

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.navblerelay.service.NavBleService

class MainActivity : AppCompatActivity() {

    private lateinit var statusText: TextView

    private val requiredPermissions = buildList {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            add(Manifest.permission.BLUETOOTH_CONNECT)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            add(Manifest.permission.POST_NOTIFICATIONS)
        }
    }

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { grants ->
        val allGranted = grants.values.all { it }
        if (allGranted) {
            startService()
        } else {
            Toast.makeText(this, "需要蓝牙和通知权限才能启动服务", Toast.LENGTH_LONG).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        statusText = findViewById(R.id.status_text)

        findViewById<android.widget.Button>(R.id.btn_start).setOnClickListener {
            if (hasAllPermissions()) {
                startService()
            } else {
                permissionLauncher.launch(requiredPermissions.toTypedArray())
            }
        }

        findViewById<android.widget.Button>(R.id.btn_stop).setOnClickListener {
            NavBleService.stop(this)
            statusText.text = "服务已停止"
        }
    }

    private fun hasAllPermissions(): Boolean {
        return requiredPermissions.all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun startService() {
        NavBleService.start(this)
        statusText.text = "服务已启动，等待高德导航广播..."
    }
}