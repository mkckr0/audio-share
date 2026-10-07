package io.github.mkckr0.audio_share_app.service

class SequenceTracker {

    var lost = 0L
        private set
    var late = 0L
        private set

    private var last = -1

    // false for duplicates and anything older than the newest packet seen
    fun accept(seq: Int): Boolean {
        if (last < 0) {
            last = seq
            return true
        }
        val diff = (seq - last) and 0xffff
        if (diff == 0 || diff >= 0x8000) {
            late++
            return false
        }
        lost += diff - 1
        last = seq
        return true
    }
}
