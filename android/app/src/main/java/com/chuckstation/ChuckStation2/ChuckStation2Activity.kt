package com.chuckstation.ChuckStation2

import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.hardware.input.InputManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.InputDevice
import android.view.InputEvent
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.Window
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.Toast

private const val TAG = "ChuckStation2"

class ChuckStation2Activity : Activity(), SurfaceHolder.Callback {

    private lateinit var surfaceView: SurfaceView
    private var nativeInitialized = false
    private val handler = Handler(Looper.getMainLooper())

    // Native methods
    private external fun nativeInit()
    private external fun nativePause()
    private external fun nativeResume()
    private external fun nativeDestroy()
    private external fun nativeGetVersion(): String
    private external fun nativeGetDeviceInfo(): String

    companion object {
        init {
            System.loadLibrary("ChuckStation2")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        Log.i(TAG, "ChuckStation2 Activity creating...")

        // Request fullscreen immersive mode
        requestWindowFeature(Window.FEATURE_NO_TITLE)
        window.setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        )
        window.setFlags(
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
        )

        // Create the surface view for Vulkan rendering
        surfaceView = SurfaceView(this).apply {
            holder.addCallback(this@ChuckStation2Activity)
        }

        setContentView(FrameLayout(this).apply {
            addView(surfaceView, FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            ))
        })

        setupImmersiveMode()
        checkVulkanSupport()
    }

    override fun onResume() {
        super.onResume()
        setupImmersiveMode()
        if (nativeInitialized) {
            try {
                nativeResume()
            } catch (e: UnsatisfiedLinkError) {
                Log.e(TAG, "nativeResume failed", e)
            }
        }
    }

    override fun onPause() {
        super.onPause()
        if (nativeInitialized) {
            try {
                nativePause()
            } catch (e: UnsatisfiedLinkError) {
                Log.e(TAG, "nativePause failed", e)
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        if (nativeInitialized) {
            try {
                nativeDestroy()
            } catch (e: UnsatisfiedLinkError) {
                Log.e(TAG, "nativeDestroy failed", e)
            }
        }
        nativeInitialized = false
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.i(TAG, "Surface created: ${holder.surfaceFrame.width()}x${holder.surfaceFrame.height()}")
        if (!nativeInitialized) {
            try {
                nativeInit()
                nativeInitialized = true
                Log.i(TAG, "ChuckStation2 native initialized successfully")
            } catch (e: UnsatisfiedLinkError) {
                Log.e(TAG, "Failed to load ChuckStation2 native library", e)
                handler.post {
                    Toast.makeText(this, "Failed to initialize ChuckStation2", Toast.LENGTH_LONG).show()
                }
            }
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.d(TAG, "Surface changed: ${width}x${height}, format=$format")
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.i(TAG, "Surface destroyed")
    }

    // Handle gamepad and touch input
    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        if (isGamepadEvent(event)) {
            // Let SDL handle gamepad input
            return super.onKeyDown(keyCode, event)
        }
        return super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        if (isGamepadEvent(event)) {
            return super.onKeyUp(keyCode, event)
        }
        return super.onKeyUp(keyCode, event)
    }

    override fun onGenericMotionEvent(event: MotionEvent?): Boolean {
        if (event != null && isGamepadEvent(event)) {
            // Pass gamepad joystick/trigger events to SDL
            return super.onGenericMotionEvent(event)
        }
        return super.onGenericMotionEvent(event)
    }

    private fun isGamepadEvent(event: InputEvent?): Boolean {
        if (event == null || event.source == 0) return false
        return (event.source and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
               (event.source and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
    }

    @Suppress("DEPRECATION")
    private fun setupImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(false)
            window.insetsController?.let { controller ->
                controller.hide(WindowInsets.Type.statusBars() or WindowInsets.Type.navigationBars())
                controller.systemBarsBehavior =
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                or View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            )
        }
    }

    private fun checkVulkanSupport() {
        val hasVulkan = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            packageManager.hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_LEVEL, 1)
        } else {
            false
        }

        if (!hasVulkan) {
            handler.post {
                Toast.makeText(
                    this,
                    "Warning: Vulkan may not be fully supported on this device",
                    Toast.LENGTH_LONG
                ).show()
            }
        }
    }
}