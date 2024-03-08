/**
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

package io.github.mkckr0.audio_share_app

import android.Manifest
import android.app.DownloadManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.util.Log
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.setupWithNavController
import androidx.preference.PreferenceManager
import com.google.android.material.snackbar.Snackbar
import io.github.mkckr0.audio_share_app.databinding.ActivityMainBinding
import io.github.mkckr0.audio_share_app.model.Asset


class MainActivity : AppCompatActivity() {

    private val tag = MainActivity::class.simpleName

    private lateinit var binding: ActivityMainBinding
    private lateinit var requestPermissionLauncher: ActivityResultLauncher<String>
    private lateinit var downloadManager: DownloadManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        downloadManager = getSystemService(Context.DOWNLOAD_SERVICE) as DownloadManager

        val navController =
            binding.navHostFragmentContainer.getFragment<NavHostFragment>().navController
        binding.bottomNavigation.setupWithNavController(navController)

        requestPermissionLauncher =
            registerForActivityResult(ActivityResultContracts.RequestPermission()) { isGranted ->

            }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                requestPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
            }
        }
    }

    override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        if (intent == null) {
            return
        }
        if (intent.getStringExtra("action") == "update") {
            val apkAsset = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                intent.getParcelableExtra("apkAsset", Asset::class.java)!!
            } else {
                @Suppress("DEPRECATION") intent.getParcelableExtra("apkAsset")!!
            }

            val preferences = PreferenceManager.getDefaultSharedPreferences(application)
            var downloadId = preferences.getLong("update_download_id", -1)
            if (downloadManager.getUriForDownloadedFile(downloadId) != null) {
                val cursor =
                    downloadManager.query(DownloadManager.Query().setFilterById(downloadId))
                val index = cursor.getColumnIndex(DownloadManager.COLUMN_TITLE)
                if (cursor.moveToFirst() && cursor.getString(index) == apkAsset.name) {
                    @Suppress("NAME_SHADOWING") val intent = Intent(Intent.ACTION_VIEW)
                    intent.setDataAndType(
                        downloadManager.getUriForDownloadedFile(downloadId),
                        downloadManager.getMimeTypeForDownloadedFile(downloadId)
                    )
                    intent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                    startActivity(intent)
                }
                cursor.close()
            } else {
                downloadManager.remove(downloadId)
                preferences.edit().remove("update_download_id").apply()

                // Enqueue a new download
                val request =
                    DownloadManager.Request(Uri.parse(apkAsset.browserDownloadUrl)).apply {
                        setTitle(apkAsset.name)
                        setDestinationInExternalFilesDir(
                            this@MainActivity, null, apkAsset.name
                        )
                        setNotificationVisibility(
                            DownloadManager.Request.VISIBILITY_VISIBLE.or(
                                DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED
                            )
                        )
                    }
                downloadId = downloadManager.enqueue(request)
                preferences.edit().putLong("update_download_id", downloadId).apply()
                Log.d(tag, "download ${apkAsset.name} $downloadId")
            }
        }
    }
}
