import org.apache.tools.ant.filters.FixCrLfFilter

plugins {
    id("java-library")
}

java {
    sourceCompatibility = JavaVersion.VERSION_1_8
    targetCompatibility = JavaVersion.VERSION_1_8
}

val syncLibs by tasks.registering(Sync::class) {
    into("src/main/resources/lib")
    into("armeabi-v7a") {
        from(rootProject.file("native/out/armeabi-v7a")) {
            include("busybox", "magiskboot", "magiskinit", "magiskinit64")
            rename { "lib$it.so" }
        }
    }
    into("x86") {
        from(rootProject.file("native/out/x86")) {
            include("busybox", "magiskboot", "magiskinit", "magiskinit64")
            rename { "lib$it.so" }
        }
    }
    doFirst {
        if (inputs.sourceFiles.files.size != 8)
            throw StopExecutionException("Build binary files first")
    }
}

tasks.processResources {
    dependsOn(syncLibs)
    inputs.property("version", Config.magiskVersion)
    inputs.property("versionCode", Config.magiskVersionCode)
    val path = "META-INF/com/google/android"
    filesMatching("$path/update_binary.sh") { name = "update-binary" }
    filesMatching("$path/flash_script.sh") { name = "updater-script" }
    filesMatching("$path/magisk_uninstaller.sh") { name = "uninstaller-script" }
    filesMatching("assets/util_functions.sh") {
        filter {
            it.replace("#MAGISK_VERSION_STUB",
                "MAGISK_VER=\"${Config.magiskVersion}\"\n" +
                    "MAGISK_VER_CODE=${Config.magiskVersionCode}")
        }
        // Fix gradle filter auto change line ending to CRLF on Windows
        filter<FixCrLfFilter>("eol" to FixCrLfFilter.CrLf.newInstance("lf"))
    }
}
