/**
 * Copyright (c) 2015-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */


#import "FBSessionCommands.h"
#import <UIKit/UIKit.h>
#import "FBApplication.h"
#import "FBCapabilities.h"
#import "FBConfiguration.h"
#import "FBLogger.h"
#import "FBProtocolHelpers.h"
#import "FBRouteRequest.h"
#import "FBSession.h"
#import "FBSettings.h"
#import "FBApplication.h"
#import "FBRuntimeUtils.h"
#import "FBActiveAppDetectionPoint.h"
#import "FBXCodeCompatibility.h"
#import "XCUIApplication+FBHelpers.h"
#import "XCUIApplication+FBQuiescence.h"
#import "XCUIDevice.h"
#import "XCUIDevice+FBHealthCheck.h"
#import "XCUIDevice+FBHelpers.h"
#import "XCUIApplicationProcessDelay.h"



#import "FBTouchActionCommands.h"

#import "FBApplication.h"
#import "FBRoute.h"
#import "FBRouteRequest.h"
#import "FBSession.h"
#import "XCUIApplication+FBTouchAction.h"



@implementation FBSessionCommands
int statusflag = 0;
#pragma mark - <FBCommandHandler>

+ (NSArray *)routes
{
  return
  @[
    [[FBRoute POST:@"/url"].withoutSession respondWithTarget:self action:@selector(handleOpenURL:)],
    [[FBRoute POST:@"/session"].withoutSession respondWithTarget:self action:@selector(handleCreateSession:)],
    [[FBRoute POST:@"/cleanSafari"].withoutSession respondWithTarget:self action:@selector(handleCleanSafari:)],
    [[FBRoute POST:@"/setProxy"].withoutSession respondWithTarget:self action:@selector(handleSetProxy:)],
    [[FBRoute POST:@"/resetProxy"].withoutSession respondWithTarget:self action:@selector(handleResetProxy:)],
    [[FBRoute GET:@"/clearPassCode"].withoutSession respondWithTarget:self action:@selector(handleclearPassCode:)],
    [[FBRoute POST:@"/setPassCode"].withoutSession respondWithTarget:self action:@selector(handleSetPassCode:)],
    [[FBRoute POST:@"/downloadfromurl"].withoutSession respondWithTarget:self action:@selector(handleDownloadRromURL:)],
    [[FBRoute POST:@"/resetPassCode"].withoutSession respondWithTarget:self action:@selector(handleResetPassCode:)],
    [[FBRoute POST:@"/wda/apps/launch"] respondWithTarget:self action:@selector(handleSessionAppLaunch:)],
    [[FBRoute POST:@"/wda/apps/activate"] respondWithTarget:self action:@selector(handleSessionAppActivate:)],
    [[FBRoute POST:@"/wda/apps/terminate"] respondWithTarget:self action:@selector(handleSessionAppTerminate:)],
    [[FBRoute POST:@"/wda/apps/state"] respondWithTarget:self action:@selector(handleSessionAppState:)],
    [[FBRoute GET:@"/wda/apps/list"] respondWithTarget:self action:@selector(handleGetActiveAppsList:)],
    [[FBRoute GET:@""] respondWithTarget:self action:@selector(handleGetActiveSession:)],
    [[FBRoute DELETE:@""] respondWithTarget:self action:@selector(handleDeleteSession:)],
    [[FBRoute GET:@"/status"].withoutSession respondWithTarget:self action:@selector(handleGetStatus:)],
    
    // Health check might modify simulator state so it should only be called in-between testing sessions
    [[FBRoute GET:@"/wda/healthcheck"].withoutSession respondWithTarget:self action:@selector(handleGetHealthCheck:)],
    
    // Settings endpoints
    [[FBRoute GET:@"/appium/settings"] respondWithTarget:self action:@selector(handleGetSettings:)],
    [[FBRoute POST:@"/appium/settings"] respondWithTarget:self action:@selector(handleSetSettings:)],
  ];
}


#pragma mark - Commands



