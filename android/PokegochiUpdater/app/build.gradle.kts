plugins {
    id("com.android.application")
}

android {
    namespace = "com.pokegochi.updater"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.pokegochi.updater"
        minSdk = 26
        targetSdk = 36
        versionCode = 6
        versionName = "2.4"
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
}
