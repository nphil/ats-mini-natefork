package com.atsmini.remote.ui.components

import android.view.HapticFeedbackConstants
import android.view.View

/** Haptics on interactive controls, matching the iOS app's feedback model. */
object Haptics {
    fun light(view: View) = view.performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP)
    fun medium(view: View) = view.performHapticFeedback(HapticFeedbackConstants.CONTEXT_CLICK)
    fun heavy(view: View) = view.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
}
