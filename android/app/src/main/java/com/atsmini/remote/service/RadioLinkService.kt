package com.atsmini.remote.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import com.atsmini.remote.MainActivity
import com.atsmini.remote.R
import com.atsmini.remote.data.FreqUnit
import com.atsmini.remote.data.RadioRepository
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

/**
 * Keeps the radio link alive in the background and shows a media-style
 * notification with the current frequency / RDS station — something an
 * iOS BLE companion app can't sustain.
 */
class RadioLinkService : Service() {

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    private var collectJob: Job? = null

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        createChannel()
        val started = runCatching {
            startForeground(NOTIF_ID, buildNotification("ATS-Mini", "Connecting…"))
        }.isSuccess
        if (!started) { stopSelf(); return START_NOT_STICKY }
        collectJob?.cancel()
        collectJob = scope.launch {
            RadioRepository.status.collectLatest { s ->
                val title = if (s.rdsStation.isNotEmpty()) s.rdsStation else s.bandName
                val text = "${s.formattedFrequency(FreqUnit.AUTO)} ${s.frequencyUnitLabel(FreqUnit.AUTO)} · ${s.modeName}"
                notificationManager().notify(NOTIF_ID, buildNotification(title, text))
            }
        }
        return START_STICKY
    }

    override fun onDestroy() {
        scope.cancel()
        super.onDestroy()
    }

    private fun buildNotification(title: String, text: String): Notification {
        val open = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        val builder = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
            Notification.Builder(this, CHANNEL_ID) else @Suppress("DEPRECATION") Notification.Builder(this)
        return builder
            .setContentTitle(title)
            .setContentText(text)
            .setSmallIcon(R.drawable.ic_tile_radio)
            .setContentIntent(open)
            .setOngoing(true)
            .build()
    }

    private fun createChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(CHANNEL_ID, "Radio link", NotificationManager.IMPORTANCE_LOW)
            notificationManager().createNotificationChannel(channel)
        }
    }

    private fun notificationManager() =
        getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

    companion object {
        private const val CHANNEL_ID = "radio_link"
        private const val NOTIF_ID = 42

        fun start(context: Context) {
            val intent = Intent(context, RadioLinkService::class.java)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) context.startForegroundService(intent)
            else context.startService(intent)
        }
    }
}
