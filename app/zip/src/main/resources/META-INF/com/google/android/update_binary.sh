#!/sbin/sh

TMPDIR=/dev/tmp
rm -rf $TMPDIR 2>/dev/null
mkdir -p $TMPDIR 2>/dev/null

export BBBIN
BBBIN=$TMPDIR/busybox
unzip -o "$3" lib/x86/libbusybox.so lib/armeabi-v7a/libbusybox.so -d $TMPDIR >&2
chmod -R 755 $TMPDIR/lib
mv -f $TMPDIR/lib/x86/libbusybox.so $BBBIN
$BBBIN >/dev/null 2>&1 || mv -f $TMPDIR/lib/armeabi-v7a/libbusybox.so $BBBIN
$BBBIN rm -rf $TMPDIR/lib

export INSTALLER=$TMPDIR/install
$BBBIN mkdir -p $INSTALLER
$BBBIN unzip -o "$3" "assets/*" "lib/*" "META-INF/com/google/*" -x "lib/*/libbusybox.so" -d $INSTALLER >&2

SCRIPT="$INSTALLER/META-INF/com/google/android"
if [ -f "$SCRIPT/updater-script" ]; then
  exec $BBBIN sh -o standalone "$SCRIPT/updater-script" "$@"
else
  exec $BBBIN sh -o standalone "$SCRIPT/uninstaller-script" "$@"
fi

exit
