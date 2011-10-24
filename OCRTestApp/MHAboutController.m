//
//  MHAboutManager.m
//  CocoaOCR
//
//  Created by Angus Hardie on 05/01/2010.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//
//Licensed under the Apache License, Version 2.0 (the "License"); 
//you may not use this file except in compliance with the License. 
//You may obtain a copy of the License at 
//
//http://www.apache.org/licenses/LICENSE-2.0 
//
//Unless required by applicable law or agreed to in writing, software 
//distributed under the License is distributed on an "AS IS" BASIS, 
//WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
//See the License for the specific language governing permissions and limitations under the License. 


#import "MHAboutController.h"


@implementation MHAboutController


- (NSAttributedString*)creditsString
{
	
	
	id creditsFilePath = [[NSBundle mainBundle] pathForResource:@"credits" ofType:@"html"];
	
	
	return [[[NSAttributedString alloc] initWithPath:creditsFilePath documentAttributes:nil] autorelease];
}

- (NSString*)versionNumberString
{
	
	return [[[NSBundle mainBundle] infoDictionary] valueForKey:@"CFBundleVersion"];
}

- (NSString*)appNameString
{
	
	return [[[NSBundle mainBundle] infoDictionary] valueForKey:@"CFBundleName"];
	
}
@end
