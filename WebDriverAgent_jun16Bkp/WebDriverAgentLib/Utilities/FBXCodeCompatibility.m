/**
 * Copyright (c) 2015-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#import "FBXCodeCompatibility.h"

#import "FBConfiguration.h"
#import "FBErrorBuilder.h"
#import "FBExceptionHandler.h"
#import "FBLogger.h"
#import "XCUIApplication+FBHelpers.h"
#import "XCUIElementQuery.h"
#import "FBXCTestDaemonsProxy.h"
#import "XCTestManager_ManagerInterface-Protocol.h"


static const NSTimeInterval APP_STATE_CHANGE_TIMEOUT = 5.0;

static BOOL FBShouldUseOldElementRootSelector = NO;
static dispatch_once_t onceRootElementToken;
@implementation XCElementSnapshot (FBCompatibility)

- (XCElementSnapshot *)fb_rootElement
{
  dispatch_once(&onceRootElementToken, ^{
    FBShouldUseOldElementRootSelector = [self respondsToSelector:@selector(_rootElement)];
  });
  if (FBShouldUseOldElementRootSelector) {
    return [self _rootElement];
  }
  return [self rootElement];
}

+ (id)fb_axAttributesForElementSnapshotKeyPathsIOS:(id)arg1
{
  return [self.class axAttributesForElementSnapshotKeyPaths:arg1 isMacOS:NO];
}

+ (nullable SEL)fb_attributesForElementSnapshotKeyPathsSelector
{
  static SEL attributesForElementSnapshotKeyPathsSelector = nil;
  static dispatch_once_t attributesForElementSnapshotKeyPathsSelectorToken;
  dispatch_once(&attributesForElementSnapshotKeyPathsSelectorToken, ^{
    if ([self.class respondsToSelector:@selector(snapshotAttributesForElementSnapshotKeyPaths:)]) {
      attributesForElementSnapshotKeyPathsSelector = @selector(snapshotAttributesForElementSnapshotKeyPaths:);
    } else if ([self.class respondsToSelector:@selector(axAttributesForElementSnapshotKeyPaths:)]) {
      attributesForElementSnapshotKeyPathsSelector = @selector(axAttributesForElementSnapshotKeyPaths:);
    } else if ([self.class respondsToSelector:@selector(axAttributesForElementSnapshotKeyPaths:isMacOS:)]) {
      attributesForElementSnapshotKeyPathsSelector = @selector(fb_axAttributesForElementSnapshotKeyPathsIOS:);
    }
  });
  return attributesForElementSnapshotKeyPathsSelector;
}

@end


NSString *const FBApplicationMethodNotSupportedException = @"FBApplicationMethodNotSupportedException";

static BOOL FBShouldUseOldAppWithPIDSelector = NO;
static dispatch_once_t onceAppWithPIDToken;
static BOOL FBCanUseActivate = NO;
static dispatch_once_t onceActivate;
@implementation XCUIApplication (FBCompatibility)

+ (instancetype)fb_applicationWithPID:(pid_t)processID
{
  dispatch_once(&onceAppWithPIDToken, ^{
    FBShouldUseOldAppWithPIDSelector = [XCUIApplication respondsToSelector:@selector(appWithPID:)];
  });
  if (FBShouldUseOldAppWithPIDSelector) {
    return [self appWithPID:processID];
  }
  return [self applicationWithPID:processID];
}

- (void)fb_activate
{
  if(SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"13.4"))
  {
    [self activate];
    if (![self fb_waitForAppElement:APP_STATE_CHANGE_TIMEOUT]) {
      [FBLogger logFmt:@"The application '%@' is not running in foreground after %.2f seconds", self.bundleID, APP_STATE_CHANGE_TIMEOUT];
    }
  }
  else{
    if (!self.fb_isActivateSupported) {
      [[NSException exceptionWithName:FBApplicationMethodNotSupportedException reason:@"'activate' method is not supported by the current iOS SDK" userInfo:@{}] raise];
    }
    [self activate];
  }
  
}

- (NSUInteger)fb_state
{
  return [[self valueForKey:@"state"] intValue];
}

- (BOOL)fb_isActivateSupported
{
  dispatch_once(&onceActivate, ^{
    FBCanUseActivate = [self respondsToSelector:@selector(activate)];
  });
  return FBCanUseActivate;
}

@end


static BOOL FBShouldUseFirstMatchSelector = NO;
static dispatch_once_t onceFirstMatchToken;
@implementation XCUIElementQuery (FBCompatibility)

- (XCUIElement *)fb_firstMatch
{
  dispatch_once(&onceFirstMatchToken, ^{
    // Unfortunately, firstMatch property does not work properly if
    // the lookup is not executed in application context:
    // https://github.com/appium/appium/issues/10101
    //    FBShouldUseFirstMatchSelector = [self respondsToSelector:@selector(firstMatch)];
    FBShouldUseFirstMatchSelector = NO;
  });
  if (FBShouldUseFirstMatchSelector) {
    XCUIElement* result = self.firstMatch;
    return result.exists ? result : nil;
  }
  if (!self.element.exists) {
    return nil;
  }
  return self.allElementsBoundByAccessibilityElement.firstObject;
}

@end