+ (id<FBResponsePayload>)handleclearPassCode:(FBRouteRequest *)request
{

  NSError *error;
  NSString *textToType = @"147369";
  NSUInteger frequency = [request.arguments[@"frequency"] unsignedIntegerValue] ?: [FBConfiguration maxTypingFrequency];
  [[XCUIDevice sharedDevice] fb_lockScreen:&error];
  [NSThread sleepForTimeInterval:2.0];
  [[XCUIDevice sharedDevice] fb_unlockScreen:&error];
  if (![FBKeyboard typeText:textToType frequency:frequency error:&error]) {
  }
  return FBResponseWithOK();
}

+ (id<FBResponsePayload>)handleSetPassCode:(FBRouteRequest *)request
{
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *passCode = @"147369";// request.arguments[@"passCode"];
  NSLog(@"%@", passCode);
  NSString *bundleID = @"com.apple.Preferences";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    //return FBResponseWithErrorFormat(@"'bundleId' desired capability not provided");
  }
  if (!passCode) {
    //return FBResponseWithErrorFormat(@"'passCode' desired capability not provided");
  }
  
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
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
      if(app.buttons[@"Keep"].enabled)
        [app.buttons[@"Keep"] tap];
      [NSThread sleepForTimeInterval:1.0];
      
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
    //    return FBResponseWithErrorFormat(@"Failed to set Passcode");
  }
  
  if (app.processID == 0) {
    //  return FBResponseWithErrorFormat(@"Failed to launch %@ application", bundleID);
  }
  /*if (requirements[@"defaultAlertAction"]) {
   [FBSession sessionWithApplication:app defaultAlertAction:(id)requirements[@"defaultAlertAction"]];
   } else {
   [FBSession sessionWithApplication:app];
   }*/
  NSError *error;
  if (![[XCUIDevice sharedDevice] fb_goToHomescreenWithError:&error]) {
    // return FBResponseWithError(error);
  }
  
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
}

+ (id<FBResponsePayload>)handleResetPassCode:(FBRouteRequest *)request
{
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *passCode = @"147369";//  request.arguments[@"passCode"];
  NSString *bundleID = @"com.apple.Preferences";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    //return FBResponseWithErrorFormat(@"'bundleId' desired capability not provided");
  }
  if (!passCode) {
    //return FBResponseWithErrorFormat(@"'passCode' desired capability not provided");
  }
  
  
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
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
    
    [NSThread sleepForTimeInterval:2.0];
    
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
    //return FBResponseWithErrorFormat(@"Failed to reset Passcode");
  }
  
  if (app.processID == 0) {
    //return FBResponseWithErrorFormat(@"Failed to launch %@ application", bundleID);
  }
  
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
}

-(void) getImageFromURLAndSaveItToLocalData:(NSString *)imageName fileURL:(NSString *)fileURL inDirectory:(NSString *)directoryPath {
  
  NSData * data = [NSData dataWithContentsOfURL:[NSURL URLWithString:fileURL]];
  
  NSError *error = nil;
  [data writeToFile:[directoryPath stringByAppendingPathComponent:[NSString stringWithFormat:@"%@", imageName]] options:NSAtomicWrite error:&error];
  
  if (error) {
    NSLog(@"Error Writing File : %@",error);
  }else{
    NSLog(@"Image %@ Saved SuccessFully",imageName);
  }
}




