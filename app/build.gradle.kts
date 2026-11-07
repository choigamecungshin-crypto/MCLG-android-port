plugins { id("com.android.application") }

android {
    namespace = "dev.mc.legacy"
    compileSdk = 35

    defaultConfig {
        applicationId = "dev.mc.legacy"
        minSdk = 26
        targetSdk = 35
        versionCode = 2
        versionName = "0.2.0-dev"
        ndk { abiFilters += listOf("arm64-v8a") }
        externalNativeBuild {
            cmake { cppFlags += listOf("-std=c++17", "-Wall", "-Wextra") }
        }
    }

    externalNativeBuild {
        cmake { path = file("src/main/cpp/CMakeLists.txt"); version = "3.22.1" }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    signingConfigs {
        create("release") {
            val storeFilePath = System.getenv("MCLG_KEYSTORE_PATH")
            if (!storeFilePath.isNullOrBlank()) {
                storeFile = file(storeFilePath)
                storePassword = System.getenv("MCLG_STORE_PASSWORD")
                keyAlias = System.getenv("MCLG_KEY_ALIAS")
                keyPassword = System.getenv("MCLG_KEY_PASSWORD")
            }
        }
    }

    buildTypes {
        debug { isDebuggable = true }
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("release")
        }
    }
}
