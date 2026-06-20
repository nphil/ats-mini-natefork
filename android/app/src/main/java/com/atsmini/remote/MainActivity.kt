package com.atsmini.remote

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import com.atsmini.remote.shizuku.ShizukuManager
import com.atsmini.remote.ui.AppRoot
import com.atsmini.remote.ui.theme.ATSMiniTheme

class MainActivity : ComponentActivity() {

    companion object {
        const val ACTION_USB_PERMISSION = "com.atsmini.remote.USB_PERMISSION"
    }

    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { /* best effort */ }

    private val usbReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                ACTION_USB_PERMISSION ->
                    if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                        Controllers.usb.open()
                    }
                UsbManager.ACTION_USB_DEVICE_ATTACHED -> Controllers.usb.refresh()
                UsbManager.ACTION_USB_DEVICE_DETACHED -> Controllers.usb.close()
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        requestRuntimePermissions()
        registerUsbReceiver()
        ShizukuManager.refresh()

        setContent {
            ATSMiniTheme {
                AppRoot(onRequestUsb = ::connectUsb)
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        runCatching { unregisterReceiver(usbReceiver) }
    }

    private fun requestRuntimePermissions() {
        val perms = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            perms += android.Manifest.permission.BLUETOOTH_SCAN
            perms += android.Manifest.permission.BLUETOOTH_CONNECT
        } else {
            perms += android.Manifest.permission.ACCESS_FINE_LOCATION
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            perms += android.Manifest.permission.POST_NOTIFICATIONS
        }
        permissionLauncher.launch(perms.toTypedArray())
    }

    private fun registerUsbReceiver() {
        val filter = IntentFilter().apply {
            addAction(ACTION_USB_PERMISSION)
            addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(usbReceiver, filter, Context.RECEIVER_NOT_EXPORTED)
        } else {
            @Suppress("UnspecifiedRegisterReceiverFlag")
            registerReceiver(usbReceiver, filter)
        }
    }

    /** Open the USB serial link, requesting device permission if needed. */
    private fun connectUsb() {
        Controllers.usb.refresh()
        val usbManager = getSystemService(Context.USB_SERVICE) as UsbManager
        val device: UsbDevice = Controllers.usb.attached.value ?: run {
            com.atsmini.remote.data.RadioRepository.log("No USB device attached")
            return
        }
        if (usbManager.hasPermission(device)) {
            Controllers.usb.open()
        } else {
            val flags = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
                PendingIntent.FLAG_MUTABLE else 0
            val pi = PendingIntent.getBroadcast(
                this, 0, Intent(ACTION_USB_PERMISSION).setPackage(packageName), flags
            )
            Controllers.usb.requestPermission(device, pi)
        }
    }
}
