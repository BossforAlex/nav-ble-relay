package com.navblerelay

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.WindowManager
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SwitchCompat

class SettingsActivity : AppCompatActivity() {

    companion object {
        private const val PREFS_NAME = "nav_ble_prefs"
        private const val KEY_LOG_DETAIL = "log_detail"
        private const val KEY_KEEP_SCREEN_ON = "keep_screen_on"
        private const val KEY_AUTO_START = "auto_start"

        fun start(context: Context) {
            context.startActivity(Intent(context, SettingsActivity::class.java))
        }

        fun isLogDetailEnabled(context: Context): Boolean {
            return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .getBoolean(KEY_LOG_DETAIL, false)
        }

        fun isKeepScreenOn(context: Context): Boolean {
            return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .getBoolean(KEY_KEEP_SCREEN_ON, false)
        }

        fun isAutoStart(context: Context): Boolean {
            return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .getBoolean(KEY_AUTO_START, false)
        }
    }

    private lateinit var switchLogDetail: SwitchCompat
    private lateinit var switchKeepScreenOn: SwitchCompat
    private lateinit var switchAutoStart: SwitchCompat
    private lateinit var btnReset: LinearLayout
    private lateinit var tvServiceUuid: TextView
    private lateinit var tvNotifyChar: TextView

    private val prefs by lazy { getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE) }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)
        initViews()
        loadPrefs()
        setupListeners()
    }

    private fun initViews() {
        findViewById<ImageButton>(R.id.btn_back).setOnClickListener { finish() }
        switchLogDetail = findViewById(R.id.switch_log_detail)
        switchKeepScreenOn = findViewById(R.id.switch_keep_screen_on)
        switchAutoStart = findViewById(R.id.switch_auto_start)
        btnReset = findViewById(R.id.btn_reset_prefs)
        tvServiceUuid = findViewById(R.id.tv_service_uuid)
        tvNotifyChar = findViewById(R.id.tv_notify_char)
    }

    private fun loadPrefs() {
        switchLogDetail.isChecked = prefs.getBoolean(KEY_LOG_DETAIL, false)
        switchKeepScreenOn.isChecked = prefs.getBoolean(KEY_KEEP_SCREEN_ON, false)
        switchAutoStart.isChecked = prefs.getBoolean(KEY_AUTO_START, false)

        tvServiceUuid.text = "0000FFE0-0000-1000-8000-00805F9B34FB"
        tvNotifyChar.text = "0000FFE1-0000-1000-8000-00805F9B34FB"
    }

    private fun setupListeners() {
        switchLogDetail.setOnCheckedChangeListener { _, checked ->
            prefs.edit().putBoolean(KEY_LOG_DETAIL, checked).apply()
        }
        switchKeepScreenOn.setOnCheckedChangeListener { _, checked ->
            prefs.edit().putBoolean(KEY_KEEP_SCREEN_ON, checked).apply()
            updateKeepScreenOn(checked)
        }
        switchAutoStart.setOnCheckedChangeListener { _, checked ->
            prefs.edit().putBoolean(KEY_AUTO_START, checked).apply()
        }
        btnReset.setOnClickListener {
            prefs.edit().clear().apply()
            loadPrefs()
            Toast.makeText(this, "设置已恢复默认", Toast.LENGTH_SHORT).show()
        }
    }

    private fun updateKeepScreenOn(enabled: Boolean) {
        if (enabled) {
            window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        } else {
            window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        }
    }
}
