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

class InquireWorker : public Nan::AsyncWorker {
 public:
  InquireWorker(Nan::Callback* found, Nan::Callback *callback)
    : Nan::AsyncWorker(callback), found(found) {}
  ~InquireWorker() {}

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
    inquiryResult = DeviceINQ::doInquire();
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback () {
    Nan::HandleScope scope;
    struct bt_device device;
    NSValue *boxedDevice;

    NSEnumerator *enumerator = [inquiryResult objectEnumerator];
    while (boxedDevice = [enumerator nextObject]) {
        [boxedDevice getValue:&device];
        Local<Value> argv[] = {
            Nan::New(device.address).ToLocalChecked(),
            Nan::New(device.name).ToLocalChecked()
        };
        found->Call(2, argv, async_resource);
    }

    callback->Call(0, 0, async_resource);
    [inquiryResult release];
  }

  private:
    NSArray *inquiryResult;
    Nan::Callback* found;
};

void DeviceINQ::EIO_SdpSearch(uv_work_t *req) {
    sdp_baton_t *baton = static_cast<sdp_baton_t *>(req->data);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSString *address = [NSString stringWithCString:baton->address encoding:NSASCIIStringEncoding];
    BluetoothWorker *worker = [BluetoothWorker getInstance: address];
    baton->channelID = [worker getRFCOMMChannelID: address];

    [pool release];
}

void DeviceINQ::EIO_AfterSdpSearch(uv_work_t *req) {
    Nan::HandleScope scope;

    sdp_baton_t *baton = static_cast<sdp_baton_t *>(req->data);

    Nan::TryCatch try_catch;

    Local<Value> argv[] = {
        Nan::New(baton->channelID)
    };
    Nan::Call(*baton->cb, 1, argv);

    if (try_catch.HasCaught()) {
        Nan::FatalException(try_catch);
    }

    baton->inquire->Unref();
    delete baton->cb;
    delete baton;
    baton = NULL;
}

void DeviceINQ::Init(Local<Object> target) {
    Nan::HandleScope scope;

    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(Nan::New("DeviceINQ").ToLocalChecked());

    Isolate *isolate = target->GetIsolate();
    Local<Context> ctx = isolate->GetCurrentContext();

    Nan::SetPrototypeMethod(t, "inquireSync", InquireSync);
    Nan::SetPrototypeMethod(t, "inquire", Inquire);
    Nan::SetPrototypeMethod(t, "findSerialPortChannel", SdpSearch);
    Nan::SetPrototypeMethod(t, "listPairedDevices", ListPairedDevices);
    target->Set(ctx, Nan::New("DeviceINQ").ToLocalChecked(), t->GetFunction(ctx).ToLocalChecked());
}

NSArray *DeviceINQ::doInquire() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    BluetoothWorker *worker = [BluetoothWorker getInstance:nil];
    NSMutableArray *devices = [[NSMutableArray alloc] init];

    // create pipe to communicate with delegate
    pipe_t *pipe = pipe_new(sizeof(device_info_t), 0);
    [worker inquireWithPipe: pipe];
    pipe_consumer_t *c = pipe_consumer_new(pipe);
    pipe_free(pipe);

    device_info_t *infod = new device_info_t;
    size_t result;

    do {
        result = pipe_pop_eager(c, infod, 1);

        if (result != 0) {
            bt_device device;
            strcpy(device.name, infod->name);
            strcpy(device.address, infod->address);

            NSValue *boxedDevice = [NSValue valueWithBytes:&device objCType:@encode(struct bt_device)];
            [devices addObject:boxedDevice];
        }
    } while (result != 0);

    delete infod;
    pipe_consumer_free(c);

    [pool release];

    return devices;
}

DeviceINQ::DeviceINQ() {

}

DeviceINQ::~DeviceINQ() {

}

NAN_METHOD(DeviceINQ::New) {
    const char *usage = "usage: DeviceINQ()";
    if (info.Length() != 0) {
        Nan::ThrowError(usage);
    }

    DeviceINQ* inquire = new DeviceINQ();
    inquire->Wrap(info.This());

    info.GetReturnValue().Set(info.This());
}

// Asynchronous access to the `Inquire()` function
NAN_METHOD(DeviceINQ::Inquire) {
  const char *usage = "usage: inquire(found, callback)";
  if (info.Length() != 2) {
      Nan::ThrowError(usage);
  }
  Nan::Callback *found = new Nan::Callback(info[0].As<Function>());
  Nan::Callback *callback = new Nan::Callback(info[1].As<Function>());

  Nan::AsyncQueueWorker(new InquireWorker(found, callback));
}

