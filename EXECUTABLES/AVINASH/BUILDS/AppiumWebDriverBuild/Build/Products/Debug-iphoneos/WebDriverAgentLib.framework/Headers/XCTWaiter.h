//
//     Generated by class-dump 3.5 (64 bit).
//
//     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2013 by Steve Nygard.
//

#import "NSObject.h"

#import "XCTWaiterManagement.h"
#import "XCTestExpectationDelegate.h"

@class NSArray, NSObject<OS_dispatch_queue>, NSString, _XCTWaiterImpl;

@interface XCTWaiter : NSObject <XCTestExpectationDelegate, XCTWaiterManagement>
{
    id _internalImplementation;
}
@property(readonly) _XCTWaiterImpl *internalImplementation; // @synthesize internalImplementation=_internalImplementation;
@property(readonly) double timeout;
@property(readonly, getter=isInProgress) BOOL inProgress;
@property struct __CFRunLoop *waitingRunLoop;
@property(readonly, nonatomic) NSObject<OS_dispatch_queue> *delegateQueue;
@property(readonly, nonatomic) NSObject<OS_dispatch_queue> *queue;
@property(readonly, copy) NSArray *waitCallStackReturnAddresses;
@property(readonly) NSArray *fulfilledExpectations;
@property __weak id <XCTWaiterDelegate> delegate;

+ (id)waitForActivity:(id)arg1 timeout:(double)arg2 block:(CDUnknownBlockType)arg3;
+ (long long)waitForExpectations:(id)arg1 timeout:(double)arg2 enforceOrder:(BOOL)arg3;
+ (long long)waitForExpectations:(id)arg1 timeout:(double)arg2;
+ (void)wait:(double)arg1;
+ (void)setStallHandler:(CDUnknownBlockType)arg1;
+ (void)handleStalledWaiter:(id)arg1;
+ (CDUnknownBlockType)installWatchdogForWaiter:(id)arg1 timeout:(double)arg2;

- (long long)result;
- (void)setState:(long long)arg1;
- (long long)state;
- (void)setWaitCallStackReturnAddresses:(id)arg1;
- (void)_queue_validateExpectationFulfillmentWithTimeoutState:(BOOL)arg1;
- (BOOL)_queue_enforceOrderingWithFulfilledExpectations:(id)arg1;
- (void)_queue_computeInitiallyFulfilledExpectations;
- (void)_queue_setExpectations:(id)arg1;
- (void)_validateExpectationFulfillmentWithTimeoutState:(BOOL)arg1;
- (void)didFulfillExpectation:(id)arg1;
- (void)cancelPrimitiveWait;
- (void)cancelWaiting;
- (void)primitiveWait:(double)arg1;
- (void)interruptForWaiter:(id)arg1;
- (long long)waitForExpectations:(id)arg1 timeout:(double)arg2 enforceOrder:(BOOL)arg3;
- (long long)waitForExpectations:(id)arg1 timeout:(double)arg2;
- (id)initWithDelegate:(id)arg1;
- (id)init;

@end
