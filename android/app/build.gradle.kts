plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.kernetile.studio"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.kernetile.studio"
        minSdk = 24
        targetSdk = 34
        versionCode = (project.findProperty("versionCode") as String?)?.toIntOrNull() ?: 1
        versionName = (project.findProperty("versionName") as String?) ?: "0.01"

        externalNativeBuild {
            cmake {
                cppFlags += listOf(
                    "-std=c++20",
                    "-ffunction-sections",
                    "-fdata-sections",
                    "-fvisibility=hidden",
                    "-fvisibility-inlines-hidden"
                )
                arguments += listOf(
                    "-DANDROID_STL=c++_static",
                    "-DBUILD_TESTING=OFF"
                )
                targets += listOf("ktui_gallery")
            }
        }
    }

    // 按 ABI 拆分独立 APK，仅保留主流 ARM64 (arm64-v8a) 与老设备 ARM32 (armeabi-v7a)
    splits {
        abi {
            isEnable = true
            reset()
            include("arm64-v8a", "armeabi-v7a")
            isUniversalApk = false
        }
    }

    signingConfigs {
        create("release") {
            val ksFilePath = System.getenv("ANDROID_KEYSTORE_FILE")
            if (!ksFilePath.isNullOrBlank() && file(ksFilePath).exists()) {
                storeFile = file(ksFilePath)
                storePassword = System.getenv("ANDROID_KEYSTORE_PASSWORD")?.trim()
                keyAlias = System.getenv("ANDROID_KEY_ALIAS")?.trim()
                keyPassword = System.getenv("ANDROID_KEY_PASSWORD")?.trim()
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            val releaseKs = System.getenv("ANDROID_KEYSTORE_FILE")
            signingConfig = if (!releaseKs.isNullOrBlank() && file(releaseKs).exists()) {
                signingConfigs.getByName("release")
            } else {
                signingConfigs.getByName("debug")
            }
        }
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }

    lint {
        checkReleaseBuilds = false
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
}
