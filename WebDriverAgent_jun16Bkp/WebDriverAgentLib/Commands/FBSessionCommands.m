/**
 * Copyright (c) 2015-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */
#include <ifaddrs.h>
#include <arpa/inet.h>

#import "FBSessionCommands.h"

#import "FBApplication.h"
#import "FBConfiguration.h"
#import "FBRouteRequest.h"
#import "FBSession.h"
#import "FBApplication.h"
#import "FBRuntimeUtils.h"
#import "XCUIDevice.h"
#import "XCUIDevice+FBHealthCheck.h"
#import "XCUIDevice+FBHelpers.h"
#import "XCUIApplicationProcessDelay.h"
#import "FBFindElementCommands.h"
#import "FBMacros.h"

static NSString* const USE_COMPACT_RESPONSES = @"shouldUseCompactResponses";
static NSString* const ELEMENT_RESPONSE_ATTRIBUTES = @"elementResponseAttributes";
static NSString* const MJPEG_SERVER_SCREENSHOT_QUALITY = @"mjpegServerScreenshotQuality";
static NSString* const MJPEG_SERVER_FRAMERATE = @"mjpegServerFramerate";
static NSString* const MJPEG_SCALING_FACTOR = @"mjpegScalingFactor";
static NSString* const MJPEG_COMPRESSION_FACTOR = @"mjpegCompressionFactor";
static NSString* const SCREENSHOT_QUALITY = @"screenshotQuality";

@implementation FBSessionCommands

#pragma mark - <FBCommandHandler>

+ (NSArray *)routes
{
  return
  @[
    [[FBRoute POST:@"/url"] respondWithTarget:self action:@selector(handleOpenURL:)],
    [[FBRoute POST:@"/session"].withoutSession respondWithTarget:self action:@selector(handleCreateSession:)],
    [[FBRoute POST:@"/setPassCode"].withoutSession respondWithTarget:self action:@selector(handleSetPassCode:)],
    [[FBRoute POST:@"/resetPassCode"].withoutSession respondWithTarget:self action:@selector(handleResetPassCode:)],
    [[FBRoute POST:@"/setProxy"].withoutSession respondWithTarget:self action:@selector(handleSetProxy:)],
    [[FBRoute POST:@"/resetProxy"].withoutSession respondWithTarget:self action:@selector(handleResetProxy:)],
    [[FBRoute POST:@"/cleanSafari"].withoutSession respondWithTarget:self action:@selector(handleCleanSafari:)],
    [[FBRoute GET:@"/wifiOn"].withoutSession respondWithTarget:self action:@selector(handleWifiOn:)],
    [[FBRoute GET:@"/wifiOff"].withoutSession respondWithTarget:self action:@selector(handleWifiOff:)],
    [[FBRoute POST:@"/connectvpn"].withoutSession respondWithTarget:self action:@selector(handleConnectVPN:)],
    [[FBRoute POST:@"/launchapp"].withoutSession respondWithTarget:self action:@selector(handleLaunchApp:)],
    [[FBRoute POST:@"/vpnip"].withoutSession respondWithTarget:self action:@selector(handleVPNIP:)],
    [[FBRoute POST:@"/disconnectvpn"].withoutSession respondWithTarget:self action:@selector(handleDisConnectVPN:)],
    [[FBRoute POST:@"/wda/apps/launch"] respondWithTarget:self action:@selector(handleSessionAppLaunch:)],
    [[FBRoute POST:@"/wda/apps/activate"] respondWithTarget:self action:@selector(handleSessionAppActivate:)],
    [[FBRoute POST:@"/wda/apps/terminate"] respondWithTarget:self action:@selector(handleSessionAppTerminate:)],
    [[FBRoute POST:@"/wda/apps/state"] respondWithTarget:self action:@selector(handleSessionAppState:)],
    [[FBRoute GET:@""] respondWithTarget:self action:@selector(handleGetActiveSession:)],
    [[FBRoute DELETE:@""] respondWithTarget:self action:@selector(handleDeleteSession:)],
    [[FBRoute GET:@"/status"].withoutSession respondWithTarget:self action:@selector(handleGetStatus:)],
    
    // Health check might modify simulator state so it should only be called in-between testing sessions
    [[FBRoute GET:@"/wda/healthcheck"].withoutSession respondWithTarget:self action:@selector(handleGetHealthCheck:)],
    
    // Settings endpoints
    [[FBRoute GET:@"/appium/settings"] respondWithTarget:self action:@selector(handleGetSettings:)],
    [[FBRoute POST:@"/appium/settings"] respondWithTarget:self action:@selector(handleSetSettings:)],
    [[FBRoute POST:@"/wda/disconnectCall"] respondWithTarget:self action:@selector(handleDisconnectCall:)],
    ];
}