+ (id<FBResponsePayload>)handleDownloadRromURL:(FBRouteRequest *)request
{

 // NSString *ipAddress = request.arguments[@"url"];
  
  NSString *imgURL = request.arguments[@"url"]; // @"https://cdn.cocoacasts.com/cc00ceb0c6bff0d536f25454d50223875d5c79f1/above-the-clouds.jpg";
  
  NSURL  *url = [NSURL URLWithString:imgURL];
  NSData *urlData = [NSData dataWithContentsOfURL:url];
  if ( urlData )
  {
    NSLog(@"Downloading started...");
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    NSString *filePath = [NSString stringWithFormat:@"%@/%@", documentsDirectory,@"dwnld_image.png"];
    NSLog(@"FILE : %@",filePath);
    [urlData writeToFile:filePath atomically:YES];
    UIImage *image=[UIImage imageWithContentsOfFile:filePath];
    
    NSLog(@"Completed...");
    dispatch_async(dispatch_get_main_queue(), ^{
      UIImageWriteToSavedPhotosAlbum(image, nil , nil, nil);
    });
    
    
    
  }
 
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
    //return FBResponseWithErrorFormat(@"'bundleId' desired capability not provided");
  }
  if (!ipAddress) {
    //return FBResponseWithErrorFormat(@"'ipAddress' desired capability not provided");
  }
  if (!port) {
    //return FBResponseWithErrorFormat(@"'port' desired capability not provided");
  }
  
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  //app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];
  [app terminate];
  [NSThread sleepForTimeInterval:1.0];
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
    //return FBResponseWithErrorForm  at(@"Failed to launch %@ application", bundleID);
  }
  [NSThread sleepForTimeInterval:1];
  [app terminate];
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
}

+ (id<FBResponsePayload>)handleResetProxy:(FBRouteRequest *)request
{
  
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *bundleID = @"com.apple.Preferences";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithUnknownErrorFormat(@"'bundleId' desired capability not provided");
  }
  CFDictionaryRef netDetaials = CFNetworkCopySystemProxySettings();
  
  NSLog(@"Reset Proxy called");
  
 
    FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
    //app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
    app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
    app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
    [app launch];
    [app terminate];
    [NSThread sleepForTimeInterval:1.0];
    
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
      
    }
    
    [NSThread sleepForTimeInterval:1];
    [app terminate];
 
  
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
}


+ (id<FBResponsePayload>)handleCleanSafari:(FBRouteRequest *)request
{
  NSError *error;
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *bundleID = @"com.apple.Preferences";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithUnknownErrorFormat(@"'bundleId' desired capability not provided");
  }
  
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  // app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
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
    return FBResponseWithObject(FBSessionCommands.sessionInformation);
    
    
  }
  if (app.processID == 0) {
    return FBResponseWithObject(FBSessionCommands.sessionInformation);
  }
  [NSThread sleepForTimeInterval:1.0];
  //[[XCUIDevice sharedDevice] fb_goToHomescreenWithError:&error];
  [app terminate];
  // return FBResponseWithUnknownErrorFormat(@"'bundleId' desired capability not provided");
  return FBResponseWithObject(FBSessionCommands.sessionInformation);
}



+ (id<FBResponsePayload>)handleOpenURL:(FBRouteRequest *)request
{
  NSString *urlString = request.arguments[@"url"];
  
  if (!urlString) {
    return FBResponseWithStatus([FBCommandStatus invalidArgumentErrorWithMessage:@"URL is required" traceback:nil]);
  }
  NSError *error;
  if (![XCUIDevice.sharedDevice fb_openUrl:urlString error:&error]) {
    return FBResponseWithUnknownError(error);
  }
  return FBResponseWithOK();
}

+ (id<FBResponsePayload>)handleCreateSession:(FBRouteRequest *)request
{
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *bundleID = requirements[@"bundleId"];
  NSString *appPath = requirements[@"app"];
  
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
  
  //[FBConfiguration setShouldWaitForQuiescence:[requirements[@"shouldWaitForQuiescence"] boolValue]];
  
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  // app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];
  
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
  return FBResponseWithObject(@(result));
}

+ (id<FBResponsePayload>)handleSessionAppState:(FBRouteRequest *)request
{
  NSUInteger state = [request.session applicationStateWithBundleId:(id)request.arguments[@"bundleId"]];
  return FBResponseWithObject(@(state));
}

