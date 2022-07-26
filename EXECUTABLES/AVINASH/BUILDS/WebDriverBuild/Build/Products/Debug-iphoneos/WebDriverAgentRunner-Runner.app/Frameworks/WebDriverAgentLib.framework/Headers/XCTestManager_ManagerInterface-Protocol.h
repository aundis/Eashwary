//
//     Generated by class-dump 3.5 (64 bit).
//
//     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2013 by Steve Nygard.
//

#import <UIKit/UIKit.h>

@class NSArray, NSDictionary, NSNumber, NSString, NSUUID, XCAccessibilityElement, XCDeviceEvent, XCSynthesizedEventRecord, XCTouchGesture, NSXPCListenerEndpoint, XCElementSnapshot;

@protocol XCTestManager_ManagerInterface
- (void)_XCT_requestBundleIDForPID:(int)arg1 reply:(void (^)(NSString *, NSError *))arg2;
- (void)_XCT_loadAccessibilityWithTimeout:(double)arg1 reply:(void (^)(BOOL, NSError *))arg2;
- (void)_XCT_injectVoiceRecognitionAudioInputPaths:(NSArray *)arg1 completion:(void (^)(BOOL, NSError *))arg2;
- (void)_XCT_injectAssistantRecognitionStrings:(NSArray *)arg1 completion:(void (^)(BOOL, NSError *))arg2;
- (void)_XCT_startSiriUIRequestWithAudioFileURL:(NSURL *)arg1 completion:(void (^)(BOOL, NSError *))arg2;
- (void)_XCT_startSiriUIRequestWithText:(NSString *)arg1 completion:(void (^)(BOOL, NSError *))arg2;
- (void)_XCT_requestDTServiceHubConnectionWithReply:(void (^)(NSXPCListenerEndpoint *, NSError *))arg1;
- (void)_XCT_enableFauxCollectionViewCells:(void (^)(BOOL, NSError *))arg1;
- (void)_XCT_setAXTimeout:(double)arg1 reply:(void (^)(int))arg2;
- (void)_XCT_requestScreenshotWithReply:(void (^)(NSData *, NSError *))arg1;
- (void)_XCT_sendString:(NSString *)arg1 maximumFrequency:(NSUInteger)arg2 completion:(void (^)(NSError *))arg3;
- (void)_XCT_updateDeviceOrientation:(long long)arg1 completion:(void (^)(NSError *))arg2;
- (void)_XCT_performDeviceEvent:(XCDeviceEvent *)arg1 completion:(void (^)(NSError *))arg2;
- (void)_XCT_synthesizeEvent:(XCSynthesizedEventRecord *)arg1 completion:(void (^)(NSError *))arg2;
- (void)_XCT_requestElementAtPoint:(CGPoint)arg1 reply:(void (^)(XCAccessibilityElement *, NSError *))arg2;
- (void)_XCT_fetchParameterizedAttributeForElement:(XCAccessibilityElement *)arg1 attributes:(NSNumber *)arg2 parameter:(id)arg3 reply:(void (^)(id, NSError *))arg4;
- (void)_XCT_setAttribute:(NSNumber *)arg1 value:(id)arg2 element:(XCAccessibilityElement *)arg3 reply:(void (^)(BOOL, NSError *))arg4;
- (void)_XCT_fetchAttributesForElement:(XCAccessibilityElement *)arg1 attributes:(NSArray *)arg2 reply:(void (^)(NSDictionary *, NSError *))arg3;
- (void)_XCT_snapshotForElement:(XCAccessibilityElement *)arg1 attributes:(NSArray *)arg2 parameters:(NSDictionary *)arg3 reply:(void (^)(XCElementSnapshot *, NSError *))arg4;
- (void)_XCT_terminateApplicationWithBundleID:(NSString *)arg1 completion:(void (^)(NSError *))arg2;
- (void)_XCT_performAccessibilityAction:(int)arg1 onElement:(XCAccessibilityElement *)arg2 withValue:(id)arg3 reply:(void (^)(NSError *))arg4;
- (void)_XCT_unregisterForAccessibilityNotification:(int)arg1 withRegistrationToken:(NSNumber *)arg2 reply:(void (^)(NSError *))arg3;
- (void)_XCT_registerForAccessibilityNotification:(int)arg1 reply:(void (^)(NSNumber *, NSError *))arg2;
- (void)_XCT_launchApplicationWithBundleID:(NSString *)arg1 arguments:(NSArray *)arg2 environment:(NSDictionary *)arg3 completion:(void (^)(NSError *))arg4;
- (void)_XCT_startMonitoringApplicationWithBundleID:(NSString *)arg1;
- (void)_XCT_requestBackgroundAssertionForPID:(int)arg1 reply:(void (^)(BOOL))arg2;
- (void)_XCT_requestBackgroundAssertionWithReply:(void (^)(void))arg1;
- (void)_XCT_registerTarget;
- (void)_XCT_requestEndpointForTestTargetWithPID:(int)arg1 preferredBackendPath:(NSString *)arg2 reply:(void (^)(NSXPCListenerEndpoint *, NSError *))arg3;
- (void)_XCT_requestSocketForSessionIdentifier:(NSUUID *)arg1 reply:(void (^)(NSFileHandle *))arg2;
- (void)_XCT_exchangeProtocolVersion:(unsigned long long)arg1 reply:(void (^)(unsigned long long))arg2;
- (void)_XCT_requestScreenshot:(/*XCTScreenshotRequest * */id)arg1 withReply:(void (^)(/*XCTImage * */id, NSError *))arg2;

// Available since Xcode9
- (void)_XCT_requestScreenshotOfScreenWithID:(unsigned int)arg1 withRect:(struct CGRect)arg2 uti:(NSString *)arg3 compressionQuality:(double)arg4 withReply:(void (^)(NSData *, NSError *))arg5;
- (void)_XCT_requestScreenshotOfScreenWithID:(unsigned int)arg1 withRect:(struct CGRect)arg2 withReply:(void (^)(NSData *, NSError *))arg3;
@end