#pragma mark - Commands

+ (id<FBResponsePayload>)handleDisconnectCall:(FBRouteRequest *)request
{
  FBApplication *app = request.session.activeApplication;
  [app.buttons[@"Decline"] tap];
  return FBResponseWithOK();
}

+ (id<FBResponsePayload>)handleOpenURL:(FBRouteRequest *)request
{
  NSString *urlString = request.arguments[@"url"];
  if (!urlString) {
    return FBResponseWithStatus(FBCommandStatusInvalidArgument, @"URL is required");
  }
  NSError *error;
  if (![XCUIDevice.sharedDevice fb_openUrl:urlString error:&error]) {
    return FBResponseWithError(error);
  }
  return FBResponseWithOK();
}

+ (id<FBResponsePayload>)handleCreateSession:(FBRouteRequest *)request
{
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *bundleID = requirements[@"bundleId"];
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithErrorFormat(@"'bundleId' desired capability not provided");
  }
  [FBConfiguration setShouldUseTestManagerForVisibilityDetection:[requirements[@"shouldUseTestManagerForVisibilityDetection"] boolValue]];
  if (requirements[@"shouldUseCompactResponses"]) {
    [FBConfiguration setShouldUseCompactResponses:[requirements[@"shouldUseCompactResponses"] boolValue]];
  }
  NSString *elementResponseAttributes = requirements[@"elementResponseAttributes"];
  if (elementResponseAttributes) {
    [FBConfiguration setElementResponseAttributes:elementResponseAttributes];
  }
  if (requirements[@"maxTypingFrequency"]) {
    [FBConfiguration setMaxTypingFrequency:[requirements[@"maxTypingFrequency"] unsignedIntegerValue]];
  }
  if (requirements[@"shouldUseSingletonTestManager"]) {
    [FBConfiguration setShouldUseSingletonTestManager:[requirements[@"shouldUseSingletonTestManager"] boolValue]];
  }
  NSNumber *delay = requirements[@"eventloopIdleDelaySec"];
  if ([delay doubleValue] > 0.0) {
    [XCUIApplicationProcessDelay setEventLoopHasIdledDelay:[delay doubleValue]];
  } else {
    [XCUIApplicationProcessDelay disableEventLoopDelay];
  }
  
  [FBConfiguration setShouldWaitForQuiescence:[requirements[@"shouldWaitForQuiescence"] boolValue]];
  
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];
  
  if (app.processID == 0) {
    return FBResponseWithErrorFormat(@"Failed to launch %@ application", bundleID);
  }
//  if (requirements[@"defaultAlertAction"]) {
//    [FBSession sessionWithApplication:app defaultAlertAction:(id)requirements[@"defaultAlertAction"]];
//  } else {
//    [FBSession sessionWithApplication:app];
//  }
  
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
}

