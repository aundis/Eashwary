sudo security unlock-keychain login.keychain

APP=$1
PROFILEPATH=$2
IDENTITY=$3
/usr/bin/security cms -D -i $2 > provision.plist
/usr/libexec/PlistBuddy -x -c 'Print :Entitlements' provision.plist > entitlements.plist
cp -vr $2  $1/embedded.mobileprovision
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1/Frameworks/WebDriverAgentLib.framework
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1/Frameworks/XCTest.framework
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1/Frameworks/XCTAutomationSupport.framework
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1/Frameworks/*/*/*
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1/PlugIns/*
/usr/bin/codesign -f -v -s $3 --entitlements entitlements.plist $1
