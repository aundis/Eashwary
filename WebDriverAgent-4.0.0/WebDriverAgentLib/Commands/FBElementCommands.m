/**
 * Copyright (c) 2015-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#import "FBElementCommands.h"

#import "FBApplication.h"
#import "FBConfiguration.h"
#import "FBKeyboard.h"
#import "FBPredicate.h"
#import "FBRoute.h"
#import "FBRouteRequest.h"
#import "FBRunLoopSpinner.h"
#import "FBElementCache.h"
#import "FBErrorBuilder.h"
#import "FBSession.h"
#import "FBApplication.h"
#import "FBElementUtils.h"
#import "FBMacros.h"
#import "FBMathUtils.h"
#import "FBRuntimeUtils.h"
#import "NSPredicate+FBFormat.h"
#import "XCTestPrivateSymbols.h"
#import "XCUICoordinate.h"
#import "XCUIDevice.h"
#import "XCUIElement+FBIsVisible.h"
#import "XCUIElement+FBPickerWheel.h"
#import "XCUIElement+FBScrolling.h"
#import "XCUIElement+FBTap.h"
#import "XCUIElement+FBForceTouch.h"
#import "XCUIElement+FBSwiping.h"
#import "XCUIElement+FBTyping.h"
#import "XCUIElement+FBUtilities.h"
#import "XCUIElement+FBWebDriverAttributes.h"
#import "XCUIElement+FBTVFocuse.h"
#import "FBElementTypeTransformer.h"
#import "XCUIElement.h"
#import "XCUIElementQuery.h"
#import "FBXCodeCompatibility.h"


#import "FBTouchActionCommands.h"

#import "FBApplication.h"
#import "FBRoute.h"
#import "FBRouteRequest.h"
#import "FBSession.h"
#import "XCUIApplication+FBTouchAction.h"
#import "FBScreen.h"


@interface FBElementCommands ()
@end

@implementation FBElementCommands

#pragma mark - <FBCommandHandler>

+ (NSArray *)routes
{
  return
  @[
    [[FBRoute GET:@"/window/size"] respondWithTarget:self action:@selector(handleGetWindowSize:)],
    [[FBRoute GET:@"/element/:uuid/enabled"] respondWithTarget:self action:@selector(handleGetEnabled:)],
    [[FBRoute POST:@"/startstream"] respondWithTarget:self action:@selector(handleStartStream:)],
    [[FBRoute POST:@"/configurestream"] respondWithTarget:self action:@selector(handleconfigureStream:)],
    [[FBRoute POST:@"/stopstream"] respondWithTarget:self action:@selector(handleStopStream:)],
    [[FBRoute GET:@"/element/:uuid/rect"] respondWithTarget:self action:@selector(handleGetRect:)],
    [[FBRoute GET:@"/element/:uuid/attribute/:name"] respondWithTarget:self action:@selector(handleGetAttribute:)],
    [[FBRoute GET:@"/element/:uuid/text"] respondWithTarget:self action:@selector(handleGetText:)],
    [[FBRoute GET:@"/element/:uuid/displayed"] respondWithTarget:self action:@selector(handleGetDisplayed:)],
    [[FBRoute GET:@"/element/:uuid/selected"] respondWithTarget:self action:@selector(handleGetSelected:)],
    [[FBRoute GET:@"/element/:uuid/name"] respondWithTarget:self action:@selector(handleGetName:)],
    [[FBRoute POST:@"/element/:uuid/value"] respondWithTarget:self action:@selector(handleSetValue:)],
    [[FBRoute POST:@"/element/:uuid/click"] respondWithTarget:self action:@selector(handleClick:)],
    [[FBRoute POST:@"/element/:uuid/clear"] respondWithTarget:self action:@selector(handleClear:)],
    // W3C element screenshot
    [[FBRoute GET:@"/element/:uuid/screenshot"] respondWithTarget:self action:@selector(handleElementScreenshot:)],
    // JSONWP element screenshot
    [[FBRoute GET:@"/screenshot/:uuid"] respondWithTarget:self action:@selector(handleElementScreenshot:)],
    [[FBRoute GET:@"/wda/element/:uuid/accessible"] respondWithTarget:self action:@selector(handleGetAccessible:)],
    [[FBRoute GET:@"/wda/element/:uuid/accessibilityContainer"] respondWithTarget:self action:@selector(handleGetIsAccessibilityContainer:)],
#if TARGET_OS_TV
    [[FBRoute GET:@"/element/:uuid/attribute/focused"] respondWithTarget:self action:@selector(handleGetFocused:)],
    [[FBRoute POST:@"/wda/element/:uuid/focuse"] respondWithTarget:self action:@selector(handleFocuse:)],
#else
    [[FBRoute POST:@"/wda/element/:uuid/swipe"] respondWithTarget:self action:@selector(handleSwipe:)],
    [[FBRoute POST:@"/wda/element/:uuid/pinch"] respondWithTarget:self action:@selector(handlePinch:)],
    [[FBRoute POST:@"/wda/element/:uuid/rotate"] respondWithTarget:self action:@selector(handleRotate:)],
    [[FBRoute POST:@"/wda/element/:uuid/doubleTap"] respondWithTarget:self action:@selector(handleDoubleTapCoordinate:)],
    [[FBRoute POST:@"/wda/element/:uuid/twoFingerTap"] respondWithTarget:self action:@selector(handleTwoFingerTap:)],
    [[FBRoute POST:@"/wda/element/:uuid/tapWithNumberOfTaps"] respondWithTarget:self action:@selector(handleTapWithNumberOfTaps:)],
    [[FBRoute POST:@"/wda/element/:uuid/touchAndHold"] respondWithTarget:self action:@selector(handleTouchAndHold:)],
    [[FBRoute POST:@"/wda/element/:uuid/scroll"] respondWithTarget:self action:@selector(handleScroll:)],
    [[FBRoute POST:@"/wda/element/:uuid/dragfromtoforduration"] respondWithTarget:self action:@selector(handleDrag:)],
    [[FBRoute POST:@"/wda/dragfromtoforduration"] respondWithTarget:self action:@selector(handleDragCoordinate:)],
    [[FBRoute POST:@"/wda/tap/:uuid"] respondWithTarget:self action:@selector(handleTap:)],
    [[FBRoute POST:@"/wda/touchAndHold"] respondWithTarget:self action:@selector(handleTouchAndHoldCoordinate:)],
    [[FBRoute POST:@"/wda/doubleTap"] respondWithTarget:self action:@selector(handleDoubleTapCoordinate:)],
    [[FBRoute POST:@"/wda/pickerwheel/:uuid/select"] respondWithTarget:self action:@selector(handleWheelSelect:)],
#endif
    [[FBRoute POST:@"/wda/keys"] respondWithTarget:self action:@selector(handleKeys:)],
    [[FBRoute POST:@"/wda/element/:uuid/forceTouch"] respondWithTarget:self action:@selector(handleForceTouch:)],
  ];
}


#pragma mark - Commands

+ (id<FBResponsePayload>)handleStartStream:(FBRouteRequest *)request
{
  
  NSError *error;
  
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *bundleID = @"com.pcloudy.Screencaster";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithUnknownErrorFormat(@"'bundleId' desired capability not provided");
  }
  
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  // app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];
  [app.buttons[@"ModuleIcon"] tap];
  
  [NSThread sleepForTimeInterval:0.5];
  
  XCUIApplication *application = request.session.activeApplication;
  
  [NSThread sleepForTimeInterval:4.0];

  if (@available(iOS 14, *)) {
    [application.buttons[@"Start Broadcast"] tap];
  }else{
    [application.buttons[@"Start Broadcast"] tap];
    [app.staticTexts[@"Start Broadcast"] tap];
  }

  

  [NSThread sleepForTimeInterval:0.5];
  [[XCUIDevice sharedDevice] fb_goToHomescreenWithError:&error];
  [app terminate];
  return FBResponseWithObject( @"Started");
}




+ (id<FBResponsePayload>)handleStopStream:(FBRouteRequest *)request
{
  
  NSError *error;
  
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *bundleID = @"com.pcloudy.Screencaster";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithUnknownErrorFormat(@"'bundleId' desired capability not provided");
  }
 
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  // app.fb_shouldWaitForQuiescence = FBConfiguration.shouldWaitForQuiescence;
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];

  [app.buttons[@"ModuleIcon"] tap];
  [NSThread sleepForTimeInterval:1.0];
  
  XCUIApplication *application = request.session.activeApplication;
  
  if (@available(iOS 14, *)) {
   
    if (application.buttons[@"Stop Broadcast"].enabled){
      [application.buttons[@"Stop Broadcast"] tap];
      [NSThread sleepForTimeInterval:4.0];
    }    
  }else{
    [app.staticTexts[@"Stop Broadcast"] tap];
  }
  
  
  

  [[XCUIDevice sharedDevice] fb_goToHomescreenWithError:&error];
 
  return FBResponseWithObject( @"Stoped");
  
}



+ (id<FBResponsePayload>)handleconfigureStream:(FBRouteRequest *)request
{
  
  NSError *error;
  NSUInteger frequency = 100;
  NSDictionary *requirements = request.arguments[@"desiredCapabilities"];
  NSString *bundleID = @"com.pcloudy.Screencaster";
  NSString *appPath = requirements[@"app"];
  if (!bundleID) {
    return FBResponseWithUnknownErrorFormat(@"'bundleId' desired capability not provided");
  }
  NSString *url = request.arguments[@"url"];
  NSString *name = request.arguments[@"name"];
  FBApplication *app = [[FBApplication alloc] initPrivateWithPath:appPath bundleID:bundleID];
  app.launchArguments = (NSArray<NSString *> *)requirements[@"arguments"] ?: @[];
  app.launchEnvironment = (NSDictionary <NSString *, NSString *> *)requirements[@"environment"] ?: @{};
  [app launch];
  
  [app terminate];
  [app launch];
  [NSThread sleepForTimeInterval:0.5];
  [app.tables.cells.staticTexts[@"Video"] tap];

  [ app.switches[@"Vertical stream"] tap];
  [NSThread sleepForTimeInterval:0.2];
  [app.tables.cells.staticTexts[@"Screen orientation"] tap];
  [NSThread sleepForTimeInterval:0.2];
  [app.tables.cells.staticTexts[@"Determine at start"] tap];
  [NSThread sleepForTimeInterval:0.2];
  [app.navigationBars.buttons[@"Back"] tap];
  [NSThread sleepForTimeInterval:0.5];
  [app.tables.cells.staticTexts[@"Audio"] tap];
  [app.navigationBars.buttons[@"Back"] tap];
  [app.tables.cells.staticTexts[@"Connections"] tap];
  
  if (app.buttons[@"Manage"].enabled){
    [NSThread sleepForTimeInterval:0.5];
    [app.buttons[@"Manage"] tap];
    [NSThread sleepForTimeInterval:0.5];
    [app.buttons[@"Mark All"] tap];
    [NSThread sleepForTimeInterval:0.5];
    [app.buttons[@"Delete"] tap];
    [NSThread sleepForTimeInterval:0.5];
    [app.alerts.buttons[@"Delete"] tap];
    [NSThread sleepForTimeInterval:0.5];
  }
  
  [app.navigationBars.buttons[@"Add"] tap];
  [NSThread sleepForTimeInterval:1.5];
  [app.staticTexts[@"Name"] tap];
  [NSThread sleepForTimeInterval:1.5];
  [FBKeyboard typeText:name frequency:frequency error:&error];
  
  
  [app.staticTexts[@"URL"] tap];
  [NSThread sleepForTimeInterval:0.5];
  [FBKeyboard typeText:url frequency:frequency error:&error];

  
  [app.staticTexts[@"Target type"] tap];
  [NSThread sleepForTimeInterval:1.0];
  [app.staticTexts[@"RTMP authorization"] tap];
  
  [app.staticTexts[@"Login"] tap];
  [NSThread sleepForTimeInterval:0.5];
  [FBKeyboard typeText:@"admin" frequency:frequency error:&error];
  
  [app.staticTexts[@"Password"] tap];
  [NSThread sleepForTimeInterval:0.5];
  [FBKeyboard typeText:@"admin" frequency:frequency error:&error];

  [app.navigationBars.buttons[@"Save"] tap];
  
  [app.navigationBars.buttons[@"Back"] tap];
  
  [app terminate];

  return FBResponseWithObject(@"Configured");
}



+ (id<FBResponsePayload>)handleGetEnabled:(FBRouteRequest *)request
{
  FBElementCache *elementCache = request.session.elementCache;
  XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
  BOOL isEnabled = element.lastSnapshot.isWDEnabled;
  return FBResponseWithObject(isEnabled ? @YES : @NO);
}

+ (id<FBResponsePayload>)handleGetRect:(FBRouteRequest *)request
{
  FBElementCache *elementCache = request.session.elementCache;
  XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
  return FBResponseWithObject(element.lastSnapshot.wdRect);
}

+ (id<FBResponsePayload>)handleGetAttribute:(FBRouteRequest *)request
{
  FBElementCache *elementCache = request.session.elementCache;
  NSString *attributeName = request.parameters[@"name"];
  NSString *wdAttributeName = [FBElementUtils wdAttributeNameForAttributeName:attributeName];
  NSArray *additionalAttributes = nil;
  NSNumber *maxDepth = nil;
  if ([wdAttributeName isEqualToString:FBStringify(XCUIElement, isWDVisible)]) {
    additionalAttributes = @[FB_XCAXAIsVisibleAttributeName];
    maxDepth = @1;
  } else if ([wdAttributeName isEqualToString:FBStringify(XCUIElement, isWDEnabled)]) {
    additionalAttributes = @[FB_XCAXAIsElementAttributeName];
    maxDepth = @1;
  } else if ([wdAttributeName isEqualToString:FBStringify(XCUIElement, isWDAccessibilityContainer)]) {
    additionalAttributes = @[FB_XCAXAIsElementAttributeName];
  }
  XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]
                       resolveForAdditionalAttributes:additionalAttributes
                                          andMaxDepth:maxDepth];
  id attributeValue = [element.lastSnapshot fb_valueForWDAttributeName:attributeName];
  return FBResponseWithObject(attributeValue ?: [NSNull null]);
}

+ (id<FBResponsePayload>)handleGetText:(FBRouteRequest *)request
{
  FBElementCache *elementCache = request.session.elementCache;
  XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
  id text = FBFirstNonEmptyValue(element.lastSnapshot.wdValue, element.lastSnapshot.wdLabel);
  return FBResponseWithObject(text ?: @"");
}

+ (id<FBResponsePayload>)handleGetDisplayed:(FBRouteRequest *)request
{
  FBElementCache *elementCache = request.session.elementCache;
  XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]
                       resolveForAdditionalAttributes:@[FB_XCAXAIsVisibleAttributeName]
                                          andMaxDepth:@1];
  return FBResponseWithObject(@(element.lastSnapshot.isWDVisible));
}

+ (id<FBResponsePayload>)handleGetAccessible:(FBRouteRequest *)request
{
  FBElementCache *elementCache = request.session.elementCache;
  XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]
                       resolveForAdditionalAttributes:@[FB_XCAXAIsElementAttributeName]
                                          andMaxDepth:@1];
  return FBResponseWithObject(@(element.lastSnapshot.isWDAccessible));
}

+ (id<FBResponsePayload>)handleGetIsAccessibilityContainer:(FBRouteRequest *)request
{
  FBElementCache *elementCache = request.session.elementCache;
  XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]
                       resolveForAdditionalAttributes:@[FB_XCAXAIsElementAttributeName]
                                          andMaxDepth:nil];
  return FBResponseWithObject(@(element.lastSnapshot.isWDAccessibilityContainer));
}

+ (id<FBResponsePayload>)handleGetName:(FBRouteRequest *)request
{
  FBElementCache *elementCache = request.session.elementCache;
  XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
  return FBResponseWithObject(element.lastSnapshot.wdType);
}


+ (id<FBResponsePayload>)handleKeys:(FBRouteRequest *)request
{
  NSString *textToType = [request.arguments[@"value"] componentsJoinedByString:@""];
  
  //NSLog(textToType);
  NSUInteger frequency = 100;// [request.arguments[@"frequency"] unsignedIntegerValue] ?: [FBConfiguration maxTypingFrequency];
  
  dispatch_async(dispatch_get_main_queue(), ^{
    NSError *error;
    [FBKeyboard typeText:textToType frequency:frequency error:&error];
    });
  return FBResponseWithOK();
}


+ (id<FBResponsePayload>)handleGetSelected:(FBRouteRequest *)request
{
  FBElementCache *elementCache = request.session.elementCache;
  XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
  return FBResponseWithObject(@(element.lastSnapshot.wdSelected));
}

+ (id<FBResponsePayload>)handleSetValue:(FBRouteRequest *)request
{
  FBElementCache *elementCache = request.session.elementCache;
  XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
  id value = request.arguments[@"value"] ?: request.arguments[@"text"];
  if (!value) {
    return FBResponseWithStatus([FBCommandStatus invalidArgumentErrorWithMessage:@"Neither 'value' nor 'text' parameter is provided" traceback:nil]);
  }
  NSString *textToType = [value isKindOfClass:NSArray.class]
  ? [value componentsJoinedByString:@""]
  : value;
  XCUIElementType elementType = element.lastSnapshot.elementType;
#if !TARGET_OS_TV
  if (elementType == XCUIElementTypePickerWheel) {
    [element adjustToPickerWheelValue:textToType];
    return FBResponseWithOK();
  }
#endif
  if (elementType == XCUIElementTypeSlider) {
    CGFloat sliderValue = textToType.floatValue;
    if (sliderValue < 0.0 || sliderValue > 1.0 ) {
      return FBResponseWithStatus([FBCommandStatus invalidArgumentErrorWithMessage:@"Value of slider should be in 0..1 range" traceback:nil]);
    }
    [element adjustToNormalizedSliderPosition:sliderValue];
    return FBResponseWithOK();
  }
  NSUInteger frequency = (NSUInteger)[request.arguments[@"frequency"] longLongValue] ?: [FBConfiguration maxTypingFrequency];
  NSError *error = nil;
  if (![element fb_typeText:textToType
                shouldClear:NO
                  frequency:frequency
                      error:&error]) {
    return FBResponseWithStatus([FBCommandStatus invalidElementStateErrorWithMessage:error.description traceback:nil]);
  }
  return FBResponseWithOK();
}

+ (id<FBResponsePayload>)handleClick:(FBRouteRequest *)request
{
  FBElementCache *elementCache = request.session.elementCache;
  XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
  NSError *error = nil;
#if TARGET_OS_IOS
  if (![element fb_tapWithError:&error]) {
#elif TARGET_OS_TV
    if (![element fb_selectWithError:&error]) {
#endif
      return FBResponseWithStatus([FBCommandStatus invalidElementStateErrorWithMessage:error.description traceback:nil]);
    }
    return FBResponseWithOK();
  }
  
  + (id<FBResponsePayload>)handleClear:(FBRouteRequest *)request
  {
    FBElementCache *elementCache = request.session.elementCache;
    XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
    NSError *error;
    if (![element fb_clearTextWithError:&error]) {
      return FBResponseWithStatus([FBCommandStatus invalidElementStateErrorWithMessage:error.description traceback:nil]);
    }
    return FBResponseWithOK();
  }
  
#if TARGET_OS_TV
  + (id<FBResponsePayload>)handleGetFocused:(FBRouteRequest *)request
  {
    // `BOOL isFocused = [elementCache elementForUUID:request.parameters[@"uuid"]];`
    // returns wrong true/false after moving focus by key up/down, for example.
    // Thus, ensure the focus compares the status with `fb_focusedElement`.
    BOOL isFocused = NO;
    XCUIElement *focusedElement = request.session.activeApplication.fb_focusedElement;
    if (focusedElement != nil) {
      FBElementCache *elementCache = request.session.elementCache;
      NSString *focusedUUID = [elementCache storeElement:focusedElement];
      if (focusedUUID && [focusedUUID isEqualToString:(id)request.parameters[@"uuid"]]) {
        isFocused = YES;
      }
    }
    
    return FBResponseWithObject(@(isFocused));
  }
  
  + (id<FBResponsePayload>)handleFocuse:(FBRouteRequest *)request
  {
    FBElementCache *elementCache = request.session.elementCache;
    XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
    NSError *error;
    if (![element fb_setFocusWithError:&error]) {
      return FBResponseWithStatus([FBCommandStatus invalidElementStateErrorWithMessage:error.description traceback:nil]);
    }
    return FBResponseWithStatus([FBCommandStatus okWithValue: FBDictionaryResponseWithElement(element, FBConfiguration.shouldUseCompactResponses)]);
  }
#else
  + (id<FBResponsePayload>)handleDoubleTap:(FBRouteRequest *)request
  {
    FBElementCache *elementCache = request.session.elementCache;
    XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
    [element doubleTap];
    return FBResponseWithOK();
  }
  
  + (id<FBResponsePayload>)handleDoubleTapCoordinate:(FBRouteRequest *)request
  {
    CGPoint doubleTapPoint = CGPointMake((CGFloat)[request.arguments[@"x"] doubleValue], (CGFloat)[request.arguments[@"y"] doubleValue]);
    XCUICoordinate *doubleTapCoordinate = [self.class gestureCoordinateWithCoordinate:doubleTapPoint application:request.session.activeApplication shouldApplyOrientationWorkaround:isSDKVersionLessThan(@"11.0")];
    [doubleTapCoordinate doubleTap];
    return FBResponseWithOK();
  }
  
  + (id<FBResponsePayload>)handleTwoFingerTap:(FBRouteRequest *)request
  {
    FBElementCache *elementCache = request.session.elementCache;
    XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
    [element twoFingerTap];
    return FBResponseWithOK();
  }
  
  + (id<FBResponsePayload>)handleTapWithNumberOfTaps:(FBRouteRequest *)request
  {
    FBElementCache *elementCache = request.session.elementCache;
    if (nil == request.arguments[@"numberOfTaps"] || nil == request.arguments[@"numberOfTouches"]) {
      return FBResponseWithStatus([FBCommandStatus invalidArgumentErrorWithMessage:@"Both 'numberOfTaps' and 'numberOfTouches' arguments must be provided"
                                                                         traceback:nil]);
    }
    XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
    [element tapWithNumberOfTaps:[request.arguments[@"numberOfTaps"] integerValue]
                 numberOfTouches:[request.arguments[@"numberOfTouches"] integerValue]];
    return FBResponseWithOK();
  }
  
  + (id<FBResponsePayload>)handleTouchAndHold:(FBRouteRequest *)request
  {
    FBElementCache *elementCache = request.session.elementCache;
    XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
    [element pressForDuration:[request.arguments[@"duration"] doubleValue]];
    return FBResponseWithOK();
  }
  
  + (id<FBResponsePayload>)handleTouchAndHoldCoordinate:(FBRouteRequest *)request
  {
    CGPoint touchPoint = CGPointMake((CGFloat)[request.arguments[@"x"] doubleValue], (CGFloat)[request.arguments[@"y"] doubleValue]);
    XCUICoordinate *pressCoordinate = [self.class gestureCoordinateWithCoordinate:touchPoint application:request.session.activeApplication shouldApplyOrientationWorkaround:isSDKVersionLessThan(@"11.0")];
    [pressCoordinate pressForDuration:[request.arguments[@"duration"] doubleValue]];
    return FBResponseWithOK();
  }
  
  + (id<FBResponsePayload>)handleScroll:(FBRouteRequest *)request
  {
    FBElementCache *elementCache = request.session.elementCache;
    XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
    // Using presence of arguments as a way to convey control flow seems like a pretty bad idea but it's
    // what ios-driver did and sadly, we must copy them.
    NSString *const name = request.arguments[@"name"];
    if (name) {
      XCUIElement *childElement = [[[[element.fb_query descendantsMatchingType:XCUIElementTypeAny] matchingIdentifier:name] allElementsBoundByAccessibilityElement] lastObject];
      if (!childElement) {
        return FBResponseWithStatus([FBCommandStatus noSuchElementErrorWithMessage:[NSString stringWithFormat:@"'%@' identifier didn't match any elements", name] traceback:[NSString stringWithFormat:@"%@", NSThread.callStackSymbols]]);
      }
      return [self.class handleScrollElementToVisible:childElement withRequest:request];
    }
    
    NSString *const direction = request.arguments[@"direction"];
    if (direction) {
      NSString *const distanceString = request.arguments[@"distance"] ?: @"1.0";
      CGFloat distance = (CGFloat)distanceString.doubleValue;
      if ([direction isEqualToString:@"up"]) {
        [element fb_scrollUpByNormalizedDistance:distance];
      } else if ([direction isEqualToString:@"down"]) {
        [element fb_scrollDownByNormalizedDistance:distance];
      } else if ([direction isEqualToString:@"left"]) {
        [element fb_scrollLeftByNormalizedDistance:distance];
      } else if ([direction isEqualToString:@"right"]) {
        [element fb_scrollRightByNormalizedDistance:distance];
      }
      return FBResponseWithOK();
    }
    
    NSString *const predicateString = request.arguments[@"predicateString"];
    if (predicateString) {
      NSPredicate *formattedPredicate = [NSPredicate fb_formatSearchPredicate:[FBPredicate predicateWithFormat:predicateString]];
      XCUIElement *childElement = [[[[element.fb_query descendantsMatchingType:XCUIElementTypeAny] matchingPredicate:formattedPredicate] allElementsBoundByAccessibilityElement] lastObject];
      if (!childElement) {
        return FBResponseWithStatus([FBCommandStatus noSuchElementErrorWithMessage:[NSString stringWithFormat:@"'%@' predicate didn't match any elements", predicateString] traceback:[NSString stringWithFormat:@"%@", NSThread.callStackSymbols]]);
      }
      return [self.class handleScrollElementToVisible:childElement withRequest:request];
    }
    
    if (request.arguments[@"toVisible"]) {
      return [self.class handleScrollElementToVisible:element withRequest:request];
    }
    return FBResponseWithStatus([FBCommandStatus invalidArgumentErrorWithMessage:@"Unsupported scroll type" traceback:nil]);
  }
  
  + (id<FBResponsePayload>)handleDragCoordinate:(FBRouteRequest *)request
  {
    FBSession *session = request.session;
    CGPoint startPoint = CGPointMake((CGFloat)[request.arguments[@"fromX"] doubleValue], (CGFloat)[request.arguments[@"fromY"] doubleValue]);
    CGPoint endPoint = CGPointMake((CGFloat)[request.arguments[@"toX"] doubleValue], (CGFloat)[request.arguments[@"toY"] doubleValue]);
    NSTimeInterval duration = [request.arguments[@"duration"] doubleValue];
    //NSInteger UIOrientation = [request.arguments[@"orientation"] integerValue];
    //if(SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"10.0")){
    /* if( UIDeviceOrientationLandscapeLeft == UIOrientation){
     startPoint = FBInvertPointForApplication(startPoint, request.session.activeApplication.wdFrame.size, UIOrientation);
     endPoint = FBInvertPointForApplication(endPoint, request.session.activeApplication.wdFrame.size, UIOrientation);
     }*/
    
    XCUICoordinate *endCoordinate = [self.class gestureCoordinateWithCoordinate:endPoint application:session.activeApplication shouldApplyOrientationWorkaround:isSDKVersionLessThan(@"11.0")];
    XCUICoordinate *startCoordinate = [self.class gestureCoordinateWithCoordinate:startPoint application:session.activeApplication shouldApplyOrientationWorkaround:isSDKVersionLessThan(@"11.0")];
    [startCoordinate pressForDuration:duration thenDragToCoordinate:endCoordinate];
    return FBResponseWithOK();
  }
  
  + (id<FBResponsePayload>)handleDrag:(FBRouteRequest *)request
  {
    FBSession *session = request.session;
    FBElementCache *elementCache = session.elementCache;
    XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
    CGRect frame = element.lastSnapshot.frame;
    CGPoint startPoint = CGPointMake((CGFloat)(frame.origin.x + [request.arguments[@"fromX"] doubleValue]), (CGFloat)(frame.origin.y + [request.arguments[@"fromY"] doubleValue]));
    CGPoint endPoint = CGPointMake((CGFloat)(frame.origin.x + [request.arguments[@"toX"] doubleValue]), (CGFloat)(frame.origin.y + [request.arguments[@"toY"] doubleValue]));
    NSTimeInterval duration = [request.arguments[@"duration"] doubleValue];
    BOOL shouldApplyOrientationWorkaround = isSDKVersionGreaterThanOrEqualTo(@"10.0") && isSDKVersionLessThan(@"11.0");
    XCUICoordinate *endCoordinate = [self.class gestureCoordinateWithCoordinate:endPoint application:session.activeApplication shouldApplyOrientationWorkaround:shouldApplyOrientationWorkaround];
    XCUICoordinate *startCoordinate = [self.class gestureCoordinateWithCoordinate:startPoint application:session.activeApplication shouldApplyOrientationWorkaround:shouldApplyOrientationWorkaround];
    [startCoordinate pressForDuration:duration thenDragToCoordinate:endCoordinate];
    return FBResponseWithOK();
  }
  
  + (id<FBResponsePayload>)handleSwipe:(FBRouteRequest *)request
  {
    FBElementCache *elementCache = request.session.elementCache;
    XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
    NSString *const direction = request.arguments[@"direction"];
    if (!direction) {
      return FBResponseWithStatus([FBCommandStatus invalidArgumentErrorWithMessage:@"Missing 'direction' parameter" traceback:nil]);
    }
    NSArray<NSString *> *supportedDirections = @[@"up", @"down", @"left", @"right"];
    if (![supportedDirections containsObject:direction.lowercaseString]) {
      return FBResponseWithStatus([FBCommandStatus
                                   invalidArgumentErrorWithMessage:[NSString stringWithFormat: @"Unsupported swipe direction '%@'. Only the following directions are supported: %@", direction, supportedDirections]
                                   traceback:nil]);
    }
    [element fb_swipeWithDirection:direction.lowercaseString velocity:request.arguments[@"velocity"]];
    return FBResponseWithOK();
  }
  
  + (id<FBResponsePayload>)handleTap:(FBRouteRequest *)request
  {
    FBElementCache *elementCache = request.session.elementCache;
    CGPoint tapPoint = CGPointMake((CGFloat)[request.arguments[@"x"] doubleValue], (CGFloat)[request.arguments[@"y"] doubleValue]);
    NSInteger UIOrientation = [request.arguments[@"orientation"] integerValue];
    NSLog(@"Orientation=%ld ----------------------------------------------------", UIOrientation);
    // request.session.activeApplication.interfaceOrientation;
    //if(SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"10.0")){
    /* if( UIDeviceOrientationLandscapeLeft == UIOrientation){
     tapPoint = FBInvertPointForApplication(tapPoint, request.session.activeApplication.wdFrame.size, UIOrientation);
     }*/
    
    
    if ([elementCache hasElementWithUUID:request.parameters[@"uuid"]]) {
      XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
      NSError *error;
      if (![element fb_tapCoordinate:tapPoint error:&error]) {
        return FBResponseWithStatus([FBCommandStatus invalidElementStateErrorWithMessage:error.description
                                                                               traceback:nil]);
      }
    } else {
      XCUICoordinate *tapCoordinate = [self.class gestureCoordinateWithCoordinate:tapPoint
                                                                      application:request.session.activeApplication
                                                 shouldApplyOrientationWorkaround:isSDKVersionLessThan(@"11.0")];
      [tapCoordinate tap];
    }
    return FBResponseWithOK();
  }
  
  + (id<FBResponsePayload>)handlePinch:(FBRouteRequest *)request
  {
    FBElementCache *elementCache = request.session.elementCache;
    XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
    CGFloat scale = (CGFloat)[request.arguments[@"scale"] doubleValue];
    CGFloat velocity = (CGFloat)[request.arguments[@"velocity"] doubleValue];
    [element pinchWithScale:scale velocity:velocity];
    return FBResponseWithOK();
  }
  
  + (id<FBResponsePayload>)handleRotate:(FBRouteRequest *)request
  {
    FBElementCache *elementCache = request.session.elementCache;
    XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
    CGFloat rotation = (CGFloat)[request.arguments[@"rotation"] doubleValue];
    CGFloat velocity = (CGFloat)[request.arguments[@"velocity"] doubleValue];
    [element rotate:rotation withVelocity:velocity];
    return FBResponseWithOK();
  }
