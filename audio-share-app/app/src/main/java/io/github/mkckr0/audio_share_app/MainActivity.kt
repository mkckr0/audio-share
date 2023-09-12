package io.github.mkckr0.audio_share_app

import android.annotation.SuppressLint
import android.graphics.Color
import android.os.Bundle
import android.util.Log
import androidx.appcompat.app.AppCompatActivity
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.setupWithNavController
import com.google.android.material.color.DynamicColors
import com.google.android.material.color.DynamicColorsOptions
import io.github.mkckr0.audio_share_app.databinding.ActivityMainBinding


class MainActivity : AppCompatActivity() {

    private lateinit var binding : ActivityMainBinding

    @SuppressLint("BatteryLife")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)

        setContentView(binding.root)

        val navController = binding.navHostFragmentContainer.getFragment<NavHostFragment>().navController
        binding.bottomNavigation.setupWithNavController(navController)
    }

//    override fun onStart() {
//        super.onStart()
//        Log.d("lifecycle", "onStart")
//    }
//
//    override fun onResume() {
//        super.onResume()
//        Log.d("lifecycle", "onResume")
//    }
//
//    override fun onPause() {
//        super.onPause()
//        Log.d("lifecycle", "onPause")
//    }
//
//    override fun onStop() {
//        super.onStop()
//        Log.d("lifecycle", "onStop")
//    }
}
