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

import com.google.protobuf.gradle.proto
import org.jetbrains.kotlin.gradle.plugin.mpp.pm20.util.archivesName
import java.util.Properties

plugins {
    id("com.android.application")
    kotlin("android")
    kotlin("kapt")
    id("com.google.protobuf")
}

android {
    namespace = "io.github.mkckr0.audio_share_app"
    compileSdk = 34

    defaultConfig {
        applicationId = "io.github.mkckr0.audio_share_app"
        minSdk = 23
        targetSdk = 34
        versionCode = 11
        versionName = "0.0.11"
        archivesName.set("${rootProject.name}-$versionName")
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
}

val protobufVersion = "3.24.3"

dependencies {

    val navVersion = "2.7.2"

    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.11.0-alpha02")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("com.google.protobuf:protobuf-kotlin-lite:$protobufVersion")
    implementation("io.netty:netty-all:4.1.100.Final")
    implementation("androidx.navigation:navigation-fragment-ktx:$navVersion")
    implementation("androidx.navigation:navigation-ui-ktx:$navVersion")
    implementation("androidx.preference:preference:1.2.1")

    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
}

protobuf {
    protoc {
        artifact = "com.google.protobuf:protoc:$protobufVersion"
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
