#!/bin/bash
#sudo apt-get install  android-tools


#adb shell input keyevent KEYCODE_APP_SWITCH
#adb shell input keyevent 22
#adb shell input keyevent 19
#adb shell input keyevent KEYCODE_DEL

echo "The version of the connected device is" 
adb shell getprop ro.build.version.release

echo "The API of the device is " 
adb shell getprop ro.build.version.sdk

echo "The total RAM of the device is "
cat /proc/meminfo | grep MemTotal

echo "The physical size of the device is " 
adb shell wm size


 
for file in *.apk ;
do 

  adb install -r $file;
  #sudo apt install aapt;
 
var=$(aapt dump badging *.apk | awk -v FS="'" '/package: name=/{print $2}')

adb shell monkey -p $var 1


#adb shell input keyevent 61 
adb shell input keyevent 66
#adb shell input keyevent 61 
adb shell input keyevent 66

sleep 2

adb shell input keyevent 66

#adb shell input keyevent 66

echo "enter the username  "

adb shell input keyevent 19
read name 
adb shell input text $name


echo "enter the pin"


read pin
adb shell input keyevent 66
adb shell input text $pin


adb shell input keyevent 22
sleep 2

adb shell input keyevent 66

adb shell input keyevent 20
adb shell input keyevent 66
adb shell input keyevent 20
adb shell input keyevent 66
adb shell input keyevent 20
adb shell input keyevent 66
adb shell input keyevent 20
adb shell input keyevent 66
adb shell input keyevent 20
adb shell input keyevent 66
adb shell input keyevent 20
adb shell input keyevent 66

done




