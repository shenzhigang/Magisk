# Magisk Lite

白名单模式的Magisk，所有应用默认隐藏，只有在Hide里勾选的应用才能获得超级用户权限。
adb shell自动具有超级用户权限。模块功能不可用。

`magiskhide add UID 包名` 添加超级用户应用
`magiskhide rm UID 包名` 移除超级用户应用
`magiskhide ls` 列出超级用户应用

## Magisk (3f9a6441-lite)
- 基于3f9a6441，相关修改参考上游更新日志
- 合并zip至apk。将apk的后缀改为zip即可在TWRP刷入。卸载包通过app的保存卸载包功能导出。如果愿意手动，删除apk里面的`META-INF/com/google/android/updater-script`，即成为卸载包。

## Magisk Manager (3f9a6441-lite)
- 基于3f9a6441，相关修改参考上游更新日志
- 正确处理来自magiskd的任何数据
- 添加不支持的环境检测：安装为系统应用或存在其它su
- 还原boot镜像后删除备份文件
- 合并zip至apk。
- 提升target API 到 Android 11
