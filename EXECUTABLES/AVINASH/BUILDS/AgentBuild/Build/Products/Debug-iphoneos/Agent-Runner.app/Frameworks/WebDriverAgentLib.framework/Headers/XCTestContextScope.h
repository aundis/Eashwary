//
//     Generated by class-dump 3.5 (64 bit).
//
//     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2013 by Steve Nygard.
//

@class NSMutableArray;

@interface XCTestContextScope : NSObject
{
    XCTestContextScope *_parentScope;
    NSMutableArray *_handlers;
}
@property(copy) NSMutableArray *handlers; // @synthesize handlers=_handlers;
@property(readonly) XCTestContextScope *parentScope; // @synthesize parentScope=_parentScope;

- (id)initWithParentScope:(id)arg1;

@end
