package com.atsmini.remote.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Build
import androidx.compose.material.icons.filled.GraphicEq
import androidx.compose.material.icons.filled.Radio
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.Icon
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.adaptive.navigationsuite.NavigationSuiteScaffold
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.dp

private enum class Dest(val label: String, val icon: ImageVector) {
    RADIO("Radio", Icons.Filled.Radio),
    VISUALIZE("Visualize", Icons.Filled.GraphicEq),
    TOOLS("Tools", Icons.Filled.Build),
    SETTINGS("Settings", Icons.Filled.Settings),
}

@Composable
fun AppRoot(onRequestUsb: () -> Unit) {
    var dest by remember { mutableStateOf(Dest.RADIO) }

    NavigationSuiteScaffold(
        navigationSuiteItems = {
            Dest.entries.forEach { d ->
                item(
                    selected = dest == d,
                    onClick = { dest = d },
                    icon = { Icon(d.icon, contentDescription = d.label) },
                    label = { Text(d.label) },
                )
            }
        },
    ) {
        // The Surface paints edge-to-edge (its background fills behind the status
        // bar); the inner Box insets content so nothing overlaps the status bar.
        Surface(Modifier.fillMaxSize()) {
            Box(Modifier.fillMaxSize().statusBarsPadding()) {
                when (dest) {
                    Dest.RADIO -> RadioPane()
                    Dest.VISUALIZE -> VisualizeScreen()
                    Dest.TOOLS -> ToolsScreen(onRequestUsb = onRequestUsb)
                    Dest.SETTINGS -> SettingsScreen()
                }
            }
        }
    }
}

/** Two-pane on tablets/foldables (controls beside the spectrum), single column on phones. */
@Composable
private fun RadioPane() {
    BoxWithConstraints(Modifier.fillMaxSize()) {
        if (maxWidth >= 720.dp) {
            Row(Modifier.fillMaxSize()) {
                Box(Modifier.weight(1f).fillMaxHeight()) { RadioScreen() }
                Box(Modifier.weight(1f).fillMaxHeight()) { VisualizeScreen() }
            }
        } else {
            RadioScreen()
        }
    }
}
