package io.github.mkckr0.audio_share_app

import android.content.Intent
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.github.mkckr0.audio_share_app.service.BootService

import org.junit.Test
import org.junit.runner.RunWith

import org.junit.Assert.*

/**
 * Instrumented test, which will execute on an Android device.
 *
 * See [testing documentation](http://d.android.com/tools/testing).
 */
@RunWith(AndroidJUnit4::class)
class ExampleInstrumentedTest {
    @Test
    fun useAppContext() {
        // Context of the app under test.
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        assertEquals("io.github.mkckr0.audio_share_app", appContext.packageName)
    }

    @Test
    fun testBootService() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        appContext.startService(Intent(appContext, BootService::class.java))
    }
}