#endif
  
  + (id<FBResponsePayload>)handleForceTouch:(FBRouteRequest *)request
  {
    FBElementCache *elementCache = request.session.elementCache;
    XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
    double pressure = [request.arguments[@"pressure"] doubleValue];
    double duration = [request.arguments[@"duration"] doubleValue];
    NSError *error;
    if (nil != request.arguments[@"x"] && nil != request.arguments[@"y"]) {
      CGPoint forceTouchPoint = CGPointMake((CGFloat)[request.arguments[@"x"] doubleValue], (CGFloat)[request.arguments[@"y"] doubleValue]);
      if (![element fb_forceTouchCoordinate:forceTouchPoint pressure:pressure duration:duration error:&error]) {
        return FBResponseWithStatus([FBCommandStatus invalidElementStateErrorWithMessage:error.description traceback:nil]);
      }
    } else {
      if (![element fb_forceTouchWithPressure:pressure duration:duration error:&error]) {
        return FBResponseWithStatus([FBCommandStatus invalidElementStateErrorWithMessage:error.description traceback:nil]);
      }
    }
    return FBResponseWithOK();
  }
  
  
  + (id<FBResponsePayload>)handleGetWindowSize:(FBRouteRequest *)request
  {
#if TARGET_OS_TV
    CGSize screenSize = request.session.activeApplication.frame.size;
#else
    NSInteger Orientation = request.session.activeApplication.interfaceOrientation;
    CGRect frame = request.session.activeApplication.wdFrame;
    CGSize screenSize = FBAdjustDimensionsForApplication(frame.size, request.session.activeApplication.interfaceOrientation);
#endif
    return FBResponseWithObject(@{
      @"width": @(screenSize.width),
      @"height": @(screenSize.height),
      @"orientation": @(Orientation)
      
      
    });
  }
  
  + (id<FBResponsePayload>)handleElementScreenshot:(FBRouteRequest *)request
  {
    FBElementCache *elementCache = request.session.elementCache;
    XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
    NSError *error;
    NSData *screenshotData = [element fb_screenshotWithError:&error];
    if (nil == screenshotData) {
      return FBResponseWithStatus([FBCommandStatus unableToCaptureScreenErrorWithMessage:error.description traceback:nil]);
    }
    NSString *screenshot = [screenshotData base64EncodedStringWithOptions:NSDataBase64Encoding64CharacterLineLength];
    return FBResponseWithObject(screenshot);
  }
  
  
