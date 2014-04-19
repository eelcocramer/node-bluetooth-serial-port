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

#ifndef NODE_BTSP_SRC_BLUETOOTH_WORKER_H
#define NODE_BTSP_SRC_BLUETOOTH_WORKER_H

#import <Foundation/NSObject.h>
#import <IOBluetooth/objc/IOBluetoothRFCOMMChannel.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothDeviceInquiry.h>
#import "pipe.h"
#include <v8.h>
#include <node.h>

struct device_info_t {
	char address[19];
	char name[248];
};

@interface BluetoothWorker: NSObject {
    @private
	NSMutableDictionary *devices;
    NSThread *worker;
    pipe_producer_t *inquiryProducer;
	NSLock *sdpLock;
	NSLock *connectLock;
	IOReturn connectResult;
	int lastChannelID;
	NSLock *writeLock;
	IOReturn writeResult;
	NSTimer *keepAliveTimer;
}

+ (id)getInstance;
- (void) disconnectFromDevice: (NSString *) address;
- (IOReturn)connectDevice: (NSString *) address onChannel: (int) channel withPipe: (pipe_t *)pipe;
- (IOReturn)writeAsync:(void *)data length:(UInt16)length toDevice: (NSString *)address;
- (void) inquireWithPipe: (pipe_t *)pipe;
- (int) getRFCOMMChannelID: (NSString *) address;

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)rfcommChannel data:(void *)dataPointer length:(size_t)dataLength;
- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel*)rfcommChannel;

- (void) deviceInquiryComplete: (IOBluetoothDeviceInquiry*) sender 
    error: (IOReturn) error
    aborted: (BOOL) aborted;
- (void) deviceInquiryDeviceFound: (IOBluetoothDeviceInquiry*) sender
	device: (IOBluetoothDevice*) device;

@end

#endif