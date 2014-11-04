adb remount
adb push disp/disp.ko /vendor/modules/disp.ko
adb shell chmod 600 /vendor/modules/disp.ko
adb push lcd/lcd.ko /vendor/modules/lcd.ko
adb shell chmod 600 /vendor/modules/lcd.ko
adb push hdmi/hdcp.ko /vendor/modules/hdcp.ko
adb shell chmod 600 /vendor/modules/hdcp.ko
adb shell sync
echo press any key to reboot
pause
adb shell reboot