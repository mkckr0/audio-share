// Top-level build file where you can add configuration options common to all sub-projects/modules.

plugins {
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.kotlin.android) apply false
    alias(libs.plugins.kotlin.kapt) apply false
    alias(libs.plugins.kotlin.plugin.serialization) apply false
    alias(libs.plugins.kotlin.plugin.parcelize) apply false
    alias(libs.plugins.protobuf) apply false
}