+ (id<FBResponsePayload>)handleGetActiveAppsList:(FBRouteRequest *)request
{
  return FBResponseWithObject([XCUIApplication fb_activeAppsInfo]);
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
  NSDictionary<NSString *, id> *requirements;
  NSError *error;
  
  /*if (![request.arguments[@"capabilities"] isKindOfClass:NSDictionary.class]) {
   return FBResponseWithStatus([FBCommandStatus sessionNotCreatedError:@"'capabilities' is mandatory to create a new session"
   traceback:nil]);
   }*/
  if (nil == (requirements = FBParseCapabilities((NSDictionary *)request.arguments[@"capabilities"], &error))) {
    return FBResponseWithStatus([FBCommandStatus sessionNotCreatedError:error.description traceback:nil]);
  }
  /*
   [FBConfiguration setShouldUseTestManagerForVisibilityDetection:[requirements[@"shouldUseTestManagerForVisibilityDetection"] boolValue]];
   if (requirements[USE_COMPACT_RESPONSES]) {
   [FBConfiguration setShouldUseCompactResponses:[requirements[USE_COMPACT_RESPONSES] boolValue]];
   }
   
   NSString *elementResponseAttributes = requirements[ELEMENT_RESPONSE_ATTRIBUTES];
   if (elementResponseAttributes) {
   [FBConfiguration setElementResponseAttributes:elementResponseAttributes];
   }
   */
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
  /*
   if (nil != requirements[WAIT_FOR_IDLE_TIMEOUT]) {
   FBConfiguration.waitForIdleTimeout = [requirements[WAIT_FOR_IDLE_TIMEOUT] doubleValue];
   }
   */
  NSString *bundleID = requirements[@"bundleId"];
  FBApplication *app = nil;
  if (bundleID != nil) {
    app = [[FBApplication alloc] initPrivateWithPath:requirements[@"app"]
                                            bundleID:bundleID];
    app.fb_shouldWaitForQuiescence = nil == requirements[@"shouldWaitForQuiescence"]
    || [requirements[@"shouldWaitForQuiescence"] boolValue];
    app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
    app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
    [app launch];
    if (app.processID == 0) {
      return FBResponseWithStatus([FBCommandStatus sessionNotCreatedError:[NSString stringWithFormat:@"Failed to launch %@ application", bundleID] traceback:nil]);
    }
  }
  if (statusflag == 0){
    if (requirements[@"defaultAlertAction"]) {
      [FBSession initWithApplication:app
                  defaultAlertAction:(id)requirements[@"defaultAlertAction"]];
    } else {
      [FBSession initWithApplication:app];
    }
    statusflag = 1;
  }
  // return FBResponseWithObject(FBSessionCommands.sessionInformation);
  // For updatedWDABundleId capability by Appium
  NSString *productBundleIdentifier = @"com.pcloudy.WebDriverAgentRunner";
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
  
  return FBResponseWithObject(
                              @{
    @"ready" : @YES,
    @"message" : @"WebDriverAgent is ready to accept commands v 4.2.0",
    @"state" : @"success",
    @"os" :
      @{
        @"name" : [[UIDevice currentDevice] systemName],
        @"version" : [[UIDevice currentDevice] systemVersion],
        @"sdkVersion": FBSDKVersion() ?: @"unknown",
        // @"testmanagerdVersion": @([XCUIApplication fb_testmanagerdVersion]),
        @"testmanagerdVersion": @(FBTestmanagerdVersion()),        },
    @"ios" :
      @{
#if TARGET_OS_SIMULATOR
        @"simulatorVersion" : [[UIDevice currentDevice] systemVersion],
#endif
        @"ip" : [XCUIDevice sharedDevice].fb_wifiIPAddress ?: [NSNull null]
      },
    @"build" : buildInfo.copy
  }
                              );
}

