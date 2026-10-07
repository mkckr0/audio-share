package io.github.mkckr0.audio_share_app

import io.github.mkckr0.audio_share_app.service.SequenceTracker
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SequenceTrackerTest {

    @Test
    fun inOrder_losesNothing() {
        val t = SequenceTracker()
        for (i in 0 until 100) assertTrue(t.accept(i))
        assertEquals(0, t.lost)
        assertEquals(0, t.late)
    }

    @Test
    fun gap_countsLostPackets() {
        val t = SequenceTracker()
        assertTrue(t.accept(10))
        assertTrue(t.accept(14))
        assertEquals(3, t.lost)
    }

    @Test
    fun duplicateAndLate_areDropped() {
        val t = SequenceTracker()
        assertTrue(t.accept(5))
        assertTrue(t.accept(6))
        assertFalse(t.accept(6))
        assertFalse(t.accept(4))
        assertEquals(2, t.late)
        assertTrue(t.accept(7))
    }

    @Test
    fun wrapAround_isInOrder() {
        val t = SequenceTracker()
        assertTrue(t.accept(0xfffe))
        assertTrue(t.accept(0xffff))
        assertTrue(t.accept(0))
        assertTrue(t.accept(1))
        assertEquals(0, t.lost)
        assertFalse(t.accept(0xffff))
    }

    @Test
    fun firstPacket_canStartAnywhere() {
        val t = SequenceTracker()
        assertTrue(t.accept(40000))
        assertEquals(0, t.lost)
    }
}