#if !TARGET_OS_TV
  static const CGFloat DEFAULT_OFFSET = (CGFloat)0.2;
  
  + (id<FBResponsePayload>)handleWheelSelect:(FBRouteRequest *)request
  {
    FBElementCache *elementCache = request.session.elementCache;
    XCUIElement *element = [elementCache elementForUUID:(NSString *)request.parameters[@"uuid"]];
    if (element.lastSnapshot.elementType != XCUIElementTypePickerWheel) {
      return FBResponseWithStatus([FBCommandStatus invalidArgumentErrorWithMessage:[NSString stringWithFormat:@"The element is expected to be a valid Picker Wheel control. '%@' was given instead", element.wdType] traceback:[NSString stringWithFormat:@"%@", NSThread.callStackSymbols]]);
    }
    NSString* order = [request.arguments[@"order"] lowercaseString];
    CGFloat offset = DEFAULT_OFFSET;
    if (request.arguments[@"offset"]) {
      offset = (CGFloat)[request.arguments[@"offset"] doubleValue];
      if (offset <= 0.0 || offset > 0.5) {
        return FBResponseWithStatus([FBCommandStatus invalidArgumentErrorWithMessage:[NSString stringWithFormat:@"'offset' value is expected to be in range (0.0, 0.5]. '%@' was given instead", request.arguments[@"offset"]] traceback:[NSString stringWithFormat:@"%@", NSThread.callStackSymbols]]);
      }
    }
    BOOL isSuccessful = false;
    NSError *error;
    if ([order isEqualToString:@"next"]) {
      isSuccessful = [element fb_selectNextOptionWithOffset:offset error:&error];
    } else if ([order isEqualToString:@"previous"]) {
      isSuccessful = [element fb_selectPreviousOptionWithOffset:offset error:&error];
    } else {
      return FBResponseWithStatus([FBCommandStatus invalidArgumentErrorWithMessage:[NSString stringWithFormat:@"Only 'previous' and 'next' order values are supported. '%@' was given instead", request.arguments[@"order"]] traceback:[NSString stringWithFormat:@"%@", NSThread.callStackSymbols]]);
    }
    if (!isSuccessful) {
      return FBResponseWithStatus([FBCommandStatus invalidElementStateErrorWithMessage:error.description traceback:nil]);
    }
    return FBResponseWithOK();
  }
  
