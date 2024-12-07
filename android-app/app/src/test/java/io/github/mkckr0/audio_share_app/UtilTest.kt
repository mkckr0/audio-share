package io.github.mkckr0.audio_share_app

import io.github.mkckr0.audio_share_app.model.Util
import org.junit.Assert.*

import org.junit.Test

class UtilTest {

    @Test
    fun isNewerVersion_arg0() {
        assertThrows(IllegalArgumentException::class.java) {
            Util.isNewerVersion("3.2.2", "v0.0.1")
        }
    }

    @Test
    fun isNewerVersion_arg1() {
        assertThrows(IllegalArgumentException::class.java) {
            Util.isNewerVersion("v3.2.", "v0.0.")
        }
    }

    @Test
    fun isNewerVersion_version0() {
        assertTrue(Util.isNewerVersion("v0.0.17", "v0.0.9"))
    }

    @Test
    fun isNewerVersion_version1() {
        assertTrue(Util.isNewerVersion("v0.1.0", "v0.0.17"))
    }

    @Test
    fun isNewerVersion_version11() {
        assertFalse(Util.isNewerVersion("v0.0.17", "v0.1.0"))
    }

    @Test
    fun isNewerVersion_version12() {
        assertFalse(Util.isNewerVersion("v0.1.0", "v0.1.0"))
    }

    @Test
    fun isNewerVersion_version2() {
        assertTrue(Util.isNewerVersion("v0.17.0", "v0.9.17"))
    }

    @Test
    fun isNewerVersion_version3() {
        assertTrue(Util.isNewerVersion("v12.17.0", "v1.0.0"))
    }

    @Test
    fun isNewerVersion_version4() {
        assertFalse(Util.isNewerVersion("v12.17.0", "v12.17.0"))
    }
}