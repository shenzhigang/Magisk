# alpha更新日志

## Magisk (7acfac6a-alpha)
- 基于 7acfac6a，相关修改参考上游更新日志
- 正确处理来自magiskd的任何数据
- 支持SharedUserId
- 添加不支持的环境检测：安装为系统应用、安装到外置存储或存在其它su
- 还原boot镜像后删除备份文件
- 内置当前版本更新日志
- 当无法下载stub时使用本地版本，现在Magisk可以完全离线使用

### 如何在Recovery使用APK文件？
一般情况建议通过Magisk应用来安装和卸载Magisk。
如果坚持使用自定义Recovery，将Magisk APK文件的`.apk`扩展名改为`.zip`即可刷入。
要卸载Magisk，zip文件名需要包含`uninstall`，例如将apk文件重命名为`uninstall.zip`。文件名不包含`uninstall`则会进行安装操作。

# 上游更新日志

## Magisk (0646f48e) (21407)

- [App] Support creating shortcuts on devices older than Android 8.0
- [App] Avoid Shell backed I/O for reliable installation

### How to Use the APK for Recoveries

In general, it is recommended to install and uninstall Magisk through the Magisk app.
However, if you insist to use custom recoveries, rename the Magisk APK's `.apk` file extension to `.zip`.
TWRP should then be able to directly flash the zip file.
If you have trouble renaming the file extension on PC, rename the file with TWRP's built-in file manager.
To uninstall in recovery, rename the zip file to `uninstall.zip` before flashing it.

## Diff from v21.4

- [General] Magisk and Magisk Manager is now merged!
- [App] Rename the app "Magisk Manager" to "Magisk"
- [App] Support hiding the Magisk app with advanced technique (stub APK loading) on Android 5.0+ (it used to be 9.0+)
- [App] Disallow re-packaging the Magisk app on devices lower than Android 5.0
- [MagiskHide] Fix a bug when stopping MagiskHide does not take effect
