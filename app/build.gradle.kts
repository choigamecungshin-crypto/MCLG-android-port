plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "dev.mc.legacy"
    compileSdk = 35

    defaultConfig {
        applicationId = "dev.mc.legacy"
        minSdk = 26
        targetSdk = 35
        versionCode = 6
        versionName = "0.4.2-kotlin-fix"
        ndk { abiFilters += listOf("arm64-v8a", "x86_64") }
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

kotlin {
    jvmToolchain(17)
}

// Package the complete original Legacy resource trees. The Android bootstrap
// copies them into app-private storage so the original engine can keep using
// its normal Common/res and Common/Media filesystem paths.
val copyLegacyGameResources by tasks.registering(Copy::class) {
    from(rootProject.file("Minecraft.Client/Common/res"))
    into(layout.projectDirectory.dir("src/main/assets/Common/res"))
}

val copyLegacyMedia by tasks.registering(Copy::class) {
    from(rootProject.file("Minecraft.Client/Common/Media"))
    into(layout.projectDirectory.dir("src/main/assets/Common/Media"))
}

// Reuse the original Minecraft Legacy artwork as the Android launcher icon.
val copyMclgLauncherIcon by tasks.registering(Copy::class) {
    from(rootProject.file("Minecraft.Client/Common/Media/Graphics/MinecraftIcon.png"))
    into(layout.projectDirectory.dir("src/main/res/drawable-nodpi"))
    rename { "mclg_icon.png" }
}

tasks.named("preBuild").configure {
    dependsOn(copyMclgLauncherIcon, copyLegacyGameResources, copyLegacyMedia)
}
