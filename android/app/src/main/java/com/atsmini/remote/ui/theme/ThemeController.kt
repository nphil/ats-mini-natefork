package com.atsmini.remote.ui.theme

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/** Persists the user's theme choice and Material You preference. */
object ThemeController {
    private const val PREFS = "atsmini.theme"
    private const val KEY_THEME = "theme"
    private const val KEY_DYNAMIC = "dynamic"

    private lateinit var prefs: android.content.SharedPreferences

    private val _theme = MutableStateFlow(AppTheme.NIGHT)
    val theme: StateFlow<AppTheme> = _theme.asStateFlow()

    private val _dynamic = MutableStateFlow(false)
    val dynamic: StateFlow<Boolean> = _dynamic.asStateFlow()

    fun init(context: Context) {
        prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        _theme.value = AppTheme.fromName(prefs.getString(KEY_THEME, null))
        _dynamic.value = prefs.getBoolean(KEY_DYNAMIC, false)
    }

    fun setTheme(theme: AppTheme) {
        _theme.value = theme
        prefs.edit().putString(KEY_THEME, theme.name).apply()
    }

    fun setDynamic(enabled: Boolean) {
        _dynamic.value = enabled
        prefs.edit().putBoolean(KEY_DYNAMIC, enabled).apply()
    }
}
