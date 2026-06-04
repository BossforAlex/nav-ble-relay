package com.navblerelay

import android.app.Application
import android.util.Log

class NavBleApp : Application() {
    companion object {
        private const val TAG = "NavBleApp"
    }

    override fun onCreate() {
        super.onCreate()
        Log.i(TAG, "Application initialized")
    }
}