+ (id<FBResponsePayload>)handleGetStatus1:(FBRouteRequest *)request
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
  
  return FBResponseWithObject(
                              @{
    @"ready" : @YES,
    @"message" : @"WebDriverAgent is ready to accept commands v 4.2.0",
    @"state" : @"success",
    @"os" :
      @{
        @"name" : [[UIDevice currentDevice] systemName],
        @"version" : [[UIDevice currentDevice] systemVersion],
        @"sdkVersion": FBSDKVersion() ?: @"unknown",
        @"testmanagerdVersion": @(FBTestmanagerdVersion()),
      },
    @"ios" :
      @{
#if TARGET_OS_SIMULATOR
        @"simulatorVersion" : [[UIDevice currentDevice] systemVersion],
#endif
        @"ip" : [XCUIDevice sharedDevice].fb_wifiIPAddress ?: [NSNull null]
      },
    @"build" : buildInfo.copy
  }
                              );
}

+ (id<FBResponsePayload>)handleGetHealthCheck:(FBRouteRequest *)request
{
  if (![[XCUIDevice sharedDevice] fb_healthCheckWithApplication:[FBApplication fb_activeApplication]]) {
    return FBResponseWithUnknownErrorFormat(@"Health check failed");
  }
  return FBResponseWithOK();
}

+ (id<FBResponsePayload>)handleGetSettings:(FBRouteRequest *)request
{
  return FBResponseWithObject(
                              @{
    FB_SETTING_USE_COMPACT_RESPONSES: @([FBConfiguration shouldUseCompactResponses]),
    FB_SETTING_ELEMENT_RESPONSE_ATTRIBUTES: [FBConfiguration elementResponseAttributes],
    FB_SETTING_MJPEG_SERVER_SCREENSHOT_QUALITY: @([FBConfiguration mjpegServerScreenshotQuality]),
    FB_SETTING_MJPEG_SERVER_FRAMERATE: @([FBConfiguration mjpegServerFramerate]),
    FB_SETTING_MJPEG_SCALING_FACTOR: @([FBConfiguration mjpegScalingFactor]),
    FB_SETTING_SCREENSHOT_QUALITY: @([FBConfiguration screenshotQuality]),
    FB_SETTING_KEYBOARD_AUTOCORRECTION: @([FBConfiguration keyboardAutocorrection]),
    FB_SETTING_KEYBOARD_PREDICTION: @([FBConfiguration keyboardPrediction]),
    FB_SETTING_CUSTOM_SNAPSHOT_TIMEOUT: @([FBConfiguration customSnapshotTimeout]),
    FB_SETTING_SNAPSHOT_MAX_DEPTH: @([FBConfiguration snapshotMaxDepth]),
    FB_SETTING_USE_FIRST_MATCH: @([FBConfiguration useFirstMatch]),
    FB_SETTING_WAIT_FOR_IDLE_TIMEOUT: @([FBConfiguration waitForIdleTimeout]),
    FB_SETTING_ANIMATION_COOL_OFF_TIMEOUT: @([FBConfiguration animationCoolOffTimeout]),
    FB_SETTING_BOUND_ELEMENTS_BY_INDEX: @([FBConfiguration boundElementsByIndex]),
    FB_SETTING_REDUCE_MOTION: @([FBConfiguration reduceMotionEnabled]),
    FB_SETTING_DEFAULT_ACTIVE_APPLICATION: request.session.defaultActiveApplication,
    FB_SETTING_ACTIVE_APP_DETECTION_POINT: FBActiveAppDetectionPoint.sharedInstance.stringCoordinates,
    FB_SETTING_INCLUDE_NON_MODAL_ELEMENTS: @([FBConfiguration includeNonModalElements]),
    FB_SETTING_ACCEPT_ALERT_BUTTON_SELECTOR: FBConfiguration.acceptAlertButtonSelector,
    FB_SETTING_DISMISS_ALERT_BUTTON_SELECTOR: FBConfiguration.dismissAlertButtonSelector,
    FB_SETTING_DEFAULT_ALERT_ACTION: request.session.defaultAlertAction ?: @"",
#if !TARGET_OS_TV
    FB_SETTING_SCREENSHOT_ORIENTATION: [FBConfiguration humanReadableScreenshotOrientation],
#endif
  }
                              );
}

