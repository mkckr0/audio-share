package io.github.mkckr0.audio_share_app

object Util {
    fun isNewerVersion(a: String, b: String): Boolean {
        if (a[0] != 'v' || b[0] != 'v') {
            throw IllegalArgumentException("version format error, a=${a} b=${b}")
        }
        val aList = a.substring(1).split('.').map { it.toInt() }
        val bList = b.substring(1).split('.').map { it.toInt() }
        if (aList.size != 3 || bList.size != 3) {
            throw IllegalArgumentException("version format error, a=${a} b=${b}")
        }
        aList.forEachIndexed { index, i ->
            if (i == bList[index]) {
                return@forEachIndexed
            } else {
                return i > bList[index]
            }
        }
        return false
    }
}