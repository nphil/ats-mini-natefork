plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "com.atsmini.remote"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.atsmini.remote"
        minSdk = 26
        targetSdk = 35
        versionCode = 14
        versionName = "1.0.13"
        vectorDrawables { useSupportLibrary = true }
    }

    // Stable keystore committed to the repo so Obtainium accepts in-place
    // updates (it verifies the APK signing certificate is unchanged).
    signingConfigs {
        create("release") {
            storeFile = file("atsmini-release.jks")
            storePassword = System.getenv("ATSMINI_STORE_PASSWORD") ?: "atsmini"
            keyAlias = System.getenv("ATSMINI_KEY_ALIAS") ?: "atsmini"
            keyPassword = System.getenv("ATSMINI_KEY_PASSWORD") ?: "atsmini"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("release")
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
        debug {
            signingConfig = signingConfigs.getByName("release")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }

    buildFeatures {
        compose = true
        buildConfig = true
    }
    lint {
        abortOnError = false
        checkReleaseBuilds = false
    }
    packaging {
        resources { excludes += "/META-INF/{AL2.0,LGPL2.1}" }
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(libs.androidx.activity.compose)

    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.graphics)
    implementation(libs.androidx.ui.tooling.preview)
    implementation(libs.androidx.material3)
    implementation(libs.androidx.material.icons.extended)
    implementation(libs.androidx.material3.adaptive.navigation.suite)

    implementation(libs.kotlinx.coroutines.android)
    implementation(libs.usb.serial)
    implementation(libs.shizuku.api)
    implementation(libs.shizuku.provider)

    debugImplementation(libs.androidx.ui.tooling)
}
