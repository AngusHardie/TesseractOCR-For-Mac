//
//  RecognitionManager.m
//  CocoaApp
//
//  Created by Angus W Hardie on 07/07/2007.
//  Copyright 2007 MalcolmHardie Solutions Ltd. All rights reserved.
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



#import "RecognitionManager.h"
#import <Tesseract/baseapi.h>
#import "util.h"
#import <Tesseract/memry.h>
#import <Tesseract/imgs.h>


using namespace tesseract;

@interface RecognitionManager (privateMethods)

- (void)processText;

@end

@implementation RecognitionManager

- (id)init
{
	
	
	self = [super init];
	
	
	if (self) {
		
		recognitionLock = [[NSRecursiveLock alloc] init];
		
		
		NSString* dataPathDirectory = [[[NSBundle mainBundle] bundlePath] stringByAppendingString:@"/Contents/Resources/"];
		
		//NSLog(@"%@",dataPathDirectory);
		
		
		
		const char* dataPathDirectoryCString = [dataPathDirectory cStringUsingEncoding:NSUTF8StringEncoding];
		
		setenv("TESSDATA_PREFIX", dataPathDirectoryCString, 1);
		
		// can the language be switched after init?
		// find out and refactor this if necessary
		
		tess = new TessBaseAPI();
		
		
//		tess->InitWithLanguage("CocoaApp" , NULL, NULL,
//									  NULL, false, 0, NULL);
		
		tess->Init(dataPathDirectoryCString, NULL, NULL, 0, false);
		
		//tess->SetPageSegMode(PSM_SINGLE_COLUMN);
		
		tess->SetAccuracyVSpeed(AVS_MOST_ACCURATE);
		
	}
	
	return self;
	
}

- (void)dealloc
{
	
	[recognitionLock release];
	
	tess->End();
	[super dealloc];
}

- (IBAction)imageChanged:(id)sender
{

	
	[self recognize];
}




/**
**	awakeFromNib - we want the window to recognize the test image on startup
**/
- (void)awakeFromNib
{
	
	
	[self recognize];

	
}


- (void)recognizeInThread:(id)object
{
	
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	[recognitionLock lock];
	
	id uiImage = [imageView image];
	id resultText = [self recognizeImage:uiImage];
	

	
	[self performSelectorOnMainThread:@selector(recognizeComplete:) withObject:resultText waitUntilDone:NO];

	
	[recognitionLock unlock];
	
	[pool release];
	[NSThread exit];
	
}


- (void)recognizeComplete:(id)resultText
{
	
	[progressView stopAnimation:self];
	
	[imageView setEnabled:YES];
	
	if (resultText) {
		[textView setString:resultText];
	}
}

/**
**	recognize the current image and display text
**/
- (void)recognize
{
	[progressView startAnimation:self];
	[imageView setEnabled:NO];
	[NSThread detachNewThreadSelector: @selector(recognizeInThread:) toTarget: self withObject:nil];
	
	
}



/**
**	processes the text from the image into the text field using tesseract ocr
**  note that tesseract setup occurs in class initalization
**/
- (NSString*)recognizeImage:(NSImage*)uiImage
{
	
	char* text;
	
	
	NSLog(@"starting processing");
	
	
	
	Class bitmapImageRepClass = [NSBitmapImageRep class];
	NSBitmapImageRep* imageRep;
	id bestRep = nil;
	id enumerator = [[uiImage representations] objectEnumerator];
	while ((imageRep = [enumerator nextObject]) != nil) {
		if ([imageRep isKindOfClass: bitmapImageRepClass]) {
			bestRep = imageRep;
			//NSLog(@"%@",imageRep);
		}
		
	}
	
	if (!bestRep) {
		//NSLog(@"no representation valid");
		
		// no representation found, try getting a tiffRepresentation as the last
		// chance. 
		
		bestRep = [NSBitmapImageRep imageRepWithData:[uiImage TIFFRepresentation]];
		
		//return;	
	}
	
	if (bestRep == nil) {
		// everything failed and we have no representation
		
		
		id alertMessage = [[NSAlert alloc] init];
		
		[alertMessage setMessageText:@"Unable to use image"];
		
		[alertMessage setInformativeText:@"This image has no valid bitmap representation"];
		
		
		
		[alertMessage addButtonWithTitle:@"OK"];
		
		[alertMessage setAlertStyle:NSWarningAlertStyle];
		[alertMessage runModal];
		
		return nil;
	}
	
	
	
	
	
	
	
	
	imageRep = bestRep;
	
	//NSLog(@"%@",[imageRep description]); 
	
	NSSize imageSize = NSMakeSize([imageRep pixelsWide],[imageRep pixelsHigh]);
	int bytes_per_line = [imageRep bytesPerRow];
	
	
	
	unsigned char* imageData = [imageRep bitmapData];
	
	text = tess->TesseractRect((const unsigned char*)imageData, [imageRep bitsPerPixel]/8,
									  bytes_per_line, 0, 0,
									  imageSize.width, imageSize.height);
	
	
	id displayString = [NSString stringWithCString:text encoding:NSUTF8StringEncoding];
	
	
	
	delete(text);
	
	
	
	return displayString;
	
	
}

/*- (void)testProcess
{
	IMAGE image;
	char* text;
	
	id imagePath = [[NSBundle mainBundle] pathForResource:@"phototest" ofType:@"tif"];
	
	const char* imagePathStr = [imagePath cStringUsingEncoding:NSUTF8StringEncoding];
	
	
	if (image.read_header(imagePathStr) < 0) {
		NSLog(@"failed to read image 1");
	}
	if (image.read(image.get_ysize ()) < 0) {
		NSLog(@"failed to read image 2");
	}
	
	int bytes_per_line = check_legal_image_size(image.get_xsize(),
												image.get_ysize(),
												image.get_bpp());
	
	text = TessBaseAPI::TesseractRect(image.get_buffer(), image.get_bpp()/8,
									  bytes_per_line, 0, 0,
									  image.get_xsize(), image.get_ysize());
	
	
	id displayString = [NSString stringWithCString:text encoding:NSUTF8StringEncoding];
	
	
	
	delete(text);
	
	
	
	if (displayString) {
		[textView setString:displayString];
	}
}*/


- (IBAction)saveDocument:(id)sender
{
	
	
	id panel = [NSSavePanel savePanel];
	
	
	[panel beginSheetForDirectory:NSHomeDirectory() file:@"output.txt" modalForWindow:recognitionWindow modalDelegate:self didEndSelector:@selector(saveRecognizedTextPanelDidEnd:returnCode:contextInfo:) contextInfo:nil];
	
}

- (void)saveRecognizedTextPanelDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode  contextInfo:(void  *)contextInfo
{
	
	if (NSOKButton == returnCode) {
		
		[[textView string] writeToURL:[sheet URL] atomically:YES];
		
	}
	
}
@end



