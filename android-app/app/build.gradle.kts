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

import com.google.protobuf.gradle.proto
import java.util.Properties

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.kapt)
    alias(libs.plugins.kotlin.plugin.serialization)
    alias(libs.plugins.kotlin.plugin.parcelize)
    alias(libs.plugins.protobuf)
}

android {
    namespace = "io.github.mkckr0.audio_share_app"
    compileSdk = 35

    defaultConfig {
        applicationId = "io.github.mkckr0.audio_share_app"
        minSdk = 23
        targetSdk = 35
        versionCode = 2001
        versionName = "0.2.1"
        base.archivesName = "${rootProject.name}-$versionName"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    signingConfigs {
        create("release") {
            val keystoreProperties = Properties().apply {
                load(rootProject.file("keystore.properties").inputStream())
            }
            storeFile = file(keystoreProperties.getProperty("storeFile"))
            keyAlias = keystoreProperties.getProperty("keyAlias")
            storePassword = keystoreProperties.getProperty("storePassword")
            keyPassword = keystoreProperties.getProperty("keyPassword")
            enableV3Signing = true
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            signingConfig = signingConfigs["release"]
        }
    }

    kotlin {
        jvmToolchain(17)
    }

    buildFeatures {
        viewBinding = true
        dataBinding = true
        buildConfig = true
    }

    sourceSets {
        getByName("main") {
            proto {
                srcDir("../../protos")
            }
        }
    }

    packaging {
        resources {
            merges += "META-INF/INDEX.LIST"
            merges += "META-INF/io.netty.versions.properties"
        }
    }

    dependenciesInfo {
        includeInApk = false
        includeInBundle = false
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.navigation.fragment.ktx)
    implementation(libs.androidx.navigation.ui.ktx)
    implementation(libs.androidx.preference)
    implementation(libs.androidx.work.runtime.ktx)
    implementation(libs.material)
    implementation(libs.google.protobuf.kotlin.lite)
    implementation(libs.netty.all)
    implementation(libs.ktor.client.android)
    implementation(libs.ktor.client.content.negotiation)
    implementation(libs.ktor.serialization.kotlinx.json)

    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
}

protobuf {
    protoc {
        artifact = "com.google.protobuf:protoc:${libs.versions.protobufVersion.get()}"
    }
    generateProtoTasks {
        all().forEach { task ->
            task.builtins {
                create("java")  {
                    option("lite")
                }
            }
        }
    }
}
