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
#include <nan.h>
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
    #include <time.h>
}

#import <Foundation/NSObject.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothRFCOMMChannel.h>
#import <IOBluetooth/objc/IOBluetoothSDPUUID.h>
#import <IOBluetooth/objc/IOBluetoothSDPServiceRecord.h>
#import "BluetoothWorker.h"

using namespace node;
using namespace v8;

void DeviceINQ::EIO_SdpSearch(uv_work_t *req) {
    sdp_baton_t *baton = static_cast<sdp_baton_t *>(req->data);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSString *address = [NSString stringWithCString:baton->address encoding:NSASCIIStringEncoding];
    BluetoothWorker *worker = [BluetoothWorker getInstance: address];
    baton->channelID = [worker getRFCOMMChannelID: address];

    [pool release];
}

void DeviceINQ::EIO_AfterSdpSearch(uv_work_t *req) {
    sdp_baton_t *baton = static_cast<sdp_baton_t *>(req->data);

    TryCatch try_catch;

    Handle<Value> argv[] = {
        NanNew(baton->channelID)
    };
    baton->cb->Call(1, argv);

    if (try_catch.HasCaught()) {
        FatalException(try_catch);
    }

    baton->inquire->Unref();
    delete baton->cb;
    delete baton;
    baton = NULL;
}

void DeviceINQ::Init(Handle<Object> target) {
    NanScope();

    Local<FunctionTemplate> t = NanNew<FunctionTemplate>(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(NanNew("DeviceINQ"));

    NODE_SET_PROTOTYPE_METHOD(t, "inquire", Inquire);
    NODE_SET_PROTOTYPE_METHOD(t, "findSerialPortChannel", SdpSearch);
    NODE_SET_PROTOTYPE_METHOD(t, "listPairedDevices", ListPairedDevices);
    target->Set(NanNew("DeviceINQ"), t->GetFunction());
    target->Set(NanNew("DeviceINQ"), t->GetFunction());
    target->Set(NanNew("DeviceINQ"), t->GetFunction());
}

DeviceINQ::DeviceINQ() {

}

DeviceINQ::~DeviceINQ() {

}

NAN_METHOD(DeviceINQ::New) {
    NanScope();

    const char *usage = "usage: DeviceINQ()";
    if (args.Length() != 0) {
        NanThrowError(usage);
    }

    DeviceINQ* inquire = new DeviceINQ();
    inquire->Wrap(args.This());

    NanReturnValue(args.This());
}

NAN_METHOD(DeviceINQ::Inquire) {
    NanScope();

    const char *usage = "usage: inquire()";
    if (args.Length() != 0) {
        NanThrowError(usage);
    }

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    BluetoothWorker *worker = [BluetoothWorker getInstance:nil];

    // create pipe to communicate with delegate
    pipe_t *pipe = pipe_new(sizeof(device_info_t), 0);
    [worker inquireWithPipe: pipe];
    pipe_consumer_t *c = pipe_consumer_new(pipe);
    pipe_free(pipe);

    device_info_t *info = new device_info_t;
    size_t result;

    do {
        result = pipe_pop_eager(c, info, 1);

        if (result != 0) {
            Local<Value> argv[3] = {
                NanNew("found"),
                NanNew(info->address),
                NanNew(info->name)
            };

            NanMakeCallback(args.This(), "emit", 3, argv);
        }
    } while (result != 0);

    delete info;
    pipe_consumer_free(c);

    Local<Value> argv[1] = {
        NanNew("finished")
    };

    NanMakeCallback(args.This(), "emit", 1, argv);

    [pool release];
    NanReturnUndefined();
}

NAN_METHOD(DeviceINQ::SdpSearch) {
    NanScope();

    const char *usage = "usage: sdpSearchForRFCOMM(address, callback)";
    if (args.Length() != 2) {
        NanThrowError(usage);
    }

    if (!args[0]->IsString()) {
        NanThrowTypeError("First argument should be a string value");
    }
    String::Utf8Value address(args[0]);

    if(!args[1]->IsFunction()) {
        NanThrowTypeError("Second argument must be a function");
    }
    Local<Function> cb = args[1].As<Function>();

    DeviceINQ* inquire = ObjectWrap::Unwrap<DeviceINQ>(args.This());

    sdp_baton_t *baton = new sdp_baton_t();
    baton->inquire = inquire;
    baton->cb = new NanCallback(cb);
    strcpy(baton->address, *address);
    baton->channelID = -1;
    baton->request.data = baton;
    baton->inquire->Ref();

    uv_queue_work(uv_default_loop(), &baton->request, EIO_SdpSearch, (uv_after_work_cb)EIO_AfterSdpSearch);

    NanReturnUndefined();
}

NAN_METHOD(DeviceINQ::ListPairedDevices) {
    NanScope();

    const char *usage = "usage: listPairedDevices(callback)";
    if (args.Length() != 1) {
        return NanThrowError(usage);
    }

    if(!args[0]->IsFunction()) {
        return NanThrowTypeError("First argument must be a function");
    }
    Local<Function> cb = args[0].As<Function>();

    NSArray *pairedDevices = [IOBluetoothDevice pairedDevices];

    Local<Array> resultArray = NanNew<v8::Array>((int)pairedDevices.count);

    // Builds an array of objects representing a paired device:
    // ex: {
    //   name: 'MyBluetoothDeviceName',
    //   address: '12-34-56-78-90',
    //   services: [
    //     { name: 'SPP', channel: 1 },
    //     { name: 'iAP', channel: 2 }
    //   ]
    // }
    for (int i = 0; i < (int)pairedDevices.count; ++i) {
        IOBluetoothDevice *device = [pairedDevices objectAtIndex:i];

        Local<Object> deviceObj = NanNew<v8::Object>();

        deviceObj->Set(NanNew("name"), NanNew([device.nameOrAddress UTF8String]));
        deviceObj->Set(NanNew("address"), NanNew([device.addressString UTF8String]));

        // A device may have multiple services, so enumerate each one
        Local<Array> servicesArray = NanNew<v8::Array>((int)device.services.count);
        for (int j = 0; j < (int)device.services.count; ++j) {
            IOBluetoothSDPServiceRecord *service = [device.services objectAtIndex:j];
            BluetoothRFCOMMChannelID channelID;
            [service getRFCOMMChannelID:&channelID];

            Local<Object> serviceObj = NanNew<v8::Object>();
            serviceObj->Set(NanNew("channel"), NanNew((int)channelID));

            if ([service getServiceName])
                serviceObj->Set(NanNew("name"), NanNew([[service getServiceName] UTF8String]));
            else
                serviceObj->Set(NanNew("name"), NanUndefined());

            servicesArray->Set(j, serviceObj);
        }
        deviceObj->Set(NanNew("services"), servicesArray);

        resultArray->Set(i, deviceObj);
    }

    Local<Value> argv[1] = {
        resultArray
    };
    cb->Call(NanGetCurrentContext()->Global(), 1, argv);

    NanReturnUndefined();
}
