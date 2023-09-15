package io.github.mkckr0.audio_share_app.ui

import android.os.Bundle
import android.text.method.ScrollingMovementMethod
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.Fragment
import androidx.fragment.app.viewModels
import com.google.android.material.slider.Slider
import io.github.mkckr0.audio_share_app.databinding.FragmentHomeBinding
import io.github.mkckr0.audio_share_app.model.HomeViewModel

class HomeFragment : Fragment() {

    private val tag = javaClass.simpleName

    private val viewModel: HomeViewModel by viewModels()

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        Log.d(tag, "onCreateView")

        val binding = FragmentHomeBinding.inflate(inflater, container, false)
        binding.viewModel = viewModel
        binding.lifecycleOwner = viewLifecycleOwner

        binding.sliderWorkVolume.addOnSliderTouchListener(VolumeSliderTouchListener())
        binding.sliderWorkVolume.addOnChangeListener { _, value, _ ->
            viewModel.onWorkVolumeChange(value.toInt())
        }
        binding.sliderIdleVolume.addOnSliderTouchListener(VolumeSliderTouchListener())
        binding.sliderIdleVolume.addOnChangeListener { _, value, _ ->
            viewModel.onIdleVolumeChange(value.toInt())
        }

        viewModel.hostError.observe(viewLifecycleOwner) { binding.textFieldHostLayout.error = it }
        viewModel.portError.observe(viewLifecycleOwner) { binding.textFieldPortLayout.error = it }
        binding.buttonStart.setOnClickListener { viewModel.startPlay() }
        binding.textViewInfo.movementMethod = ScrollingMovementMethod()
        binding.textFieldHost.doOnTextChanged { _, _, _, _ -> viewModel.hostError.value = null }
        binding.textFieldPort.doOnTextChanged { _, _, _, _ -> viewModel.portError.value = null }

        return binding.root
    }

    inner class VolumeSliderTouchListener : Slider.OnSliderTouchListener {
        override fun onStartTrackingTouch(slider: Slider) {}
        override fun onStopTrackingTouch(slider: Slider) { viewModel.saveVolume() }
    }
}