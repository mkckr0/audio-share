// Top-level build file where you can add configuration options common to all sub-projects/modules.

plugins {
    val kotlinVersion = "1.8.20"

    id("com.android.application") version "8.1.0" apply false
    kotlin("android") version kotlinVersion apply false
    kotlin("kapt") version kotlinVersion apply false
    id("com.google.protobuf") version "0.9.4" apply false
}
