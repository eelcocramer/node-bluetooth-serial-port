#import "BluetoothWorker.h"
#import "BluetoothDeviceResources.h"
#import <Foundation/NSObject.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothRFCOMMChannel.h>
#import <IOBluetooth/objc/IOBluetoothSDPUUID.h>
#import <IOBluetooth/objc/IOBluetoothSDPServiceRecord.h>

#include <v8.h>
#include <node.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <node_object_wrap.h>
#include "pipe.h"

#ifndef RFCOMM_UUID
#define RFCOMM_UUID 0x0003
#endif

using namespace node;
using namespace v8;

/** Private class for wrapping a pipe */
@interface Pipe : NSObject {
	pipe_t *pipe;
}
@property (nonatomic, assign) pipe_t *pipe;
@end

/** Implementation of the pipe class */
@implementation Pipe
@synthesize pipe;
@end

/** Private class for wrapping data */
@interface BTData : NSObject {
	NSData *data;
	NSString *address;
}
@property (nonatomic, assign) NSData *data;
@property (nonatomic, assign) NSString *address;
@end

/** Implementation of bt data class */
@implementation BTData
@synthesize data;
@synthesize address;
@end

/** Class that is handling all the Bluetooth work */
@implementation BluetoothWorker

/** The BluetoothWorker class is a singleton. An instance can be obtained using this method */
+ (id)getInstance
{
    static BluetoothWorker *instance = nil;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        instance = [[self alloc] init];
    });

    return instance;
}

/** Initializes a BluetoothWorker object */
- (id) init
{
  	self = [super init];
  	sdpLock = [[NSLock alloc] init];
	devices = [[NSMutableDictionary alloc] init];
  	connectLock = [[NSLock alloc] init];
  	writeLock = [[NSLock alloc] init];

  	// creates a worker thread that handles all the asynchronous stuff
  	worker = [[NSThread alloc]initWithTarget: self selector: @selector(startBluetoothThread:) object: nil];
	[worker start];
	return self;
}

/** Creates a run loop and sets a timer to keep the run loop alive */
- (void) startBluetoothThread: (id) arg
{
	NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
      //schedule a timer so runMode won't stop immediately
    keepAliveTimer = [[NSTimer alloc] initWithFireDate:[NSDate distantFuture] 
        interval:1 target:nil selector:nil userInfo:nil repeats:YES];
    [runLoop addTimer:keepAliveTimer forMode:NSDefaultRunLoopMode];
 	[[NSRunLoop currentRunLoop] run];
}

/** Disconnect from a Bluetooth device */
- (void) disconnectFromDevice:(NSString *)address
{
	// this function is called synchronous from javascript so it waits on the worker task to complete.
	[self performSelector:@selector(disconnectFromDeviceTask:) onThread:worker withObject: address waitUntilDone:true];
}

/** Task on the worker to disconnect from a Bluetooth device */
- (void) disconnectFromDeviceTask: (NSString *) address
{
	// make it safe
	@synchronized(self) {
		BluetoothDeviceResources *res = [devices objectForKey: address];

		if (res != nil) {
			if (res.producer != NULL) {
				pipe_producer_free(res.producer);
				res.producer = NULL;
			}

			if (res.channel != NULL) {
				[res.channel closeChannel];
				res.channel = NULL;
			}

			if (res.device != NULL) {
				[res.device closeConnection];
				res.device = NULL;
			}

			[devices removeObjectForKey: address];
		}
	}
}

/** Connect to a Bluetooth device on a specific channel using a pipe to communicate with the main thread */
- (IOReturn)connectDevice: (NSString *) address onChannel: (int) channel withPipe: (pipe_t *)pipe
{
	[connectLock lock];
	
	Pipe *pipeObj = [[Pipe alloc] init];
	pipeObj.pipe = pipe;

	NSDictionary *parameters = [[NSDictionary alloc] initWithObjectsAndKeys:
		address, @"address", [NSNumber numberWithInt: channel], @"channel", pipeObj, @"pipe", nil];

	// connect to a device and wait for the result 
	[self performSelector:@selector(connectDeviceTask:) onThread:worker withObject:parameters waitUntilDone:true];
	IOReturn result = connectResult;
	[connectLock unlock];

	return result;
}

/** Task to connect to a specific device */
- (void)connectDeviceTask: (NSDictionary *)parameters
{
	NSString *address = [parameters objectForKey:@"address"];
	NSNumber *channelID = [parameters objectForKey:@"channel"];
	pipe_t *pipe = ((Pipe *)[parameters objectForKey:@"pipe"]).pipe;

	@synchronized(self) {
		connectResult = kIOReturnError;

		if ([devices objectForKey: address] == nil) {
			IOBluetoothDevice *device = [IOBluetoothDevice deviceWithAddressString:address];

			if (device != nil) {
				IOBluetoothRFCOMMChannel *channel = [[IOBluetoothRFCOMMChannel alloc] init];
				if ([device openRFCOMMChannelSync: &channel withChannelID:[channelID intValue] delegate: self] == kIOReturnSuccess) {
					connectResult = kIOReturnSuccess;
				   	pipe_producer_t *producer = pipe_producer_new(pipe);
				   	BluetoothDeviceResources *res = [[BluetoothDeviceResources alloc] init];
				   	res.device = device;
				   	res.producer = producer;
				   	res.channel = channel;

				   	[devices setObject:res forKey:address];
				}
			}
		}
	}
}

