package com.layouter.aimodeling

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.ui.graphics.Color

private val StudioBlue = Color(0xFF2563EB)

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        val dark = getSharedPreferences("app_theme", MODE_PRIVATE).getBoolean("dark", true)
        val scheme = if (dark) darkColorScheme(
            primary = StudioBlue,
            onPrimary = Color.White,
            background = Color(0xFF000000),
            onBackground = Color.White,
            surface = Color(0xFF090909),
            onSurface = Color(0xFFF5F5F5),
            surfaceVariant = Color(0xFF171717),
            onSurfaceVariant = Color(0xFFBDBDBD),
            secondary = StudioBlue
        ) else lightColorScheme(
            primary = StudioBlue,
            onPrimary = Color.White,
            background = Color(0xFFFFFFFF),
            onBackground = Color(0xFF111111),
            surface = Color(0xFFFFFFFF),
            onSurface = Color(0xFF111111),
            surfaceVariant = Color(0xFFF3F4F6),
            onSurfaceVariant = Color(0xFF4B5563),
            secondary = StudioBlue
        )
        setContent {
            MaterialTheme(colorScheme = scheme) {
                EditorScreen()
            }
        }
    }
}
