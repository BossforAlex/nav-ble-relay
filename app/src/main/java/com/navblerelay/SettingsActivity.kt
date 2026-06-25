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
        private const val KEY_COMPACT_MODE = "compact_mode"
        private const val KEY_KEEP_SCREEN_ON = "keep_screen_on"
        private const val KEY_AUTO_START = "auto_start"
        private const val KEY_TARGET_DEVICE_MAC = "target_device_mac"

        fun start(context: Context) {
            context.startActivity(Intent(context, SettingsActivity::class.java))
        }

        fun isLogDetailEnabled(context: Context): Boolean {
            return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .getBoolean(KEY_LOG_DETAIL, false)
        }

        fun isCompactMode(context: Context): Boolean {
            return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .getBoolean(KEY_COMPACT_MODE, false)
        }

        fun isKeepScreenOn(context: Context): Boolean {
            return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .getBoolean(KEY_KEEP_SCREEN_ON, false)
        }

        fun isAutoStart(context: Context): Boolean {
            return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .getBoolean(KEY_AUTO_START, false)
        }

        fun getTargetDeviceMac(context: Context): String {
            return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .getString(KEY_TARGET_DEVICE_MAC, "")?.trim() ?: ""
        }

        fun isTargetDeviceAllowed(context: Context, address: String?): Boolean {
            val target = getTargetDeviceMac(context)
            if (target.isEmpty() || address == null) return true
            return address.equals(target, ignoreCase = true)
        }
    }

    private lateinit var switchLogDetail: SwitchCompat
    private lateinit var switchCompactMode: SwitchCompat
    private lateinit var switchKeepScreenOn: SwitchCompat
    private lateinit var switchAutoStart: SwitchCompat
    private lateinit var etTargetMac: android.widget.EditText
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
        switchCompactMode = findViewById(R.id.switch_compact_mode)
        switchKeepScreenOn = findViewById(R.id.switch_keep_screen_on)
        switchAutoStart = findViewById(R.id.switch_auto_start)
        etTargetMac = findViewById(R.id.et_target_mac)
        btnReset = findViewById(R.id.btn_reset_prefs)
        tvServiceUuid = findViewById(R.id.tv_service_uuid)
        tvNotifyChar = findViewById(R.id.tv_notify_char)
    }

    private fun loadPrefs() {
        switchLogDetail.isChecked = prefs.getBoolean(KEY_LOG_DETAIL, false)
        switchCompactMode.isChecked = prefs.getBoolean(KEY_COMPACT_MODE, false)
        switchKeepScreenOn.isChecked = prefs.getBoolean(KEY_KEEP_SCREEN_ON, false)
        switchAutoStart.isChecked = prefs.getBoolean(KEY_AUTO_START, false)
        etTargetMac.setText(prefs.getString(KEY_TARGET_DEVICE_MAC, "") ?: "")

        tvServiceUuid.text = "0000FFE0-0000-1000-8000-00805F9B34FB"
        tvNotifyChar.text = "0000FFE1-0000-1000-8000-00805F9B34FB"
    }

    private fun setupListeners() {
        switchLogDetail.setOnCheckedChangeListener { _, checked ->
            prefs.edit().putBoolean(KEY_LOG_DETAIL, checked).apply()
        }
        switchCompactMode.setOnCheckedChangeListener { _, checked ->
            prefs.edit().putBoolean(KEY_COMPACT_MODE, checked).apply()
        }
        switchKeepScreenOn.setOnCheckedChangeListener { _, checked ->
            prefs.edit().putBoolean(KEY_KEEP_SCREEN_ON, checked).apply()
            updateKeepScreenOn(checked)
        }
        switchAutoStart.setOnCheckedChangeListener { _, checked ->
            prefs.edit().putBoolean(KEY_AUTO_START, checked).apply()
        }
        etTargetMac.setOnFocusChangeListener { _, hasFocus ->
            if (!hasFocus) saveTargetMac()
        }
        btnReset.setOnClickListener {
            prefs.edit().clear().apply()
            loadPrefs()
            Toast.makeText(this, "设置已恢复默认", Toast.LENGTH_SHORT).show()
        }
    }

    private fun saveTargetMac() {
        val raw = etTargetMac.text.toString().trim()
        if (raw.isNotEmpty() && !isValidMac(raw)) {
            Toast.makeText(this, R.string.target_device_invalid, Toast.LENGTH_SHORT).show()
            return
        }
        prefs.edit().putString(KEY_TARGET_DEVICE_MAC, raw.uppercase()).apply()
    }

    private fun isValidMac(mac: String): Boolean {
        return mac.matches(Regex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$"))
    }

    override fun onPause() {
        super.onPause()
        saveTargetMac()
    }

    private fun updateKeepScreenOn(enabled: Boolean) {
        if (enabled) {
            window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        } else {
            window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        }
    }
}
