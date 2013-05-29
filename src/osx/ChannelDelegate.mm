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

- (id) initWithPipe:(pipe_t *)pipe
{
  self = [super init];
  if (self) {
  	@synchronized(self) {
	    m_producer = pipe_producer_new(pipe);
  	}
  }

  return self;
}

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)rfcommChannel data:(void *)dataPointer length:(size_t)dataLength
{
	@synchronized(self) {
		fprintf(stderr, "Received data!");
		if (m_producer != NULL) {
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
	}
}

- (void) dealloc {
	[self close];
	fprintf(stderr, "Dealloc called!!!!!!!!!!!!!!!!!!\n\r");
	[super dealloc];
}

@end