import org.gradle.api.tasks.Sync
import java.io.InputStream
import java.security.MessageDigest
import java.util.zip.ZipFile

plugins {
    id("com.android.application")
}

val stagedAndroidAssets = layout.buildDirectory.dir("generated/androidAssets")
val canonicalAssetRoot = file("../../../.generated/sdcard/pokegochi")
val canonicalAssetPack = canonicalAssetRoot.resolve("pokegochi.pak")
val canonicalAssetPackVersion = canonicalAssetRoot.resolve("assets/pack_version.txt")
val requiredAssetPackVersionHeader = file("../../../include/config/AssetPackVersion.h")
val battleAnimationAudit = file("../../../.generated/battle_animation_audit.json")
val generatedBattleAnimations = file("../../../src/game/BattleAnimationGenerated.inc")

fun sha256(stream: InputStream): String {
    val digest = MessageDigest.getInstance("SHA-256")
    val buffer = ByteArray(1024 * 1024)
    while (true) {
        val count = stream.read(buffer)
        if (count < 0) break
        digest.update(buffer, 0, count)
    }
    return digest.digest().joinToString("") { "%02x".format(it) }
}

val validateAndroidAssetCatalog by tasks.registering {
    inputs.file(battleAnimationAudit)
    inputs.file(generatedBattleAnimations)
    inputs.file(canonicalAssetRoot.resolve("assets/animation_catalog.txt"))
    inputs.file(canonicalAssetPack)
    inputs.file(canonicalAssetPackVersion)
    inputs.file(requiredAssetPackVersionHeader)
    doLast {
        val auditText = battleAnimationAudit.readText()
        val expectedCatalog = Regex("\\\"catalogHash\\\"\\s*:\\s*\\\"([0-9a-f]{16})\\\"")
            .find(auditText)?.groupValues?.get(1)
            ?: error("battle_animation_audit.json has no catalogHash")
        val compiledCatalog = Regex("kBattleAnimationCatalogId\\[\\]\\s*=\\s*\\\"([0-9a-f]{16})\\\"")
            .find(generatedBattleAnimations.readText())?.groupValues?.get(1)
            ?: error("BattleAnimationGenerated.inc has no catalog identifier")
        val packagedCatalog = canonicalAssetRoot.resolve("assets/animation_catalog.txt")
            .readText().trim()
        check(expectedCatalog == compiledCatalog) {
            "Generated battle animation code ($compiledCatalog) does not match its audit ($expectedCatalog)"
        }
        check(expectedCatalog == packagedCatalog) {
            "Android asset catalog ($packagedCatalog) does not match the native runtime ($expectedCatalog). " +
                "Run build_sd_asset_pack.py --runtime-metadata-only before packaging."
        }
        val requiredPackVersion = Regex("kRequiredAssetPackVersion\\s*=\\s*(\\d+)")
            .find(requiredAssetPackVersionHeader.readText())?.groupValues?.get(1)?.toInt()
            ?: error("AssetPackVersion.h does not define kRequiredAssetPackVersion")
        val packagedVersion = canonicalAssetPackVersion.readText().trim().toInt()
        check(requiredPackVersion == packagedVersion) {
            "Android asset pack version ($packagedVersion) does not match native runtime ($requiredPackVersion)."
        }
        canonicalAssetPack.inputStream().use { source ->
            val header = ByteArray(10)
            check(source.read(header) == header.size) { "Android asset archive header is truncated" }
            val archiveVersion = (header[8].toInt() and 0xff) or
                ((header[9].toInt() and 0xff) shl 8)
            check(archiveVersion == requiredPackVersion) {
                "Android asset archive version ($archiveVersion) does not match native runtime ($requiredPackVersion)."
            }
        }
    }
}
val stageAndroidAssets by tasks.registering(Sync::class) {
    dependsOn(validateAndroidAssetCatalog)
    // The pack is generated outside Gradle. Always restage it so an old
    // build/generated copy can never be silently reused in a new APK.
    outputs.upToDateWhen { false }
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
        versionCode = 87
        versionName = "2.0"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    val bundledNdk = file("../../../.tools/android-ndk-r27d")
    ndkVersion = "27.3.13750724"
    if (bundledNdk.isDirectory) {
        // Release workspaces carry a pinned NDK so builds do not depend on
        // enumerating or downloading SDK-side installations.
        ndkPath = bundledNdk.absolutePath
    }
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

fun registerApkAssetPackVerifier(buildType: String) {
    val variant = buildType.replaceFirstChar { it.uppercase() }
    tasks.register("verify${variant}ApkAssetPack") {
        dependsOn("assemble$variant")
        inputs.file(canonicalAssetPack)
        inputs.file(canonicalAssetPackVersion)
        doLast {
            val outputDir = layout.buildDirectory.dir("outputs/apk/$buildType").get().asFile
            val preferredNames = if (buildType == "release") {
                listOf("app-release-unsigned.apk", "app-release.apk")
            } else {
                listOf("app-debug.apk")
            }
            val apk = preferredNames.asSequence()
                .map { outputDir.resolve(it) }
                .firstOrNull { it.isFile }
                ?: error("No $buildType APK found in ${outputDir.absolutePath}")
            val expectedLength = canonicalAssetPack.length()
            val expectedHash = canonicalAssetPack.inputStream().use(::sha256)
            val expectedVersion = canonicalAssetPackVersion.readText().trim()
            ZipFile(apk).use { zip ->
                val packEntry = zip.getEntry("assets/pokegochi/pokegochi.pak")
                    ?: error("APK does not contain pokegochi.pak")
                check(packEntry.size == expectedLength) {
                    "APK asset pack is stale: ${packEntry.size} bytes, expected $expectedLength"
                }
                val packagedHash = zip.getInputStream(packEntry).use(::sha256)
                check(packagedHash == expectedHash) {
                    "APK asset pack hash $packagedHash does not match canonical $expectedHash"
                }
                val versionEntry = zip.getEntry("assets/pokegochi/assets/pack_version.txt")
                    ?: error("APK does not contain pack_version.txt")
                val packagedVersion = zip.getInputStream(versionEntry).bufferedReader().use { it.readText().trim() }
                check(packagedVersion == expectedVersion) {
                    "APK pack version $packagedVersion does not match canonical $expectedVersion"
                }
            }
            logger.lifecycle(
                "Verified $buildType APK asset pack: version=$expectedVersion " +
                    "bytes=$expectedLength sha256=$expectedHash"
            )
        }
    }
}

registerApkAssetPackVerifier("debug")
registerApkAssetPackVerifier("release")
