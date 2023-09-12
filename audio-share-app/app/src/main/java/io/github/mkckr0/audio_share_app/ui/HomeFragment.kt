package io.github.mkckr0.audio_share_app.ui

import android.content.Context
import android.content.SharedPreferences
import android.media.AudioManager
import android.os.Bundle
import android.os.PowerManager
import android.text.method.ScrollingMovementMethod
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.appcompat.content.res.AppCompatResources
import androidx.fragment.app.Fragment
import com.google.android.material.slider.Slider
import com.google.android.material.snackbar.Snackbar
import io.github.mkckr0.audio_share_app.NetClient
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.databinding.FragmentHomeBinding

class HomeFragment : Fragment() {

    private val tag = javaClass.simpleName

    private lateinit var binding: FragmentHomeBinding

    private lateinit var audioManager : AudioManager
    private lateinit var powerManager : PowerManager
    private lateinit var sharedPreferences : SharedPreferences
    private lateinit var netClient : NetClient

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Log.d(tag, "onCreate")

        binding = FragmentHomeBinding.inflate(layoutInflater)

        audioManager = requireContext().getSystemService(Context.AUDIO_SERVICE) as AudioManager
        powerManager = requireContext().getSystemService(Context.POWER_SERVICE) as PowerManager
        sharedPreferences = requireActivity().getPreferences(Context.MODE_PRIVATE)
        netClient = NetClient(NetClientHandler())


        binding.sliderWorkVolume.valueFrom = audioManager.getStreamMinVolume(AudioManager.STREAM_MUSIC).toFloat()
        binding.sliderWorkVolume.valueTo = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC).toFloat()
        binding.sliderWorkVolume.value = sharedPreferences.getInt(
            "work_volume",
            audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC) / 2
        ).toFloat()
        binding.sliderWorkVolume.addOnSliderTouchListener(object : Slider.OnSliderTouchListener {
            override fun onStartTrackingTouch(slider: Slider) {
            }

            override fun onStopTrackingTouch(slider: Slider) {
                sharedPreferences.edit().putInt("work_volume", slider.value.toInt()).apply()
            }
        })
        binding.sliderWorkVolume.addOnChangeListener { _, value, _ ->
            if (netClient.isPlaying()) {
                audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, value.toInt(), 0)
            }
        }

        binding.sliderIdleVolume.valueFrom = binding.sliderWorkVolume.valueFrom
        binding.sliderIdleVolume.valueTo = binding.sliderWorkVolume.valueTo
        binding.sliderIdleVolume.value = sharedPreferences.getInt(
            "idle_volume",
            audioManager.getStreamVolume(AudioManager.STREAM_MUSIC)
        ).toFloat()
        binding.sliderIdleVolume.addOnSliderTouchListener(object : Slider.OnSliderTouchListener {
            override fun onStartTrackingTouch(slider: Slider) {
            }

            override fun onStopTrackingTouch(slider: Slider) {
                sharedPreferences.edit().putInt("idle_volume", slider.value.toInt()).apply()
            }
        })
        binding.sliderIdleVolume.addOnChangeListener { _, value, _ ->
            if (!netClient.isPlaying()) {
                audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, value.toInt(), 0)
            }
        }

        binding.buttonStart.setOnClickListener { onButtonStartClick() }

        binding.textViewInfo.movementMethod = ScrollingMovementMethod()

        binding.textFieldHost.setText(sharedPreferences.getString("host", getString(R.string.default_host)))
        binding.textFieldPort.setText(sharedPreferences.getString("port", getString(R.string.default_port)))
    }

//    override fun onStart() {
//        super.onStart()
//        Log.d(tag, "onStart")
//    }
//
//    override fun onResume() {
//        super.onResume()
//        Log.d(tag, "onResume")
//    }
//
//    override fun onPause() {
//        super.onPause()
//        Log.d(tag, "onPause")
//    }
//
//    override fun onStop() {
//        super.onStop()
//        Log.d(tag, "onStop")
//    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        Log.d(tag, "onCreateView")
        return binding.root
    }

    private fun onButtonStartClick() {
        if (netClient.isPlaying()) {
            netClient.stop()
        } else {
            if (binding.textFieldHost.text.isNullOrBlank()  || binding.textFieldPort.text.isNullOrBlank()) {
                Snackbar.make(requireView(), "Please enter host and port", Toast.LENGTH_LONG)
                    .show()
                return
            }

            // save host and port
            sharedPreferences.edit()
                .putString("host", binding.textFieldHost.text.toString())
                .putString("port", binding.textFieldPort.text.toString())
                .apply()

            binding.textViewInfo.text = getString(R.string.audio_starting)
            switchEnableState(false)

            Log.d("AudioShare", "click start")

            netClient.start(binding.textFieldHost.text.toString(), binding.textFieldPort.text.toString().toInt())
        }
    }

    private fun switchEnableState(b: Boolean) {
        binding.textFieldHost.isEnabled = b
        binding.textFieldPort.isEnabled = b
        binding.buttonStart.icon = if (b) {
            AppCompatResources.getDrawable(requireContext(), R.drawable.baseline_play_circle)
        } else {
            AppCompatResources.getDrawable(requireContext(), R.drawable.baseline_pause_circle)
        }
    }

    inner class NetClientHandler : NetClient.Handler {
        override fun onNetError(e: String) {
            binding.textViewInfo.text = e
            switchEnableState(true)
        }

        override fun onAudioStop() {
            switchEnableState(true)
            binding.textViewInfo.text = getString(R.string.audio_stopped)
            audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, binding.sliderIdleVolume.value.toInt(), 0)
        }

        override fun onAudioStart() {
            binding.textViewInfo.text = getString(R.string.audio_started)
            audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, binding.sliderWorkVolume.value.toInt(), 0)
        }
    }
}