// TODO if we get lots more settings, handling them with a series of if-statements will be unwieldy
// and this should be refactored
+ (id<FBResponsePayload>)handleSetSettings:(FBRouteRequest *)request
{
  NSDictionary* settings = request.arguments[@"settings"];
  
  if (nil != [settings objectForKey:FB_SETTING_USE_COMPACT_RESPONSES]) {
    [FBConfiguration setShouldUseCompactResponses:[[settings objectForKey:FB_SETTING_USE_COMPACT_RESPONSES] boolValue]];
  }
  if (nil != [settings objectForKey:FB_SETTING_ELEMENT_RESPONSE_ATTRIBUTES]) {
    [FBConfiguration setElementResponseAttributes:(NSString *)[settings objectForKey:FB_SETTING_ELEMENT_RESPONSE_ATTRIBUTES]];
  }
  if (nil != [settings objectForKey:FB_SETTING_MJPEG_SERVER_SCREENSHOT_QUALITY]) {
    [FBConfiguration setMjpegServerScreenshotQuality:[[settings objectForKey:FB_SETTING_MJPEG_SERVER_SCREENSHOT_QUALITY] unsignedIntegerValue]];
  }
  if (nil != [settings objectForKey:FB_SETTING_MJPEG_SERVER_FRAMERATE]) {
    [FBConfiguration setMjpegServerFramerate:[[settings objectForKey:FB_SETTING_MJPEG_SERVER_FRAMERATE] unsignedIntegerValue]];
  }
  if (nil != [settings objectForKey:FB_SETTING_SCREENSHOT_QUALITY]) {
    [FBConfiguration setScreenshotQuality:[[settings objectForKey:FB_SETTING_SCREENSHOT_QUALITY] unsignedIntegerValue]];
  }
  if (nil != [settings objectForKey:FB_SETTING_MJPEG_SCALING_FACTOR]) {
    [FBConfiguration setMjpegScalingFactor:[[settings objectForKey:FB_SETTING_MJPEG_SCALING_FACTOR] unsignedIntegerValue]];
  }
  if (nil != [settings objectForKey:FB_SETTING_KEYBOARD_AUTOCORRECTION]) {
    [FBConfiguration setKeyboardAutocorrection:[[settings objectForKey:FB_SETTING_KEYBOARD_AUTOCORRECTION] boolValue]];
  }
  if (nil != [settings objectForKey:FB_SETTING_KEYBOARD_PREDICTION]) {
    [FBConfiguration setKeyboardPrediction:[[settings objectForKey:FB_SETTING_KEYBOARD_PREDICTION] boolValue]];
  }
  // SNAPSHOT_TIMEOUT setting is deprecated. Please use CUSTOM_SNAPSHOT_TIMEOUT instead
  if (nil != [settings objectForKey:FB_SETTING_SNAPSHOT_TIMEOUT]) {
    [FBConfiguration setCustomSnapshotTimeout:[[settings objectForKey:FB_SETTING_SNAPSHOT_TIMEOUT] doubleValue]];
  }
  if (nil != [settings objectForKey:FB_SETTING_CUSTOM_SNAPSHOT_TIMEOUT]) {
    [FBConfiguration setCustomSnapshotTimeout:[[settings objectForKey:FB_SETTING_CUSTOM_SNAPSHOT_TIMEOUT] doubleValue]];
  }
  if (nil != [settings objectForKey:FB_SETTING_SNAPSHOT_MAX_DEPTH]) {
    [FBConfiguration setSnapshotMaxDepth:[[settings objectForKey:FB_SETTING_SNAPSHOT_MAX_DEPTH] intValue]];
  }
  if (nil != [settings objectForKey:FB_SETTING_USE_FIRST_MATCH]) {
    [FBConfiguration setUseFirstMatch:[[settings objectForKey:FB_SETTING_USE_FIRST_MATCH] boolValue]];
  }
  if (nil != [settings objectForKey:FB_SETTING_BOUND_ELEMENTS_BY_INDEX]) {
    [FBConfiguration setBoundElementsByIndex:[[settings objectForKey:FB_SETTING_BOUND_ELEMENTS_BY_INDEX] boolValue]];
  }
  if (nil != [settings objectForKey:FB_SETTING_REDUCE_MOTION]) {
    [FBConfiguration setReduceMotionEnabled:[[settings objectForKey:FB_SETTING_REDUCE_MOTION] boolValue]];
  }
  if (nil != [settings objectForKey:FB_SETTING_DEFAULT_ACTIVE_APPLICATION]) {
    request.session.defaultActiveApplication = (NSString *)[settings objectForKey:FB_SETTING_DEFAULT_ACTIVE_APPLICATION];
  }
  if (nil != [settings objectForKey:FB_SETTING_ACTIVE_APP_DETECTION_POINT]) {
    NSError *error;
    if (![FBActiveAppDetectionPoint.sharedInstance setCoordinatesWithString:(NSString *)[settings objectForKey:FB_SETTING_ACTIVE_APP_DETECTION_POINT]
                                                                      error:&error]) {
      return FBResponseWithStatus([FBCommandStatus invalidArgumentErrorWithMessage:error.description traceback:nil]);
    }
  }
  if (nil != [settings objectForKey:FB_SETTING_INCLUDE_NON_MODAL_ELEMENTS]) {
    if ([XCUIElement fb_supportsNonModalElementsInclusion]) {
      [FBConfiguration setIncludeNonModalElements:[[settings objectForKey:FB_SETTING_INCLUDE_NON_MODAL_ELEMENTS] boolValue]];
    } else {
      [FBLogger logFmt:@"'%@' settings value cannot be assigned, because non modal elements inclusion is not supported by the current iOS SDK", FB_SETTING_INCLUDE_NON_MODAL_ELEMENTS];
    }
  }
  if (nil != [settings objectForKey:FB_SETTING_ACCEPT_ALERT_BUTTON_SELECTOR]) {
    [FBConfiguration setAcceptAlertButtonSelector:(NSString *)[settings objectForKey:FB_SETTING_ACCEPT_ALERT_BUTTON_SELECTOR]];
  }
  if (nil != [settings objectForKey:FB_SETTING_DISMISS_ALERT_BUTTON_SELECTOR]) {
    [FBConfiguration setDismissAlertButtonSelector:(NSString *)[settings objectForKey:FB_SETTING_DISMISS_ALERT_BUTTON_SELECTOR]];
  }
  if (nil != [settings objectForKey:FB_SETTING_WAIT_FOR_IDLE_TIMEOUT]) {
    [FBConfiguration setWaitForIdleTimeout:[[settings objectForKey:FB_SETTING_WAIT_FOR_IDLE_TIMEOUT] doubleValue]];
  }
  if (nil != [settings objectForKey:FB_SETTING_ANIMATION_COOL_OFF_TIMEOUT]) {
    [FBConfiguration setAnimationCoolOffTimeout:[[settings objectForKey:FB_SETTING_ANIMATION_COOL_OFF_TIMEOUT] doubleValue]];
  }
  if ([[settings objectForKey:FB_SETTING_DEFAULT_ALERT_ACTION] isKindOfClass:NSString.class]) {
    request.session.defaultAlertAction = [settings[FB_SETTING_DEFAULT_ALERT_ACTION] lowercaseString];
  }
  
#if !TARGET_OS_TV
  if (nil != [settings objectForKey:FB_SETTING_SCREENSHOT_ORIENTATION]) {
    NSError *error;
    if (![FBConfiguration setScreenshotOrientation:(NSString *)[settings objectForKey:FB_SETTING_SCREENSHOT_ORIENTATION]
                                             error:&error]) {
      return FBResponseWithStatus([FBCommandStatus invalidArgumentErrorWithMessage:error.description traceback:nil]);
    }
    
    
  }
#endif
  
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
    @"sessionId" :  [FBSession activeSession].identifier ?: NSNull.null,
    @"capabilities" : FBSessionCommands.currentCapabilities
  };
}

+ (NSDictionary *)currentCapabilities
{
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