/** Write synchronized to a connected Bluetooth device */
- (IOReturn)writeSync:(void *)data length:(UInt16)length toDevice: (NSString *)address
{
	[writeLock lock];
	
	BTData *writeData = [[BTData alloc] init];
	writeData.data = [NSData dataWithBytes: data length: length];
	writeData.address = address;

	// wait for the write to be performed on the worker thread
	[self performSelector:@selector(writeSyncTask:) onThread:worker withObject:writeData waitUntilDone:true];
	
	IOReturn result = writeResult;
	[writeLock unlock];

	return result;
}

/** Task to do the writing */
- (void)writeSyncTask:(BTData *)writeData
{
	@synchronized(self) {
		BluetoothDeviceResources *res = [devices objectForKey:writeData.address];

		if (res != nil) {
			writeResult = [res.channel writeSync: (void *)[writeData.data bytes] length: [writeData.data length]];
		}
	}
}

/** Inquire Bluetooth devices and send results through the given pipe */
- (void) inquireWithPipe: (pipe_t *)pipe
{
	@synchronized(self) {
	   	inquiryProducer = pipe_producer_new(pipe);
		[self performSelector:@selector(inquiryTask) onThread:worker withObject:nil waitUntilDone:false];
	}
}

/** Worker task to the the inquiry */
- (void) inquiryTask
{
    IOBluetoothDeviceInquiry *bdi = [[IOBluetoothDeviceInquiry alloc] init];
	[bdi setDelegate: self];
	[bdi start];
}

/** Get the RFCOMM channel for a given device */
- (int) getRFCOMMChannelID: (NSString *) address 
{
	[sdpLock lock];
	// call the task on the worker thread and wait for the result
	[self performSelector:@selector(getRFCOMMChannelIDTask:) onThread:worker withObject:address waitUntilDone:true];
	int returnValue = lastChannelID;
	[sdpLock unlock];
	return returnValue;
}

/** Task to get the RFCOMM channel */
- (void) getRFCOMMChannelIDTask: (NSString *) address
{
    IOBluetoothDevice *device = [IOBluetoothDevice deviceWithAddressString:address];
    IOBluetoothSDPUUID *uuid = [[IOBluetoothSDPUUID alloc] initWithUUID16:RFCOMM_UUID];
    NSArray *uuids = [NSArray arrayWithObject:uuid];

    // always perform a new SDP query
    NSDate *lastServicesUpdate = [device getLastServicesUpdate];
    NSDate *currentServiceUpdate = NULL;

    // only search for the UUIDs we are going to need...
    [device performSDPQuery: NULL uuids: uuids];

    bool stop = false;

	NSTimeInterval endTime = [[NSDate date] timeIntervalSince1970] + 60;
    // if needed wait for a while for the sdp update
    while (!stop && [[NSDate date] timeIntervalSince1970] < endTime) { // wait no more than 60 seconds for SDP update
        currentServiceUpdate = [device getLastServicesUpdate];

        if (currentServiceUpdate != NULL && [currentServiceUpdate laterDate: lastServicesUpdate]) {
            stop = true;
        } else {
            sleep(1);
        }
    }

    NSArray *services = [device services];
    
    // if there are services check if it is the one we are looking for.
    if (services != NULL) {
        for (NSUInteger i=0; i<[services count]; i++) {
            IOBluetoothSDPServiceRecord *sr = [services objectAtIndex: i];
            
            if ([sr hasServiceFromArray: uuids]) {
                BluetoothRFCOMMChannelID cid = -1;
                if ([sr getRFCOMMChannelID: &cid] == kIOReturnSuccess) {
                	lastChannelID = cid;
                	return;
                }
            }
        }
    }

    // This can happen is some conditions where the network is unreliable. Just ignore for now...
    lastChannelID = -1;
}

/** Called when data is received from a remote device */
- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)rfcommChannel data:(void *)dataPointer length:(size_t)dataLength
{
	@synchronized(self) {
		NSString *address = [[rfcommChannel getDevice] getAddressString];
		NSData *data = [NSData dataWithBytes: dataPointer length: dataLength];
		BluetoothDeviceResources *res = [devices objectForKey: address];

		if (res != NULL && res.producer != NULL) {
			// push the data into the pipe so it can be read from the main thread 
			pipe_push(res.producer, [data bytes], data.length);
		}
	}
}

/** Called when a channel has been closed */
- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel*)rfcommChannel
{
	[self disconnectFromDevice: [[rfcommChannel getDevice] getAddressString]];
}

/** Called when the device inquiry completes */
- (void) deviceInquiryComplete: (IOBluetoothDeviceInquiry *) sender error: (IOReturn) error aborted: (BOOL) aborted
{
	@synchronized(self) {
		if (inquiryProducer != NULL) {
			// free the producer so the main thread is signaled that the inquiry has been completed.
			pipe_producer_free(inquiryProducer);
			inquiryProducer = NULL;
		}
	}
}

/** Called when a device has been found */
- (void) deviceInquiryDeviceFound: (IOBluetoothDeviceInquiry*) sender device: (IOBluetoothDevice*) device 
{
	@synchronized(self) {
		if (inquiryProducer != NULL) {
			device_info_t *info = new device_info_t;
			strcpy(info->address, [[device getAddressString] UTF8String]);
			strcpy(info->name, [[device getNameOrAddress] UTF8String]);

			// push the device data into the pipe to notify the main thread
			pipe_push(inquiryProducer, info, 1);
			
			delete info;
		}
	}
}

@end
