
APP=$1
PROFILEPATH=$2
IDENTITY=$3
echo 'm1mac' | sudo -S  security unlock-keychain -p 'm1mac' login.keychain
/usr/bin/security cms -D -i $2 > provision.plist
/usr/libexec/PlistBuddy -x -c 'Print :Entitlements' provision.plist > entitlements.plist
cp -vr $2  $1/embedded.mobileprovision
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1/Frameworks/XCUIAutomation.framework
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1/Frameworks/WebDriverAgentLib.framework
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1/Frameworks/XCTest.framework
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1/Frameworks/XCTAutomationSupport.framework
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1/Frameworks/XCTestCore.framework
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1/Frameworks/*/*/*
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1/PlugIns/*
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1/Frameworks/XCUnit.framework
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1

