package com.navblerelay

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.WindowManager
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.Spinner
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.appcompat.widget.SwitchCompat

class SettingsActivity : AppCompatActivity() {

    companion object {
        private const val PREFS_NAME = "nav_ble_prefs"
        private const val KEY_LOG_DETAIL = "log_detail"
        private const val KEY_KEEP_SCREEN_ON = "keep_screen_on"
        private const val KEY_AUTO_START = "auto_start"
        private const val KEY_THEME_MODE = "theme_mode"

        const val THEME_SYSTEM = 0
        const val THEME_LIGHT = 1
        const val THEME_DARK = 2

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

        fun getThemeMode(context: Context): Int {
            return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .getInt(KEY_THEME_MODE, THEME_SYSTEM)
        }

        fun applyTheme(context: Context) {
            AppCompatDelegate.setDefaultNightMode(when (getThemeMode(context)) {
                THEME_LIGHT -> AppCompatDelegate.MODE_NIGHT_NO
                THEME_DARK -> AppCompatDelegate.MODE_NIGHT_YES
                else -> AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
            })
        }
    }

    private lateinit var switchLogDetail: SwitchCompat
    private lateinit var switchKeepScreenOn: SwitchCompat
    private lateinit var switchAutoStart: SwitchCompat
    private lateinit var spinnerTheme: Spinner
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
        spinnerTheme = findViewById(R.id.spinner_theme)
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

        val themeLabels = listOf(
            getString(R.string.theme_system),
            getString(R.string.theme_light),
            getString(R.string.theme_dark)
        )
        spinnerTheme.adapter = ArrayAdapter(this, android.R.layout.simple_spinner_item, themeLabels)
            .apply { setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item) }
        spinnerTheme.setSelection(prefs.getInt(KEY_THEME_MODE, THEME_SYSTEM))
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
        spinnerTheme.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: android.view.View?, position: Int, id: Long) {
                val current = prefs.getInt(KEY_THEME_MODE, THEME_SYSTEM)
                if (current == position) return
                prefs.edit().putInt(KEY_THEME_MODE, position).apply()
                applyTheme(this@SettingsActivity)
                Toast.makeText(this@SettingsActivity, "主题已切换，重启应用后生效", Toast.LENGTH_SHORT).show()
            }
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
        btnReset.setOnClickListener {
            prefs.edit().clear().apply()
            loadPrefs()
            applyTheme(this)
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