+ (id<FBResponsePayload>)handleSetPassCode:(FBRouteRequest *)request
{
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *passCode = request.arguments[@"passCode"];
  NSString *bundleID = @"com.apple.Preferences";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithErrorFormat(@"'bundleId' desired capability not provided");
  }
  if (!passCode) {
    return FBResponseWithErrorFormat(@"'passCode' desired capability not provided");
  }
  /*[FBConfiguration setShouldUseTestManagerForVisibilityDetection:[requirements[@"shouldUseTestManagerForVisibilityDetection"] boolValue]];
   if (requirements[@"shouldUseCompactResponses"]) {
   [FBConfiguration setShouldUseCompactResponses:[requirements[@"shouldUseCompactResponses"] boolValue]];
   }
   NSString *elementResponseAttributes = requirements[@"elementResponseAttributes"];
   if (elementResponseAttributes) {
   [FBConfiguration setElementResponseAttributes:elementResponseAttributes];
   }
   if (requirements[@"maxTypingFrequency"]) {
   [FBConfiguration setMaxTypingFrequency:[requirements[@"maxTypingFrequency"] unsignedIntegerValue]];
   }
   if (requirements[@"shouldUseSingletonTestManager"]) {
   [FBConfiguration setShouldUseSingletonTestManager:[requirements[@"shouldUseSingletonTestManager"] boolValue]];
   }
   NSNumber *delay = requirements[@"eventloopIdleDelaySec"];
   if ([delay doubleValue] > 0.0) {
   [XCUIApplicationProcessDelay setEventLoopHasIdledDelay:[delay doubleValue]];
   } else {
   [XCUIApplicationProcessDelay disableEventLoopDelay];
   }
   
   [FBConfiguration setShouldWaitForQuiescence:[requirements[@"shouldWaitForQuiescence"] boolValue]];
   */
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];
  if(app.tables.cells.staticTexts[@"Touch ID & Passcode"].enabled){
    [app.tables.cells.staticTexts[@"Touch ID & Passcode"] tap];
    if(app.navigationBars[@"Enter Passcode"].enabled)
      return FBResponseWithObject(@"Passcode already set");
     [NSThread sleepForTimeInterval:1.0];
    if(app.tables.cells.staticTexts[@"Turn Passcode On"].enabled){
      [app.tables.cells.staticTexts[@"Turn Passcode On"] tap];
      //[app.otherElements[@"Passcode"] tap];
      //[app.otherElements[@"Passcode"] typeText:passCode];
      [NSThread sleepForTimeInterval:1.0];
      if(app.alerts.buttons[@"Keep"].enabled)
        [app.alerts.buttons[@"Keep"] tap];
      for (NSUInteger charIdx=0; charIdx<passCode.length; charIdx++)
      { [NSThread sleepForTimeInterval:1.0];
        NSString *key = [passCode substringWithRange:NSMakeRange(charIdx, 1)];
        [app.keyboards.keys[key] tap];
      }
      
      [NSThread sleepForTimeInterval:1.0];
      for (NSUInteger charIdx=0; charIdx<passCode.length; charIdx++)
      {
        [NSThread sleepForTimeInterval:1.0];
        NSString *key = [passCode substringWithRange:NSMakeRange(charIdx, 1)];
        [app.keyboards.keys[key] tap];
      }
      
    }
  }
  else if(app.tables.cells.staticTexts[@"Face ID & Passcode"].enabled){
    [app.tables.cells.staticTexts[@"Face ID & Passcode"] tap];
    if(app.tables.cells.staticTexts[@"Turn Passcode On"].enabled){
      [NSThread sleepForTimeInterval:1.0];
      [app.tables.cells.staticTexts[@"Turn Passcode On"] tap];
      [NSThread sleepForTimeInterval:1.0];
      if(app.alerts.buttons[@"Keep"].enabled)
        [app.alerts.buttons[@"Keep"] tap];
      for (NSUInteger charIdx=0; charIdx<passCode.length; charIdx++)
      {
        [NSThread sleepForTimeInterval:1.0];
        NSString *key = [passCode substringWithRange:NSMakeRange(charIdx, 1)];
        [app.keyboards.keys[key] tap];
      }
      [NSThread sleepForTimeInterval:1.0];
      for (NSUInteger charIdx=0; charIdx<passCode.length; charIdx++)
      {
        [NSThread sleepForTimeInterval:1.0];
        NSString *key = [passCode substringWithRange:NSMakeRange(charIdx, 1)];
        [app.keyboards.keys[key] tap];
      }
    }
  }
  else{
    return FBResponseWithErrorFormat(@"Failed to set Passcode");
  }
  
  if (app.processID == 0) {
    return FBResponseWithErrorFormat(@"Failed to launch %@ application", bundleID);
  }
  /*if (requirements[@"defaultAlertAction"]) {
   [FBSession sessionWithApplication:app defaultAlertAction:(id)requirements[@"defaultAlertAction"]];
   } else {
   [FBSession sessionWithApplication:app];
   }*/
  NSError *error;
  if (![[XCUIDevice sharedDevice] fb_goToHomescreenWithError:&error]) {
    return FBResponseWithError(error);
  }
  
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
}

