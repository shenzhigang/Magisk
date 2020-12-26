package com.topjohnwu.magisk.core.download

import android.content.Context
import android.net.Uri
import android.os.Parcelable
import androidx.core.net.toUri
import com.topjohnwu.magisk.BuildConfig
import com.topjohnwu.magisk.core.Info
import com.topjohnwu.magisk.core.model.ManagerJson
import com.topjohnwu.magisk.core.model.StubJson
import com.topjohnwu.magisk.core.model.module.OnlineModule
import com.topjohnwu.magisk.core.utils.MediaStoreUtils
import com.topjohnwu.magisk.ktx.cachedFile
import com.topjohnwu.magisk.ktx.get
import kotlinx.parcelize.IgnoredOnParcel
import kotlinx.parcelize.Parcelize

private fun cachedFile(name: String) = get<Context>().cachedFile(name).apply { delete() }.toUri()

sealed class Subject : Parcelable {

    abstract val file: Uri
    abstract val action: Action
    abstract val title: String

    @Parcelize
    class Module(
        val module: OnlineModule,
        override val action: Action
    ) : Subject() {
        val url: String get() = module.zip_url
        override val title: String get() = module.downloadFilename

        @IgnoredOnParcel
        override val file by lazy {
            MediaStoreUtils.getFile(title).uri
        }
    }

    @Parcelize
    class Manager(
        private val app: ManagerJson = Info.remote.app,
        val stub: StubJson = Info.remote.stub
    ) : Subject() {
        override val action get() = Action.Download
        override val title: String get() = "MagiskManager-${app.version}(${app.versionCode})"
        val url: String get() = app.link

        @IgnoredOnParcel
        override val file by lazy {
            cachedFile("manager.apk")
        }

    }

    abstract class Magisk : Subject() {

        @Parcelize
        private class Internal(
            override val action: Action
        ) : Magisk() {
            override val title: String
                get() = "Magisk-${BuildConfig.BUILDIN_MAGISK}(${BuildConfig.BUILDIN_MAGISK_CODE})"

            @IgnoredOnParcel
            override val file by lazy {
                cachedFile("magisk.zip")
            }
        }

        @Parcelize
        private class Uninstall : Magisk() {
            override val action get() = Action.Uninstall
            override val title: String get() = "uninstall.zip"

            @IgnoredOnParcel
            override val file by lazy {
                cachedFile(title)
            }
        }

        @Parcelize
        private class Download : Magisk() {
            override val action get() = Action.Download
            override val title: String
                get() = "Magisk-${BuildConfig.BUILDIN_MAGISK}(${BuildConfig.BUILDIN_MAGISK_CODE}).zip"

            @IgnoredOnParcel
            override val file by lazy {
                MediaStoreUtils.getFile(title).uri
            }
        }

        @Parcelize
        private class DownloadUninstaller : Magisk() {
            override val action get() = Action.DownloadUninstaller
            override val title: String
                get() = "MagiskUninstaller-${BuildConfig.BUILDIN_MAGISK}(${BuildConfig.BUILDIN_MAGISK_CODE}).zip"

            @IgnoredOnParcel
            override val file by lazy {
                MediaStoreUtils.getFile(title).uri
            }
        }

        companion object {
            operator fun invoke(config: Action) = when (config) {
                Action.Download -> Download()
                Action.DownloadUninstaller -> DownloadUninstaller()
                Action.Uninstall -> Uninstall()
                Action.EnvFix, is Action.Flash, is Action.Patch -> Internal(config)
            }
        }

    }

}
