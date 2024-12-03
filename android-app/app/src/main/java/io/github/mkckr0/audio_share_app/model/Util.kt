/*
 *    Copyright 2022-2024 mkckr0 <https://github.com/mkckr0>
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

package io.github.mkckr0.audio_share_app.model

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