+ (id<FBResponsePayload>)handleResetPassCode:(FBRouteRequest *)request
{
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *passCode = request.arguments[@"passCode"];
  NSString *bundleID = @"com.apple.Preferences";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithErrorFormat(@"'bundleId' desired capability not provided");
  }
  if (!passCode) {
    return FBResponseWithErrorFormat(@"'passCode' desired capability not provided");
  }
  /*[FBConfiguration setShouldUseTestManagerForVisibilityDetection:[requirements[@"shouldUseTestManagerForVisibilityDetection"] boolValue]];
   if (requirements[@"shouldUseCompactResponses"]) {
   [FBConfiguration setShouldUseCompactResponses:[requirements[@"shouldUseCompactResponses"] boolValue]];
   }
   NSString *elementResponseAttributes = requirements[@"elementResponseAttributes"];
   if (elementResponseAttributes) {
   [FBConfiguration setElementResponseAttributes:elementResponseAttributes];
   }
   if (requirements[@"maxTypingFrequency"]) {
   [FBConfiguration setMaxTypingFrequency:[requirements[@"maxTypingFrequency"] unsignedIntegerValue]];
   }
   if (requirements[@"shouldUseSingletonTestManager"]) {
   [FBConfiguration setShouldUseSingletonTestManager:[requirements[@"shouldUseSingletonTestManager"] boolValue]];
   }
   NSNumber *delay = requirements[@"eventloopIdleDelaySec"];
   if ([delay doubleValue] > 0.0) {
   [XCUIApplicationProcessDelay setEventLoopHasIdledDelay:[delay doubleValue]];
   } else {
   [XCUIApplicationProcessDelay disableEventLoopDelay];
   }
   
   [FBConfiguration setShouldWaitForQuiescence:[requirements[@"shouldWaitForQuiescence"] boolValue]];
   */
  
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];
  if(app.tables.cells.staticTexts[@"Touch ID & Passcode"].enabled)
  {
    [app.tables.cells.staticTexts[@"Touch ID & Passcode"] tap];
    [NSThread sleepForTimeInterval:2.0];
    if(app.tables.cells.staticTexts[@"Turn Passcode On"].enabled){
      return FBResponseWithObject(@"Passcode already reset");
    }
    
    [NSThread sleepForTimeInterval:1.0];
    
   
    for (NSUInteger charIdx=0; charIdx<passCode.length; charIdx++)
    {
      [NSThread sleepForTimeInterval:1.0];
      NSString *key = [passCode substringWithRange:NSMakeRange(charIdx, 1)];
      [app.keyboards.keys[key] tap];
    }
    [NSThread sleepForTimeInterval:1.0];
    [app.tables.cells.staticTexts[@"Turn Passcode Off"] tap];
    [NSThread sleepForTimeInterval:1.0];
    
    if(app.alerts.buttons[@"Turn Off"].enabled)
      [app.alerts.buttons[@"Turn Off"] tap];
    
    for (NSUInteger charIdx=0; charIdx<passCode.length; charIdx++)
    {
      [NSThread sleepForTimeInterval:1.0];
      NSString *key = [passCode substringWithRange:NSMakeRange(charIdx, 1)];
      [app.keyboards.keys[key] tap];
    }
     [NSThread sleepForTimeInterval:1.0];
  }
  else if(app.tables.cells.staticTexts[@"Face ID & Passcode"].enabled){
    [app.tables.cells.staticTexts[@"Face ID & Passcode"] tap];
    for (NSUInteger charIdx=0; charIdx<passCode.length; charIdx++)
    {
      [NSThread sleepForTimeInterval:1.0];
      NSString *key = [passCode substringWithRange:NSMakeRange(charIdx, 1)];
      [app.keyboards.keys[key] tap];
    }
    [NSThread sleepForTimeInterval:1.0];
    [app.tables.cells.staticTexts[@"Turn Passcode Off"] tap];
    [NSThread sleepForTimeInterval:1.0];
    
    if(app.alerts.buttons[@"Turn Off"].enabled)
      [app.alerts.buttons[@"Turn Off"] tap];
    
    for (NSUInteger charIdx=0; charIdx<passCode.length; charIdx++)
    {
      [NSThread sleepForTimeInterval:1.0];
      NSString *key = [passCode substringWithRange:NSMakeRange(charIdx, 1)];
      [app.keyboards.keys[key] tap];
    }
      [NSThread sleepForTimeInterval:1.0];
  }
  else{
    return FBResponseWithErrorFormat(@"Failed to reset Passcode");
  }

  if (app.processID == 0) {
    return FBResponseWithErrorFormat(@"Failed to launch %@ application", bundleID);
  }
  
  /*if (requirements[@"defaultAlertAction"]) {
   [FBSession sessionWithApplication:app defaultAlertAction:(id)requirements[@"defaultAlertAction"]];
   } else {
   [FBSession sessionWithApplication:app];
   }*/
  
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
}


