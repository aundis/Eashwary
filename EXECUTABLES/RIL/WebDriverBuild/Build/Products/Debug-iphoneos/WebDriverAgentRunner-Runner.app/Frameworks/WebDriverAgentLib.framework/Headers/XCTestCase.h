//
//     Generated by class-dump 3.5 (64 bit).
//
//     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2013 by Steve Nygard.
//

#import <XCTest/XCTest.h>

#import <WebDriverAgentLib/CDStructures.h>

@class NSInvocation, XCTestCaseRun, XCTestContext, _XCTestCaseImplementation;

@interface XCTestCase()
{
    id _internalImplementation;
}
@property(retain) _XCTestCaseImplementation *internalImplementation; // @synthesize internalImplementation=_internalImplementation;
@property(readonly) XCTestContext *testContext;
@property(readonly) unsigned long long activityRecordStackDepth;
@property(nonatomic) BOOL shouldHaltWhenReceivesControl;
@property(retain) XCTestCaseRun *testCaseRun;

+ (id)_baselineDictionary;
+ (BOOL)_treatMissingBaselinesAsTestFailures;
+ (id)knownMemoryMetrics;
+ (id)measurementFormatter;
+ (BOOL)_reportPerformanceFailuresForLargeImprovements;
+ (BOOL)_enableSymbolication;

+ (BOOL)isInheritingTestCases;
+ (id)_testStartActvityDateFormatter;
+ (id)testCaseWithSelector:(SEL)arg1;


+ (void)tearDown;
+ (void)setUp;
+ (id)defaultTestSuite;
+ (id)allTestMethodInvocations;
+ (void)_allTestMethodInvocations:(id)arg1;
+ (id)testMethodInvocations;
+ (id)allSubclasses;
- (void)startActivityWithTitle:(id)arg1 block:(CDUnknownBlockType)arg2;
- (void)registerDefaultMetrics;
- (id)baselinesDictionaryForTest;
- (void)_logAndReportPerformanceMetrics:(id)arg1 perfMetricResultsForIDs:(id)arg2 withBaselinesForTest:(id)arg3;
- (void)_logAndReportPerformanceMetrics:(id)arg1 perfMetricResultsForIDs:(id)arg2 withBaselinesForTest:(id)arg3 defaultBaselinesForPerfMetricID:(id)arg4;
- (void)registerMetricID:(id)arg1 name:(id)arg2 unitString:(id)arg3;
- (void)registerMetricID:(id)arg1 name:(id)arg2 unit:(id)arg3;
- (void)reportMetric:(id)arg1 reportFailures:(BOOL)arg2;
- (void)reportMeasurements:(id)arg1 forMetricID:(id)arg2 reportFailures:(BOOL)arg3;
- (void)_recordValues:(id)arg1 forPerformanceMetricID:(id)arg2 name:(id)arg3 unitsOfMeasurement:(id)arg4 baselineName:(id)arg5 baselineAverage:(id)arg6 maxPercentRegression:(id)arg7 maxPercentRelativeStandardDeviation:(id)arg8 maxRegression:(id)arg9 maxStandardDeviation:(id)arg10 file:(id)arg11 line:(unsigned long long)arg12;
- (id)_symbolicationRecordForTestCodeInAddressStack:(id)arg1;
- (void)measureBlock:(CDUnknownBlockType)arg1;
- (void)stopMeasuring;
- (void)startMeasuring;
- (BOOL)_isMeasuringMetrics;
- (BOOL)_didStopMeasuring;
- (BOOL)_didStartMeasuring;
- (BOOL)_didMeasureMetrics;
- (id)_perfMetricsForID;
- (void)_logMemoryGraphDataFromFilePath:(id)arg1 withTitle:(id)arg2;
- (void)_logMemoryGraphData:(id)arg1 withTitle:(id)arg2;
- (unsigned long long)numberOfTestIterationsForTestWithSelector:(SEL)arg1;
- (void)afterTestIteration:(unsigned long long)arg1 selector:(SEL)arg2;
- (void)beforeTestIteration:(unsigned long long)arg1 selector:(SEL)arg2;
- (void)tearDownTestWithSelector:(SEL)arg1;
- (void)setUpTestWithSelector:(SEL)arg1;
- (void)performTest:(id)arg1;
- (void)invokeTest;
- (Class)testRunClass;
- (Class)_requiredTestRunBaseClass;
- (void)_recordUnexpectedFailureWithDescription:(id)arg1 error:(id)arg2;
- (void)_recordUnexpectedFailureWithDescription:(id)arg1 exception:(id)arg2;
- (void)_enqueueFailureWithDescription:(NSString *)description inFile:(NSString *)filePath atLine:(NSUInteger)lineNumber expected:(BOOL)expected;
- (void)_dequeueFailures;
- (void)_interruptTest;
- (BOOL)isEqual:(id)arg1;
- (id)nameForLegacyLogging;
- (id)name;
- (id)languageAgnosticTestMethodName;
- (unsigned long long)testCaseCount;
- (id)initWithSelector:(SEL)arg1;
- (id)init;
- (void)waiter:(id)arg1 didFulfillInvertedExpectation:(id)arg2;
- (void)waiter:(id)arg1 fulfillmentDidViolateOrderingConstraintsForExpectation:(id)arg2 requiredExpectation:(id)arg3;
- (void)waiter:(id)arg1 didTimeoutWithUnfulfilledExpectations:(id)arg2;
- (id)expectationForPredicate:(id)arg1 evaluatedWithObject:(id)arg2 handler:(CDUnknownBlockType)arg3;
- (id)expectationForNotification:(id)arg1 object:(id)arg2 handler:(CDUnknownBlockType)arg3;
- (id)keyValueObservingExpectationForObject:(id)arg1 keyPath:(id)arg2 handler:(CDUnknownBlockType)arg3;
- (id)keyValueObservingExpectationForObject:(id)arg1 keyPath:(id)arg2 expectedValue:(id)arg3;
- (void)_addExpectation:(id)arg1;
- (void)waitForExpectations:(id)arg1 timeout:(double)arg2 enforceOrder:(BOOL)arg3;
- (void)waitForExpectations:(id)arg1 timeout:(double)arg2;
- (void)waitForExpectationsWithTimeout:(double)arg1 handler:(CDUnknownBlockType)arg2;
- (void)_waitForExpectations:(id)arg1 timeout:(double)arg2 enforceOrder:(BOOL)arg3 handler:(CDUnknownBlockType)arg4;
- (id)expectationWithDescription:(id)arg1;
- (id)_expectationForDarwinNotification:(id)arg1;
- (void)nestedWaiter:(id)arg1 wasInterruptedByTimedOutWaiter:(id)arg2;

@end