import org.gradle.api.tasks.Sync

plugins {
    id("com.android.application")
}

val stagedAndroidAssets = layout.buildDirectory.dir("generated/androidAssets")
val stageAndroidAssets by tasks.registering(Sync::class) {
    from("../../../.generated/sdcard/pokegochi/pokegochi.pak") {
        into("pokegochi")
    }
    listOf("pack_version.txt", "animation_catalog.txt", "manifest.json").forEach { name ->
        from("../../../.generated/sdcard/pokegochi/assets/$name") {
            into("pokegochi/assets")
        }
    }
    into(stagedAndroidAssets)
}

android {
    namespace = "com.pokegochi.app"
    compileSdk = 36
    buildFeatures {
        buildConfig = true
    }

    defaultConfig {
        applicationId = "com.pokegochi.app"
        minSdk = 26
        targetSdk = 36
        versionCode = 52
        versionName = "1.0"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    ndkVersion = "27.3.13750724"
    // The firmware save ABI intentionally keeps 8-byte alignment. The
    // 32-bit NDK targets change that layout, so ship the native runtime
    // for current Android devices only.
    defaultConfig {
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    sourceSets {
        getByName("main") {
            // The Android runtime reads the canonical archive.  Stage only it
            // and the small metadata files; the generated pack also contains
            // thousands of loose PKG mirrors for the ESP32 SD card.
            assets.directories.add(stagedAndroidAssets.get().asFile.path)
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
}

tasks.named("preBuild") {
    dependsOn(stageAndroidAssets)
}