+ (id<FBResponsePayload>)handleSetProxy:(FBRouteRequest *)request
{
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *bundleID = @"com.apple.Preferences";
  NSString *ipAddress = request.arguments[@"ipAddress"];
  NSString *port = request.arguments[@"port"];
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithErrorFormat(@"'bundleId' desired capability not provided");
  }
  if (!ipAddress) {
    return FBResponseWithErrorFormat(@"'ipAddress' desired capability not provided");
  }
  if (!port) {
    return FBResponseWithErrorFormat(@"'port' desired capability not provided");
  }
  
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];
  if(app.tables.cells.staticTexts[@"Wi-Fi"].enabled)
  {
    [app.tables.cells.staticTexts[@"Wi-Fi"] tap];
    [NSThread sleepForTimeInterval:1.0];
    
    [app.buttons[@"More Info"].firstMatch tap];
    [NSThread sleepForTimeInterval:1.0];
    if(app.staticTexts[@"Configure Proxy"].isEnabled)
    {
        [app.staticTexts[@"Configure Proxy"] tap];
        [NSThread sleepForTimeInterval:1.0];
        [app.staticTexts[@"Manual"] tap];
        [NSThread sleepForTimeInterval:1.0];
        [app.tables.cells.textFields[@"Server"] tap];
        [NSThread sleepForTimeInterval:0.5];
        [app.textFields[@"Server"] typeText:ipAddress];
        [NSThread sleepForTimeInterval:1.0];
        [app.tables.cells.textFields[@"Port"] tap];
        [NSThread sleepForTimeInterval:0.5];
        [app.textFields[@"Port"] typeText:port];
        [NSThread sleepForTimeInterval:0.5];
        [app.buttons[@"Save"] tap];

    }
    else if(app.buttons[@"Manual"].enabled)
    {
	    [app.buttons[@"Manual"] tap];
	    [app.textFields[@"Server"] tap];
	    [NSThread sleepForTimeInterval:0.5];
	    [app.textFields[@"Server"] typeText:ipAddress];
            [NSThread sleepForTimeInterval:1.0];
            [app.textFields[@"Port"] tap];
    	      [NSThread sleepForTimeInterval:0.5];
    	      [app.textFields[@"Port"] typeText:port];
            [NSThread sleepForTimeInterval:0.5];
            [app.buttons[@"Wi-Fi"] tap];
     }
  }
  if (app.processID == 0) {
    return FBResponseWithErrorFormat(@"Failed to launch %@ application", bundleID);
  }
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
}

+ (id<FBResponsePayload>)handleResetProxy:(FBRouteRequest *)request
{
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *bundleID = @"com.apple.Preferences";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithErrorFormat(@"'bundleId' desired capability not provided");
  }
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];
  
  if(app.tables.cells.staticTexts[@"Wi-Fi"].enabled)
  {
    [app.tables.cells.staticTexts[@"Wi-Fi"] tap];
    [NSThread sleepForTimeInterval:1.0];
  
    
    [app.buttons[@"More Info"].firstMatch tap];
    [NSThread sleepForTimeInterval:1.0];
    if(app.staticTexts[@"Configure Proxy"].isEnabled)
    {
         [app.staticTexts[@"Configure Proxy"] tap];
         [NSThread sleepForTimeInterval:1.0];
         [app.staticTexts[@"Off"] tap];
         [app.buttons[@"Save"] tap];
    }
    else if(app.buttons[@"Off"].enabled)
    {
         [app.buttons[@"Off"] tap];
         [NSThread sleepForTimeInterval:0.5];
         [app.buttons[@"Wi-Fi"] tap];
    }
    // [app.staticTexts[@"Off"] tap];
    /*[app.tables.cells.textFields[@"Server"] tap];
     [app.textFields[@"Server"] typeText:ipAddress];
     [app.tables.cells.textFields[@"Port"] tap];
     [app.textFields[@"Port"] typeText:port];*/
  }
  if (app.processID == 0) {
    return FBResponseWithErrorFormat(@"Failed to launch %@ application", bundleID);
  }
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
}

+ (id<FBResponsePayload>)handleCleanSafari:(FBRouteRequest *)request
{
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *bundleID = @"com.apple.Preferences";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithErrorFormat(@"'bundleId' desired capability not provided");
  }
  
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];
  [app terminate];
  [app launch];
  if(app.tables.cells.staticTexts[@"Safari"].enabled){
    [app.tables.cells.staticTexts[@"Safari"] tap];
    [NSThread sleepForTimeInterval:0.5];
    /*[app.buttons[@"More Info"] tap];
     [NSThread sleepForTimeInterval:1.0];*/
    [app.staticTexts[@"Clear History and Website Data"] tap];
    [NSThread sleepForTimeInterval:0.5];
if(app.alerts.buttons[@"Clear"].enabled)
        [app.alerts.buttons[@"Clear"] tap];
    else
        [app.buttons[@"Clear History and Data"] tap];
  
}
  else{
    return FBResponseWithErrorFormat(@"Safari cleanup Failed!!");
  }
  if (app.processID == 0) {
    return FBResponseWithErrorFormat(@"Failed to launch %@ application", bundleID);
  }
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
}

