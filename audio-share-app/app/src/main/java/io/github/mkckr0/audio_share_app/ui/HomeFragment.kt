/**
 *    Copyright 2022-2023 mkckr0 <https://github.com/mkckr0>
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

    private var _binding: FragmentHomeBinding? = null
    private val binding get() = _binding!!
    private val viewModel: HomeViewModel by viewModels()

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        Log.d(tag, "onCreateView")
        _binding = FragmentHomeBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
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
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    inner class VolumeSliderTouchListener : Slider.OnSliderTouchListener {
        override fun onStartTrackingTouch(slider: Slider) {}
        override fun onStopTrackingTouch(slider: Slider) { viewModel.saveVolume() }
    }
}