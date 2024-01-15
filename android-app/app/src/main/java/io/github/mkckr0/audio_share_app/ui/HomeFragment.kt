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

package io.github.mkckr0.audio_share_app.ui

import android.animation.Animator
import android.animation.Animator.AnimatorListener
import android.animation.AnimatorListenerAdapter
import android.animation.AnimatorSet
import android.animation.ObjectAnimator
import android.animation.ValueAnimator
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.graphics.Color
import android.os.Bundle
import android.text.method.ScrollingMovementMethod
import android.util.Log
import android.util.TypedValue
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.animation.AnimationSet
import android.widget.EditText
import android.widget.TextView
import androidx.core.content.ContextCompat.getSystemService
import androidx.core.content.res.ResourcesCompat.ThemeCompat
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.Fragment
import androidx.fragment.app.viewModels
import androidx.preference.PreferenceManager
import com.google.android.material.slider.Slider
import com.google.android.material.snackbar.Snackbar
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.databinding.FragmentHomeBinding
import io.github.mkckr0.audio_share_app.model.HomeViewModel
import kotlinx.coroutines.NonCancellable.start

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

        binding.sliderAudioVolume.addOnChangeListener { _, value, _ ->
            viewModel.onAudioVolumeChange(value)
        }
        binding.sliderAudioVolume.addOnSliderTouchListener(object : Slider.OnSliderTouchListener{
            override fun onStartTrackingTouch(slider: Slider) {}
            override fun onStopTrackingTouch(slider: Slider) { viewModel.saveAudioVolume() }
        })

        viewModel.hostError.observe(viewLifecycleOwner) { binding.textFieldHostLayout.error = it }
        viewModel.portError.observe(viewLifecycleOwner) { binding.textFieldPortLayout.error = it }
        binding.buttonStart.setOnClickListener { viewModel.switchPlay() }
        binding.textViewInfo.setOnLongClickListener {
            val sharedPreferences = PreferenceManager.getDefaultSharedPreferences(requireContext())
            if (!sharedPreferences.getBoolean("debug_enable_copy_exception", resources.getBoolean(R.bool.debug_enable_copy_exception))) {
                return@setOnLongClickListener false
            }
            ValueAnimator.ofInt(0).apply {
                duration = 100
                addListener(object : AnimatorListenerAdapter() {
                    override fun onAnimationStart(animation: Animator) {
                        val typedValue = TypedValue()
                        this@HomeFragment.requireContext().theme.resolveAttribute(com.google.android.material.R.attr.colorSurfaceVariant, typedValue, true)
                        binding.textViewInfo.setBackgroundColor(typedValue.data)
                        val clipboard = requireContext().getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                        clipboard.setPrimaryClip(ClipData.newPlainText("label", (it as TextView).text))
                        Snackbar.make(requireView(), "The Exception Message has been copied.", Snackbar.LENGTH_SHORT).show()
                    }
                    override fun onAnimationEnd(animation: Animator) {
                        binding.textViewInfo.setBackgroundColor(0)
                    }
                })
                start()
            }
            return@setOnLongClickListener true
        }
        binding.textFieldHost.doOnTextChanged { _, _, _, _ -> viewModel.hostError.value = null }
        binding.textFieldPort.doOnTextChanged { _, _, _, _ -> viewModel.portError.value = null }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}