+ (id<FBResponsePayload>)handleWifiOn:(FBRouteRequest *)request
{
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *bundleID = @"com.apple.Preferences";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithErrorFormat(@"'bundleId' desired capability not provided");
  }
  
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];
  if(app.tables.cells.staticTexts[@"Wi-Fi"].enabled)
  {
    if(app.tables.cells.staticTexts[@"Off"].enabled)
    {
      
        [app.tables.cells.staticTexts[@"Wi-Fi"] tap];
        [NSThread sleepForTimeInterval:1.0];
        if(app.tables.cells.switches[@"Wi-Fi"].enabled)
        [app.tables.cells.switches[@"Wi-Fi"] tap];
      
    }
    else{
      return FBResponseWithErrorFormat(@"Wifi is already enabled");
    }
  }
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
  
}

+ (id<FBResponsePayload>)handleWifiOff:(FBRouteRequest *)request
{
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *bundleID = @"com.apple.Preferences";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithErrorFormat(@"'bundleId' desired capability not provided");
  }
  
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];
  if(app.tables.cells.staticTexts[@"Wi-Fi"].enabled)
  {
    if(app.tables.cells.staticTexts[@"Off"].enabled)
    {
      return FBResponseWithErrorFormat(@"Wifi is already disabled");
    }
    else
    {
      [app.tables.cells.staticTexts[@"Wi-Fi"] tap];
      [NSThread sleepForTimeInterval:1.0];
      if(app.tables.cells.switches[@"Wi-Fi"].enabled)
       [app.tables.cells.switches[@"Wi-Fi"] tap];
    }
  }
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
  
}


+ (id<FBResponsePayload>)handleConnectVPN:(FBRouteRequest *)request
{
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *bundleID = @"com.pcloudy.wildnet";
  //  NSString *bundleID = @"net.openvpn.connect.app";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithErrorFormat(@"'bundleId' desired capability not provided");
  }
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];
  
  [NSThread sleepForTimeInterval:0.5];
  [app.buttons[@"Connect"] tap];
  [NSThread sleepForTimeInterval:0.2];
  
  // [app.otherElements[@"ec2-54-212-245-246.us-west-2.compute.amazonaws.com [pcloudy]"] tap];
  [NSThread sleepForTimeInterval:0.5];
  if (app.processID == 0) {
    return FBResponseWithErrorFormat(@"Failed to launch %@ application", bundleID);
  }
  [NSThread sleepForTimeInterval:3.5];
  NSLog(@"Getting VPN IP");
  NSError *error;
  if (![[XCUIDevice sharedDevice] fb_goToHomescreenWithError:&error]) {
    return FBResponseWithError(error);
  }
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
}





+ (id<FBResponsePayload>)handleLaunchApp:(FBRouteRequest *)request
{
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *bundleID = request.arguments[@"bundleID"];
  //NSString *bundleID = @"com.investtools.radar";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithErrorFormat(@"'bundleId' desired capability not provided");
  }
 /*
  [[UIApplication sharedApplication] openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]];


  XCUIApplication *applcation = [[XCUIApplication alloc] init];
  [applcation initWithBundleIdentifier:bundleID];
  [applcation launch];
*/
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  //app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  NSString *nostring = @"NoAnimations";
  //NSArray *stringaarray = [@"NoAnimations"];
  NSArray *argument = @[nostring];

  //app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchArguments = (NSArray<NSString *> *)argument;
 
  
  //app.launchArguments =  "@*nostring";
  //app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  app.fb_shouldWaitForQuiescence  = false;
  [app activate];
  [NSThread sleepForTimeInterval:0.5];
   
  return FBResponseWithErrorFormat(@"0.0.0.0");
 // return FBResponseWithObject(FBSessionCommands.sessionInformation);
}

