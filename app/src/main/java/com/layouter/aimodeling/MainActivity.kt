package com.layouter.aimodeling

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Viewport()
            }
        }
    }
}

@Composable
fun Viewport() {
    AndroidView(
        modifier = Modifier.fillMaxSize(),
        factory = { context -> ModelingSurfaceView(context) }
    )
}
