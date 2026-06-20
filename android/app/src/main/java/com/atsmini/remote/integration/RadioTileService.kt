package com.atsmini.remote.integration

import android.service.quicksettings.Tile
import android.service.quicksettings.TileService
import com.atsmini.remote.data.Protocol
import com.atsmini.remote.data.RadioRepository

/** Quick Settings tile: tap to toggle the radio's sleep mode from the shade. */
class RadioTileService : TileService() {

    override fun onStartListening() {
        super.onStartListening()
        updateTile()
    }

    override fun onClick() {
        super.onClick()
        if (RadioRepository.isConnected) {
            RadioRepository.send(Protocol.sleep(true))
        }
        updateTile()
    }

    private fun updateTile() {
        val tile = qsTile ?: return
        val connected = RadioRepository.isConnected
        tile.state = if (connected) Tile.STATE_ACTIVE else Tile.STATE_INACTIVE
        tile.subtitle = if (connected) {
            val s = RadioRepository.status.value
            "${s.formattedFrequency(com.atsmini.remote.data.FreqUnit.AUTO)} ${s.frequencyUnitLabel(com.atsmini.remote.data.FreqUnit.AUTO)}"
        } else "Disconnected"
        tile.updateTile()
    }
}