+ (id<FBResponsePayload>)handleVPNIP:(FBRouteRequest *)request
{
  NSLog(@"Getting VPN IP");
  
  NSString *address = @"error";
  struct ifaddrs *interfaces = NULL;
  struct ifaddrs *temp_addr = NULL;
  int success = 0;
  // retrieve the current interfaces - returns 0 on success
  success = getifaddrs(&interfaces);
  if (success == 0) {
    // Loop through linked list of interfaces
    temp_addr = interfaces;
    while(temp_addr != NULL) {
      if(temp_addr->ifa_addr->sa_family == AF_INET) {
        NSLog(@"Network Interface %@", [NSString stringWithUTF8String:temp_addr->ifa_name] );
        // Check if interface is en0 which is the wifi connection on the iPhone
        // if([[NSString stringWithUTF8String:temp_addr->ifa_name] isEqualToString:@"utun1"]) {
        if([[NSString stringWithUTF8String:temp_addr->ifa_name] containsString:@"utun"]) {
          // Get NSString from C String
          address = [NSString stringWithUTF8String:inet_ntoa(((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr)];
          
        }
        
      }
      
      temp_addr = temp_addr->ifa_next;
    }
  }
  // Free memory
  freeifaddrs(interfaces);
  if (!(address == @"error")){
    return FBResponseWithErrorFormat(@"%@ ", address);
  }else{
    return FBResponseWithErrorFormat(@"0.0.0.0");
  }
}

+ (id<FBResponsePayload>)handleDisConnectVPN:(FBRouteRequest *)request
{
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  //NSString *bundleID = @"net.openvpn.connect.app";
  NSString *bundleID = @"com.pcloudy.wildnet";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithErrorFormat(@"'bundleId' desired capability not provided");
  }
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];
  [NSThread sleepForTimeInterval:0.5];
  [app.buttons[@"Connect"] tap];
  [app.buttons[@"Disconnect"] tap];
  // [app.otherElements[@"ec2-54-212-245-246.us-west-2.compute.amazonaws.com [pcloudy]"] tap];
  [NSThread sleepForTimeInterval:0.5];
  if (app.processID == 0) {
    return FBResponseWithErrorFormat(@"Failed to launch %@ application", bundleID);
  }
  
  
  NSError *error;
  if (![[XCUIDevice sharedDevice] fb_goToHomescreenWithError:&error]) {
    return FBResponseWithError(error);
  }
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
}

+ (id<FBResponsePayload>)handleSessionAppLaunch:(FBRouteRequest *)request
{
  [request.session launchApplicationWithBundleId:(id)request.arguments[@"bundleId"]
                         shouldWaitForQuiescence:request.arguments[@"shouldWaitForQuiescence"]
                                       arguments:request.arguments[@"arguments"]
                                     environment:request.arguments[@"environment"]];
  return FBResponseWithOK();
}

+ (id<FBResponsePayload>)handleSessionAppActivate:(FBRouteRequest *)request
{
  [request.session activateApplicationWithBundleId:(id)request.arguments[@"bundleId"]];
  return FBResponseWithOK();
}

+ (id<FBResponsePayload>)handleSessionAppTerminate:(FBRouteRequest *)request
{
  BOOL result = [request.session terminateApplicationWithBundleId:(id)request.arguments[@"bundleId"]];
  return FBResponseWithStatus(FBCommandStatusNoError, @(result));
}

+ (id<FBResponsePayload>)handleSessionAppState:(FBRouteRequest *)request
{
  NSUInteger state = [request.session applicationStateWithBundleId:(id)request.arguments[@"bundleId"]];
  return FBResponseWithStatus(FBCommandStatusNoError, @(state));
}

+ (id<FBResponsePayload>)handleGetActiveSession:(FBRouteRequest *)request
{
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
}

+ (id<FBResponsePayload>)handleDeleteSession:(FBRouteRequest *)request
{
  [request.session kill];
  return FBResponseWithOK();
}

+ (id<FBResponsePayload>)handleGetStatus:(FBRouteRequest *)request
{
  // For updatedWDABundleId capability by Appium
  NSString *productBundleIdentifier = @"com.facebook.WebDriverAgentRunner";
  NSString *envproductBundleIdentifier = NSProcessInfo.processInfo.environment[@"WDA_PRODUCT_BUNDLE_IDENTIFIER"];
  if (envproductBundleIdentifier && [envproductBundleIdentifier length] != 0) {
    productBundleIdentifier = NSProcessInfo.processInfo.environment[@"WDA_PRODUCT_BUNDLE_IDENTIFIER"];
  }
  
  NSMutableDictionary *buildInfo = [NSMutableDictionary dictionaryWithDictionary:@{
                                                                                   @"time" : [self.class buildTimestamp],
                                                                                   @"productBundleIdentifier" : productBundleIdentifier,
                                                                                   }];
  NSString *upgradeTimestamp = NSProcessInfo.processInfo.environment[@"UPGRADE_TIMESTAMP"];
  if (nil != upgradeTimestamp && upgradeTimestamp.length > 0) {
    [buildInfo setObject:upgradeTimestamp forKey:@"upgradedAt"];
  }
  
  return
  FBResponseWithStatus(
                       FBCommandStatusNoError,
                       @{
                         @"state" : @"success",
                         @"os" :
                           @{
                             @"name" : [[UIDevice currentDevice] systemName],
                             @"version" : [[UIDevice currentDevice] systemVersion],
                             @"sdkVersion": FBSDKVersion() ?: @"unknown",
                             },
                         @"ios" :
                           @{
                             @"simulatorVersion" : [[UIDevice currentDevice] systemVersion],
                             @"ip" : [XCUIDevice sharedDevice].fb_wifiIPAddress ?: [NSNull null],
                             },
                         @"build" : buildInfo.copy
                         }
                       );
}

