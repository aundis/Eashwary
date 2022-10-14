//
//  ReSetProxy.m
//  ReSetProxy
//
//  Created by Bikee Bihari on 27/06/22.
//

#import <XCTest/XCTest.h>

@interface ReSetProxy : XCTestCase

@end

@implementation ReSetProxy

- (void)testResetProxy {
	
	XCUIApplication *app = [ [XCUIApplication alloc] initWithBundleIdentifier:@"com.apple.Preferences"];
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
			[NSThread sleepForTimeInterval:2.0];
			[app.staticTexts[@"Off"] tap];
			[NSThread sleepForTimeInterval:2.0];
			[app.buttons[@"Save"] tap];
		}else if(app.buttons[@"Off"].enabled)
		{
			[app.buttons[@"Off"] tap];
			[NSThread sleepForTimeInterval:0.5];
			[app.buttons[@"Wi-Fi"] tap];
		}

	}
	[NSThread sleepForTimeInterval:1];
	[app terminate];
}


@end
