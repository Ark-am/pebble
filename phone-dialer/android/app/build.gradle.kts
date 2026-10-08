plugins {
    id("com.android.application")
}

android {
    namespace = "com.arkam.pebbledialer"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.arkam.pebbledialer"
        minSdk = 24
        targetSdk = 36
        versionCode = 4
        versionName = "1.3.0"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}

dependencies {
    // 1.3.x currently requires Android API 37; 1.2.0 supports compileSdk 36.
    implementation("io.rebble.pebblekit2:client-java:1.2.0")
    testImplementation("junit:junit:4.13.2")
}
