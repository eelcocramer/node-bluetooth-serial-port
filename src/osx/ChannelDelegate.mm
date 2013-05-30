/*
 * Copyright (c) 2012-2013, Eelco Cramer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "ChannelDelegate.h"
#import <Foundation/NSObject.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothRFCOMMChannel.h>
#import "BTSerialPortBinding.h"
#include <v8.h>
#include <node.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <node_object_wrap.h>
#include "pipe.h"

using namespace node;
using namespace v8;

@implementation ChannelDelegate

- (id) initWithPipe:(pipe_t *)pipe device: (IOBluetoothDevice *) device
{
  	self = [super init];

  	if (self) {
  		@synchronized(self) {
	    	m_producer = pipe_producer_new(pipe);
  		}
  	}

  	m_worker = [[NSThread alloc]initWithTarget: self selector: @selector(startBluetoothThread:) object: nil];
	[m_worker start];
	fprintf(stderr, "[m_worker start] called\n\r");
  	m_device = device;
  
	return self;
}

// Create run loop
- (void) startBluetoothThread: (id) arg
{
	[[NSRunLoop currentRunLoop] run];
}

- (IOReturn) connectOnChannel: (int) channel 
{
	fprintf(stderr, "worker is executing...\n");

	[self performSelector: @selector(connectOnChannelTask:) onThread: m_worker withObject: [NSNumber numberWithInt: channel] waitUntilDone:true];

	fprintf(stderr, "connect has been called...\n");

	if (m_channel != NULL) {
		return kIOReturnSuccess;
	} else {
		return kIOReturnError;
	}
}

- (void) connectOnChannelTask: (NSNumber *)channelID
{
	fprintf(stderr, "connectOnChannelTask: %i\n\r", [channelID intValue]);
	IOBluetoothRFCOMMChannel *channel = [[IOBluetoothRFCOMMChannel alloc] init];
	if ([m_device openRFCOMMChannelSync: &channel withChannelID: [channelID intValue] delegate: self] == kIOReturnSuccess) {
		m_channel = channel;
	}
	fprintf(stderr, "ready\n\r");
}

- (IOReturn)writeSync:(void *)data length:(UInt16)length {
	return [m_channel writeSync: data length:length];
}

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)rfcommChannel data:(void *)dataPointer length:(size_t)dataLength
{
	@synchronized(self) {
		if (m_producer != NULL) {
			fprintf(stderr, "Received data!");
			pipe_push(m_producer, dataPointer, dataLength);
		}
	}
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel*)rfcommChannel
{
	[self close];
}

- (void) close {
	@synchronized(self) {
		if (m_producer != NULL) {
			pipe_producer_free(m_producer);
			m_producer = NULL;
		}

		if (m_device != NULL) {
			[m_device closeConnection];
			m_device = NULL;
		}
	}
}

- (void) dealloc {
	[self close];
	fprintf(stderr, "Dealloc called!!!!!!!!!!!!!!!!!!\n\r");
	[super dealloc];
}

@end