+ (id<FBResponsePayload>)handleGetHealthCheck:(FBRouteRequest *)request
{
  if (![[XCUIDevice sharedDevice] fb_healthCheckWithApplication:[FBApplication fb_activeApplication]]) {
    return FBResponseWithErrorFormat(@"Health check failed");
  }
  return FBResponseWithOK();
}

+ (id<FBResponsePayload>)handleGetSettings:(FBRouteRequest *)request
{
  return FBResponseWithObject(
                              @{
                                USE_COMPACT_RESPONSES: @([FBConfiguration shouldUseCompactResponses]),
                                ELEMENT_RESPONSE_ATTRIBUTES: [FBConfiguration elementResponseAttributes],
                                MJPEG_SERVER_SCREENSHOT_QUALITY: @([FBConfiguration mjpegServerScreenshotQuality]),
                                MJPEG_SERVER_FRAMERATE: @([FBConfiguration mjpegServerFramerate]),
                                MJPEG_SCALING_FACTOR: @([FBConfiguration mjpegScalingFactor]),
                                SCREENSHOT_QUALITY: @([FBConfiguration screenshotQuality]),
                                }
                              );
}

// TODO if we get lots more settings, handling them with a series of if-statements will be unwieldy
// and this should be refactored
+ (id<FBResponsePayload>)handleSetSettings:(FBRouteRequest *)request
{
  NSDictionary* settings = request.arguments[@"settings"];
  
  if ([settings objectForKey:USE_COMPACT_RESPONSES]) {
    [FBConfiguration setShouldUseCompactResponses:[[settings objectForKey:USE_COMPACT_RESPONSES] boolValue]];
  }
  if ([settings objectForKey:ELEMENT_RESPONSE_ATTRIBUTES]) {
    [FBConfiguration setElementResponseAttributes:(NSString *)[settings objectForKey:ELEMENT_RESPONSE_ATTRIBUTES]];
  }
  if ([settings objectForKey:MJPEG_SERVER_SCREENSHOT_QUALITY]) {
    [FBConfiguration setMjpegServerScreenshotQuality:[[settings objectForKey:MJPEG_SERVER_SCREENSHOT_QUALITY] unsignedIntegerValue]];
  }
  if ([settings objectForKey:MJPEG_SERVER_FRAMERATE]) {
    [FBConfiguration setMjpegServerFramerate:[[settings objectForKey:MJPEG_SERVER_FRAMERATE] unsignedIntegerValue]];
  }
  if ([settings objectForKey:SCREENSHOT_QUALITY]) {
    [FBConfiguration setScreenshotQuality:[[settings objectForKey:SCREENSHOT_QUALITY] unsignedIntegerValue]];
  }
  if ([settings objectForKey:MJPEG_SCALING_FACTOR]) {
    [FBConfiguration setMjpegScalingFactor:[[settings objectForKey:MJPEG_SCALING_FACTOR] unsignedIntegerValue]];
  }
  
  return [self handleGetSettings:request];
}


#pragma mark - Helpers

+ (NSString *)buildTimestamp
{
  return [NSString stringWithFormat:@"%@ %@",
          [NSString stringWithUTF8String:__DATE__],
          [NSString stringWithUTF8String:__TIME__]
          ];
}

+ (NSDictionary *)sessionInformation
{
  return
  @{
   // @"sessionId" : [FBSession activeSession].identifier ?: NSNull.null,
    @"capabilities" : FBSessionCommands.currentCapabilities
    };
}

+ (NSDictionary *)currentCapabilities
{
  if(SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"13.4")){
    //FBApplication *application = [FBSession activeSession].activeApplication;
    return
    @{
      @"device": ([UIDevice currentDevice].userInterfaceIdiom == UIUserInterfaceIdiomPad) ? @"ipad" : @"iphone",
      @"sdkVersion": [[UIDevice currentDevice] systemVersion],
     // @"browserName": application.label ?: [NSNull null],
      //@"CFBundleIdentifier": application.bundleID ?: [NSNull null],
      };
  }
    FBApplication *application = [FBSession activeSession].activeApplication;
  return
  @{
    @"device": ([UIDevice currentDevice].userInterfaceIdiom == UIUserInterfaceIdiomPad) ? @"ipad" : @"iphone",
    @"sdkVersion": [[UIDevice currentDevice] systemVersion],
    @"browserName": application.label ?: [NSNull null],
    @"CFBundleIdentifier": application.bundleID ?: [NSNull null],
    };
}

@end
