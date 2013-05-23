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

#include <v8.h>
#include <node.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <node_object_wrap.h>
#include "DeviceINQ.h"

extern "C"{
    #include <stdio.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <stdlib.h>
    #include <signal.h>
    #include <termios.h>
    #include <sys/poll.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <assert.h>
}

#import <Foundation/NSObject.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothDeviceInquiry.h>

using namespace std;
using namespace node;
using namespace v8;

@interface Discoverer: NSObject {
    DeviceINQ::objc_baton_t *m_baton;
}

- (id) initWithBaton:(DeviceINQ::objc_baton_t *)baton;

- (void) deviceInquiryComplete: (IOBluetoothDeviceInquiry*) sender 
                            error: (IOReturn) error
                            aborted: (BOOL) aborted;
- (void) deviceInquiryDeviceFound: (IOBluetoothDeviceInquiry*) sender
                            device: (IOBluetoothDevice*) device;
@end

@implementation Discoverer

- (id) initWithBaton:(DeviceINQ::objc_baton_t *)baton
{
  self = [super init];
  if (self) {
    m_baton = baton;
  }
  return self;
}

-(void) deviceInquiryComplete: (IOBluetoothDeviceInquiry*) sender 
                            error: (IOReturn) error
                            aborted: (BOOL) aborted
{
    // finnished
    Local<Value> argv[1] = {
        String::New("finished")
    };

    MakeCallback(m_baton->args->This(), "emit", 1, argv);

    CFRunLoopStop( CFRunLoopGetCurrent() );
}

-(void) deviceInquiryDeviceFound: (IOBluetoothDeviceInquiry*) sender
                            device: (IOBluetoothDevice*) device
{
    Local<Value> argv[3] = {
      String::New("found"),
      String::New([[device getAddressString] cString]),
      String::New([[device getName] cString])
    };

    MakeCallback(m_baton->args->This(), "emit", 3, argv);

    //printf("discovered %s\n", [[device getAddressString] cString]);
}
@end

void DeviceINQ::EIO_SdpSearch(uv_work_t *req) {
    sdp_baton_t *baton = static_cast<sdp_baton_t *>(req->data);

    // default, no channel is found
    baton->channel = -1;

    // if( proto == RFCOMM_UUID ) {
    //     baton->channel = d->val.int8;
    //     return; // stop if channel is found
    // }
}

void DeviceINQ::EIO_AfterSdpSearch(uv_work_t *req) {
    sdp_baton_t *baton = static_cast<sdp_baton_t *>(req->data);

    TryCatch try_catch;
    
    Local<Value> argv[1];
    argv[0] = Integer::New(baton->channel);
    baton->cb->Call(Context::GetCurrent()->Global(), 1, argv);
    
    if (try_catch.HasCaught()) {
        FatalException(try_catch);
    }

    baton->inquire->Unref();
    baton->cb.Dispose();
    delete baton;
    baton = NULL;
}

void DeviceINQ::Init(Handle<Object> target) {
    HandleScope scope;
    
    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(String::NewSymbol("DeviceINQ"));
    
    NODE_SET_PROTOTYPE_METHOD(t, "inquire", Inquire);
    NODE_SET_PROTOTYPE_METHOD(t, "findSerialPortChannel", SdpSearch);
    target->Set(String::NewSymbol("DeviceINQ"), t->GetFunction());
    target->Set(String::NewSymbol("DeviceINQ"), t->GetFunction());
}
    
DeviceINQ::DeviceINQ() {
        
}
    
DeviceINQ::~DeviceINQ() {
        
}
    
Handle<Value> DeviceINQ::New(const Arguments& args) {
    HandleScope scope;

    const char *usage = "usage: DeviceINQ()";
    if (args.Length() != 0) {
        return ThrowException(Exception::Error(String::New(usage)));
    }

    DeviceINQ* inquire = new DeviceINQ();
    inquire->Wrap(args.This());

    return args.This();
}
 
Handle<Value> DeviceINQ::Inquire(const Arguments& args) {
    HandleScope scope;

    const char *usage = "usage: inquire()";
    if (args.Length() != 0) {
        return ThrowException(Exception::Error(String::New(usage)));
    }

    objc_baton_t *baton = new objc_baton_t();
    baton->args = &args;

    Discoverer *d = [[Discoverer alloc] initWithBaton: baton];

    IOBluetoothDeviceInquiry *bdi = 
        [[IOBluetoothDeviceInquiry alloc] init];
    [bdi setDelegate: d];
    
    if ([bdi start] == kIOReturnSuccess) {
        CFRunLoopRun();
    }
    
    return Undefined();
}
    
Handle<Value> DeviceINQ::SdpSearch(const Arguments& args) {
    HandleScope scope;
    
    const char *usage = "usage: sdpSearchForRFCOMM(address, callback)";
    if (args.Length() != 2) {
        return ThrowException(Exception::Error(String::New(usage)));
    }
    
    const char *should_be_a_string = "address must be a string";
    if (!args[0]->IsString()) {
        return ThrowException(Exception::Error(String::New(should_be_a_string)));
    }
    
    String::Utf8Value address(args[0]);
    Local<Function> cb = Local<Function>::Cast(args[1]);
            
    DeviceINQ* inquire = ObjectWrap::Unwrap<DeviceINQ>(args.This());
    
    sdp_baton_t *baton = new sdp_baton_t();
    baton->inquire = inquire;
    baton->cb = Persistent<Function>::New(cb);
    strcpy(baton->address, *address);
    baton->channel = -1;
    baton->request.data = baton;
    baton->inquire->Ref();

    uv_queue_work(uv_default_loop(), &baton->request, EIO_SdpSearch, (uv_after_work_cb)EIO_AfterSdpSearch);

    return Undefined();
}