NAN_METHOD(DeviceINQ::InquireSync) {
    const char *usage = "usage: inquireSync(found, callback)";
    if (info.Length() != 2) {
        Nan::ThrowError(usage);
    }

    Nan::Callback *found = new Nan::Callback(info[0].As<Function>());
    Nan::Callback *callback = new Nan::Callback(info[1].As<Function>());

    NSValue *boxedDevice;
    NSArray *inquiryResult = doInquire();
    NSEnumerator *enumerator = [inquiryResult objectEnumerator];
    Nan::AsyncResource resource("bluetooth-serial-port:inqureSync");
    while (boxedDevice = [enumerator nextObject]) {
        struct bt_device device;

        [boxedDevice getValue:&device];
        Local<Value> argv[] = {
            Nan::New(device.address).ToLocalChecked(),
            Nan::New(device.name).ToLocalChecked()
        };

        found->Call(2, argv, &resource);
    }

    callback->Call(0, 0, &resource);
    [inquiryResult release];

    return;
}

NAN_METHOD(DeviceINQ::SdpSearch) {
    const char *usage = "usage: sdpSearchForRFCOMM(address, callback)";
    if (info.Length() != 2) {
        return Nan::ThrowError(usage);
    }

    if (!info[0]->IsString()) {
        return Nan::ThrowTypeError("First argument should be a string value");
    }
    String::Utf8Value address(info.GetIsolate(), info[0]);

    if(!info[1]->IsFunction()) {
        return Nan::ThrowTypeError("Second argument must be a function");
    }
    Local<Function> cb = info[1].As<Function>();

    DeviceINQ* inquire = Nan::ObjectWrap::Unwrap<DeviceINQ>(info.This());

    sdp_baton_t *baton = new sdp_baton_t();
    baton->inquire = inquire;
    baton->cb = new Nan::Callback(cb);
    strcpy(baton->address, *address);
    baton->channelID = -1;
    baton->request.data = baton;
    baton->inquire->Ref();

    uv_queue_work(uv_default_loop(), &baton->request, EIO_SdpSearch, (uv_after_work_cb)EIO_AfterSdpSearch);

    return;
}

NAN_METHOD(DeviceINQ::ListPairedDevices) {
    const char *usage = "usage: listPairedDevices(callback)";
    if (info.Length() != 1) {
        return Nan::ThrowError(usage);
    }

    if(!info[0]->IsFunction()) {
        return Nan::ThrowTypeError("First argument must be a function");
    }
    Local<Function> cb = info[0].As<Function>();

    NSArray *pairedDevices = [IOBluetoothDevice pairedDevices];

    Local<Array> resultArray = Nan::New<v8::Array>((int)pairedDevices.count);

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

        Local<Object> deviceObj = Nan::New<v8::Object>();

        Nan::Set(deviceObj, Nan::New("name").ToLocalChecked(), Nan::New([device.nameOrAddress UTF8String]).ToLocalChecked());
        Nan::Set(deviceObj, Nan::New("address").ToLocalChecked(), Nan::New([device.addressString UTF8String]).ToLocalChecked());

        // A device may have multiple services, so enumerate each one
        Local<Array> servicesArray = Nan::New<v8::Array>((int)device.services.count);
        for (int j = 0; j < (int)device.services.count; ++j) {
            IOBluetoothSDPServiceRecord *service = [device.services objectAtIndex:j];
            BluetoothRFCOMMChannelID channelID;
            [service getRFCOMMChannelID:&channelID];

            Local<Object> serviceObj = Nan::New<v8::Object>();
            Nan::Set(serviceObj, Nan::New("channel").ToLocalChecked(), Nan::New((int)channelID));

            if ([service getServiceName])
                Nan::Set(serviceObj, Nan::New("name").ToLocalChecked(), Nan::New([[service getServiceName] UTF8String]).ToLocalChecked());
            else
                Nan::Set(serviceObj, Nan::New("name").ToLocalChecked(), Nan::Undefined());

            Nan::Set(servicesArray, j, serviceObj);
        }
        Nan::Set(deviceObj, Nan::New("services").ToLocalChecked(), servicesArray);

        Nan::Set(resultArray, i, deviceObj);
    }

    Local<Value> argv[1] = {
        resultArray
    };
    cb->Call(Nan::GetCurrentContext(), Nan::GetCurrentContext()->Global(), 1, argv);

    return;
}
