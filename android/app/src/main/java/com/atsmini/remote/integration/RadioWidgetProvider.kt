package com.atsmini.remote.integration

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.widget.RemoteViews
import com.atsmini.remote.MainActivity
import com.atsmini.remote.R
import com.atsmini.remote.data.FreqUnit
import com.atsmini.remote.data.RadioRepository

/** Home-screen widget showing live frequency / signal at a glance. */
class RadioWidgetProvider : AppWidgetProvider() {

    override fun onUpdate(context: Context, manager: AppWidgetManager, ids: IntArray) {
        ids.forEach { id -> manager.updateAppWidget(id, buildViews(context)) }
    }

    companion object {
        /** Push a fresh snapshot into all placed widgets. */
        fun refresh(context: Context) {
            val manager = AppWidgetManager.getInstance(context)
            val ids = manager.getAppWidgetIds(ComponentName(context, RadioWidgetProvider::class.java))
            if (ids.isEmpty()) return
            val views = buildViews(context)
            ids.forEach { manager.updateAppWidget(it, views) }
        }

        private fun buildViews(context: Context): RemoteViews {
            val s = RadioRepository.status.value
            val views = RemoteViews(context.packageName, R.layout.widget_radio)
            views.setTextViewText(R.id.widget_band, if (s.isConnected) s.bandName else "ATS-Mini")
            views.setTextViewText(
                R.id.widget_freq,
                "${s.formattedFrequency(FreqUnit.AUTO)} ${s.frequencyUnitLabel(FreqUnit.AUTO)}",
            )
            views.setTextViewText(
                R.id.widget_signal,
                if (s.isConnected) "${s.modeName} · RSSI ${s.rssi}" else "Disconnected",
            )
            val pi = PendingIntent.getActivity(
                context, 0, Intent(context, MainActivity::class.java),
                PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
            )
            views.setOnClickPendingIntent(R.id.widget_root, pi)
            return views
        }
    }
}
