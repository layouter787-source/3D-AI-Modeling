package com.layouter.aimodeling

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.graphics.Color

private val StudioBlue = Color(0xFF2563EB)
private val StudioBlueDark = Color(0xFF60A5FA)

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        val preferences = getSharedPreferences("studio_preferences", MODE_PRIVATE)
        setContent {
            var darkTheme by remember { mutableStateOf(preferences.getBoolean("dark_theme", true)) }
            SideEffect { NativeBridge.setViewportTheme(darkTheme) }
            val colors = if (darkTheme) darkColorScheme(
                primary = StudioBlueDark,
                onPrimary = Color(0xFF07111F),
                secondary = StudioBlueDark,
                background = Color(0xFF000000),
                surface = Color(0xFF101010),
                surfaceVariant = Color(0xFF191919),
                onBackground = Color(0xFFFFFFFF),
                onSurface = Color(0xFFFFFFFF),
                onSurfaceVariant = Color(0xFFB8B8B8)
            ) else lightColorScheme(
                primary = StudioBlue,
                onPrimary = Color.White,
                secondary = StudioBlue,
                background = Color(0xFFFFFFFF),
                surface = Color(0xFFFFFFFF),
                surfaceVariant = Color(0xFFF3F4F6),
                onBackground = Color(0xFF000000),
                onSurface = Color(0xFF000000),
                onSurfaceVariant = Color(0xFF525252)
            )
            MaterialTheme(colorScheme = colors) {
                EditorScreen(
                    isDarkTheme = darkTheme,
                    onToggleTheme = {
                        darkTheme = !darkTheme
                        preferences.edit().putBoolean("dark_theme", darkTheme).apply()
                    }
                )
            }
        }
    }
}
