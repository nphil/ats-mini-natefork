package com.atsmini.remote.ui.theme

import android.os.Build
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.compose.collectAsStateWithLifecycle

@Composable
fun ATSMiniTheme(content: @Composable () -> Unit) {
    val theme by ThemeController.theme.collectAsStateWithLifecycle()
    val dynamic by ThemeController.dynamic.collectAsStateWithLifecycle()
    val context = LocalContext.current
    val systemDark = isSystemInDarkTheme()

    val colors = if (dynamic && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        if (systemDark) dynamicDarkColorScheme(context) else dynamicLightColorScheme(context)
    } else {
        theme.colorScheme()
    }

    MaterialTheme(colorScheme = colors, content = content)
}
