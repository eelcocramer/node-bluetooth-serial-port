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

@interface Pipe : NSObject {
	pipe_t *pipe;
}
@property (nonatomic, assign) pipe_t *pipe;
@end

@implementation Pipe
@synthesize pipe;
@end

@interface BTData : NSObject {
	NSData *data;
	NSString *address;
}
@property (nonatomic, assign) NSData *data;
@property (nonatomic, assign) NSString *address;
@end

@implementation BTData
@synthesize data;
@synthesize address;
@end

@implementation BluetoothWorker

+ (id)getInstance
{
    static BluetoothWorker *instance = nil;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        instance = [[self alloc] init];
    });

    return instance;
}

- (id) init
{
  	self = [super init];
  	sdpLock = [[NSLock alloc] init];
	devices = [[NSMutableDictionary alloc] init];
  	connectLock = [[NSLock alloc] init];
  	writeLock = [[NSLock alloc] init];
  	worker = [[NSThread alloc]initWithTarget: self selector: @selector(startBluetoothThread:) object: nil];
	[worker start];
	return self;
}

// Create run loop
- (void) startBluetoothThread: (id) arg
{
	NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
      //schedule a timer so runMode won't stop immediately
    keepAliveTimer = [[NSTimer alloc] initWithFireDate:[NSDate distantFuture] 
        interval:1 target:nil selector:nil userInfo:nil repeats:YES];
    [runLoop addTimer:keepAliveTimer forMode:NSDefaultRunLoopMode];
 	[[NSRunLoop currentRunLoop] run];
}

- (void) disconnectFromDevice:(NSString *)address
{

	[self performSelector:@selector(disconnectFromDeviceTask:) onThread:worker withObject: address waitUntilDone:true];
}

- (void) disconnectFromDeviceTask: (NSString *) address
{
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

- (IOReturn)connectDevice: (NSString *) address onChannel: (int) channel withPipe: (pipe_t *)pipe
{
	[connectLock lock];
	
	Pipe *pipeObj = [[Pipe alloc] init];
	pipeObj.pipe = pipe;

	NSDictionary *parameters = [[NSDictionary alloc] initWithObjectsAndKeys:
		address, @"address", [NSNumber numberWithInt: channel], @"channel", pipeObj, @"pipe", nil];

	[self performSelector:@selector(connectDeviceTask:) onThread:worker withObject:parameters waitUntilDone:true];
	IOReturn result = connectResult;
	[connectLock unlock];

	return result;
}

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

- (IOReturn)writeSync:(void *)data length:(UInt16)length toDevice: (NSString *)address
{
	[writeLock lock];
	
	BTData *writeData = [[BTData alloc] init];
	writeData.data = [NSData dataWithBytes: data length: length];
	writeData.address = address;

	[self performSelector:@selector(writeSyncTask:) onThread:worker withObject:writeData waitUntilDone:true];
	
	IOReturn result = writeResult;
	[writeLock unlock];

	return result;
}

- (void)writeSyncTask:(BTData *)writeData
{
	@synchronized(self) {
		BluetoothDeviceResources *res = [devices objectForKey:writeData.address];

		if (res != nil) {
			writeResult = [res.channel writeSync: (void *)[writeData.data bytes] length: [writeData.data length]];
		}
	}
}

- (void) inquireWithPipe: (pipe_t *)pipe
{
	@synchronized(self) {
	   	inquiryProducer = pipe_producer_new(pipe);
		[self performSelector:@selector(inquiryTask) onThread:worker withObject:nil waitUntilDone:false];
	}
}

- (void) inquiryTask
{
    IOBluetoothDeviceInquiry *bdi = [[IOBluetoothDeviceInquiry alloc] init];
	[bdi setDelegate: self];
	[bdi start];
}

- (int) getRFCOMMChannelID: (NSString *) address 
{
	[sdpLock lock];
	[self performSelector:@selector(getRFCOMMChannelIDTask:) onThread:worker withObject:address waitUntilDone:true];
	int returnValue = lastChannelID;
	[sdpLock unlock];
	return returnValue;
}

- (void) getRFCOMMChannelIDTask: (NSString *) address
{
    IOBluetoothDevice *device = [IOBluetoothDevice deviceWithAddressString:address];
    IOBluetoothSDPUUID *uuid = [[IOBluetoothSDPUUID alloc] initWithUUID16:RFCOMM_UUID];
    NSArray *uuids = [NSArray arrayWithObject:uuid];

    // always perform a new SDP query
    NSDate *lastServicesUpdate = [device getLastServicesUpdate];
    NSDate *currentServiceUpdate = NULL;
    [device performSDPQuery: NULL uuids: uuids];
    int counter = 0;
    bool stop = false;

    while (!stop && counter < 60) { // wait no more than 60 seconds for SDP update
        currentServiceUpdate = [device getLastServicesUpdate];

        if (currentServiceUpdate != NULL && [currentServiceUpdate laterDate: lastServicesUpdate]) {
            stop = true;
        } else {
            sleep(1);
        }

        counter++;
    }

    NSArray *services = [device services];
    
    if (services == NULL) {
        if ([device getLastServicesUpdate] == NULL) {
            //TODO not sure if this will happen... But we should at least throw an exception here that is
            // logged correctly in Javascript.
            fprintf(stderr, "[device services] == NULL -> This was not expected. Please file a bug for node-bluetooth-serial-port on Github. Thanks.\n\r");
        }
    } else {
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

    lastChannelID = -1;
}

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)rfcommChannel data:(void *)dataPointer length:(size_t)dataLength
{
	@synchronized(self) {
		NSString *address = [[rfcommChannel getDevice] getAddressString];
		NSData *data = [NSData dataWithBytes: dataPointer length: dataLength];
		BluetoothDeviceResources *res = [devices objectForKey: address];

		if (res != NULL && res.producer != NULL) {
			pipe_push(res.producer, [data bytes], data.length);
		}
	}
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel*)rfcommChannel
{
	[self disconnectFromDevice: [[rfcommChannel getDevice] getAddressString]];
}

- (void) deviceInquiryComplete: (IOBluetoothDeviceInquiry *) sender error: (IOReturn) error aborted: (BOOL) aborted
{
	@synchronized(self) {
		if (inquiryProducer != NULL) {
			pipe_producer_free(inquiryProducer);
			inquiryProducer = NULL;
		}
	}
}

- (void) deviceInquiryDeviceFound: (IOBluetoothDeviceInquiry*) sender device: (IOBluetoothDevice*) device 
{
	@synchronized(self) {
		if (inquiryProducer != NULL) {
			device_info_t *info = new device_info_t;
			strcpy(info->address, [[device getAddressString] UTF8String]);
			strcpy(info->name, [[device getNameOrAddress] UTF8String]);
			pipe_push(inquiryProducer, info, 1);
			delete info;
		}
	}
}

@end