#pragma mark - Helpers
  
  + (id<FBResponsePayload>)handleScrollElementToVisible:(XCUIElement *)element withRequest:(FBRouteRequest *)request
  {
    NSError *error;
    if (!element.exists) {
      return FBResponseWithStatus([FBCommandStatus elementNotVisibleErrorWithMessage:@"Can't scroll to element that does not exist" traceback:[NSString stringWithFormat:@"%@", NSThread.callStackSymbols]]);
    }
    if (![element fb_scrollToVisibleWithError:&error]) {
      return FBResponseWithStatus([FBCommandStatus invalidElementStateErrorWithMessage:error.description traceback:[NSString stringWithFormat:@"%@", NSThread.callStackSymbols]]);
    }
    return FBResponseWithOK();
  }
  
  /**
   Returns gesture coordinate for the application based on absolute coordinate
   
   @param coordinate absolute screen coordinates
   @param application the instance of current application under test
   @shouldApplyOrientationWorkaround whether to apply orientation workaround. This is to
   handle XCTest bug where it does not translate screen coordinates for elements if
   screen orientation is different from the default one (which is portrait).
   Different iOS version have different behavior, for example iOS 9.3 returns correct
   coordinates for elements in landscape, but iOS 10.0+ returns inverted coordinates as if
   the current screen orientation would be portrait.
   @return translated gesture coordinates ready to be passed to XCUICoordinate methods
   */
  + (XCUICoordinate *)gestureCoordinateWithCoordinate:(CGPoint)coordinate application:(XCUIApplication *)application shouldApplyOrientationWorkaround:(BOOL)shouldApplyOrientationWorkaround
  {
    CGPoint point = coordinate;
    if (shouldApplyOrientationWorkaround) {
      point = FBInvertPointForApplication(coordinate, application.frame.size, application.interfaceOrientation);
    }
    
    /**
     If SDK >= 11, the tap coordinate based on application is not correct when
     the application orientation is landscape and
     tapX > application portrait width or tapY > application portrait height.
     Pass the window element to the method [FBElementCommands gestureCoordinateWithCoordinate:element:]
     will resolve the problem.
     More details about the bug, please see the following issues:
     #705: https://github.com/facebook/WebDriverAgent/issues/705
     #798: https://github.com/facebook/WebDriverAgent/issues/798
     #856: https://github.com/facebook/WebDriverAgent/issues/856
     Notice: On iOS 10, if the application is not launched by wda, no elements will be found.
     See issue #732: https://github.com/facebook/WebDriverAgent/issues/732
     */
    XCUIElement *element = application;
    if (isSDKVersionGreaterThanOrEqualTo(@"11.0")) {
      XCUIElement *window = application.windows.fb_firstMatch;
      if (window) {
        element = window;
        XCElementSnapshot *snapshot = element.fb_cachedSnapshot ?: element.fb_takeSnapshot;
        point.x -= snapshot.frame.origin.x;
        point.y -= snapshot.frame.origin.y;
      }
    }
    return [self gestureCoordinateWithCoordinate:point element:element];
  }
  
  /**
   Returns gesture coordinate based on the specified element.
   
   @param coordinate absolute coordinates based on the element
   @param element the element in the current application under test
   @return translated gesture coordinates ready to be passed to XCUICoordinate methods
   */
  + (XCUICoordinate *)gestureCoordinateWithCoordinate:(CGPoint)coordinate element:(XCUIElement *)element
  {
    XCUICoordinate *appCoordinate = [[XCUICoordinate alloc] initWithElement:element normalizedOffset:CGVectorMake(0, 0)];
    return [[XCUICoordinate alloc] initWithCoordinate:appCoordinate pointsOffset:CGVectorMake(coordinate.x, coordinate.y)];
  }
#endif
  
  @end
