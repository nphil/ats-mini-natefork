package com.atsmini.remote

import android.app.Application
import com.atsmini.remote.ble.BleManager
import com.atsmini.remote.data.RadioRepository
import com.atsmini.remote.integration.RadioWidgetProvider
import com.atsmini.remote.net.FirmwareCache
import com.atsmini.remote.service.RadioLinkService
import com.atsmini.remote.shizuku.ShizukuManager
import com.atsmini.remote.ui.theme.ThemeController
import com.atsmini.remote.usb.UsbSerialManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

/** Process-wide singletons shared by the UI, service, widget and tile. */
object Controllers {
    lateinit var ble: BleManager
        private set
    lateinit var usb: UsbSerialManager
        private set

    fun init(app: Application) {
        ble = BleManager(app)
        usb = UsbSerialManager(app)
    }
}

class AtsApp : Application() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)

    override fun onCreate() {
        super.onCreate()
        Controllers.init(this)
        ThemeController.init(this)
        ShizukuManager.init()
        FirmwareCache.init(this)

        // Start the foreground link service on connect; refresh widgets on any change.
        var wasConnected = false
        scope.launch {
            RadioRepository.status.collectLatest { s ->
                if (s.isConnected && !wasConnected) RadioLinkService.start(this@AtsApp)
                wasConnected = s.isConnected
                RadioWidgetProvider.refresh(this@AtsApp)
            }
        }
    }
}
