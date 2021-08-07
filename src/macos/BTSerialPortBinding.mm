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
#include <node_buffer.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "BTSerialPortBinding.h"
#include "BluetoothWorker.h"

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
    #include <sys/types.h>
    #include <assert.h>
}

#import <Foundation/NSObject.h>

using namespace node;
using namespace v8;

uv_mutex_t write_queue_mutex;
ngx_queue_t write_queue;

void BTSerialPortBinding::EIO_Connect(uv_work_t *req) {
    connect_baton_t *baton = static_cast<connect_baton_t *>(req->data);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSString *address = [NSString stringWithCString:baton->address encoding:NSASCIIStringEncoding];
    BluetoothWorker *worker = [BluetoothWorker getInstance: address];
    // create pipe to communicate with delegate
    pipe_t *pipe = pipe_new(sizeof(unsigned char), 0);

    IOReturn result = [worker connectDevice: address onChannel:baton->channelID withPipe:pipe];

    if (result == kIOReturnSuccess) {
        pipe_consumer_t *c = pipe_consumer_new(pipe);

        // save consumer side of the pipe
        baton->rfcomm->consumer = c;
        baton->status = 0;
    } else {
        baton->status = 1;
    }

    pipe_free(pipe);
    [pool release];
}

void BTSerialPortBinding::EIO_AfterConnect(uv_work_t *req) {
    Nan::HandleScope scope;

    connect_baton_t *baton = static_cast<connect_baton_t *>(req->data);

    Nan::TryCatch try_catch;

    Nan::AsyncResource resource("bluetooth-serial-port:Connect");
    if (baton->status == 0) {
        baton->cb->Call(0, NULL, &resource);
    } else {
        char msg[80];
        sprintf(msg, "Cannot connect: %d", baton->status);
        Local<Value> argv[] = {
            Nan::Error(msg)
        };
        baton->ecb->Call(1, argv, &resource);
    }

    if (try_catch.HasCaught()) {
        Nan::FatalException(try_catch);
    }

    baton->rfcomm->Unref();

    delete baton->cb;
    delete baton->ecb;
    delete baton;
    baton = NULL;
}

void BTSerialPortBinding::EIO_Write(uv_work_t *req) {
    queued_write_t *queuedWrite = static_cast<queued_write_t*>(req->data);
    write_baton_t *data = static_cast<write_baton_t*>(queuedWrite->baton);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSString *address = [NSString stringWithCString:data->address encoding:NSASCIIStringEncoding];
    BluetoothWorker *worker = [BluetoothWorker getInstance: address];

    if ([worker writeAsync: data->bufferData length: data->bufferLength toDevice: address] != kIOReturnSuccess) {
        sprintf(data->errorString, "Write was unsuccessful");
    } else {
        data->result = data->bufferLength;
    }

    [pool release];
}

void BTSerialPortBinding::EIO_AfterWrite(uv_work_t *req) {
    Nan::HandleScope scope;

    queued_write_t *queuedWrite = static_cast<queued_write_t*>(req->data);
    write_baton_t *data = static_cast<write_baton_t*>(queuedWrite->baton);

    Local<Value> argv[2];
    if (data->errorString[0]) {
        argv[0] = Nan::Error(data->errorString);
        argv[1] = Nan::Undefined();
    } else {
        argv[0] = Nan::Undefined();
        argv[1] = Nan::New<v8::Integer>((int32_t)data->result);
    }

    Nan::AsyncResource resource("bluetooth-serial-port:Write");
    data->callback->Call(2, argv, &resource);

    uv_mutex_lock(&write_queue_mutex);
    ngx_queue_remove(&queuedWrite->queue);

    if (!ngx_queue_empty(&write_queue)) {
        // Always pull the next work item from the head of the queue
        ngx_queue_t* head = ngx_queue_head(&write_queue);
        queued_write_t* nextQueuedWrite = ngx_queue_data(head, queued_write_t, queue);
        uv_queue_work(uv_default_loop(), &nextQueuedWrite->req, EIO_Write, (uv_after_work_cb)EIO_AfterWrite);
    }
    uv_mutex_unlock(&write_queue_mutex);

    data->buffer.Reset();
    delete data->callback;
    data->rfcomm->Unref();

    delete data;
    delete queuedWrite;
}

void BTSerialPortBinding::EIO_Read(uv_work_t *req) {
    unsigned char buf[1024] = { 0 };

    read_baton_t *baton = static_cast<read_baton_t *>(req->data);
    size_t size = 0;

    memset(buf, 0, sizeof(buf));

    if (baton->rfcomm->consumer != NULL) {
        size = pipe_pop_eager(baton->rfcomm->consumer, buf, sizeof(buf));
    }

    if (size == 0) {
        pipe_consumer_free(baton->rfcomm->consumer);
        baton->rfcomm->consumer = NULL;
    }

    // when no data is read from rfcomm the connection has been closed.
    baton->size = size;
    memcpy(&baton->result, buf, size);
}

void BTSerialPortBinding::EIO_AfterRead(uv_work_t *req) {
    Nan::HandleScope scope;

    read_baton_t *baton = static_cast<read_baton_t *>(req->data);

    Nan::TryCatch try_catch;

    Local<Value> argv[2];

    if (baton->size < 0) {
        argv[0] = Nan::Error("Error reading from connection");
        argv[1] = Nan::Undefined();
    } else {
        Local<Object> resultBuffer = Nan::NewBuffer(baton->size).ToLocalChecked();
        memcpy(Buffer::Data(resultBuffer), baton->result, baton->size);

        argv[0] = Nan::Undefined();
        argv[1] = resultBuffer;
    }

    Nan::AsyncResource resource("bluetooth-serial-port:Read");
    baton->cb->Call(2, argv, &resource);

    if (try_catch.HasCaught()) {
        Nan::FatalException(try_catch);
    }

    baton->rfcomm->Unref();
    delete baton->cb;
    delete baton;
    baton = NULL;
}

void BTSerialPortBinding::Init(Local<Object> target) {
    Nan::HandleScope scope;

    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(Nan::New("BTSerialPortBinding").ToLocalChecked());

    Isolate *isolate = target->GetIsolate();
    Local<Context> ctx = isolate->GetCurrentContext();

    Nan::SetPrototypeMethod(t, "write", Write);
    Nan::SetPrototypeMethod(t, "read", Read);
    Nan::SetPrototypeMethod(t, "close", Close);
    target->Set(ctx, Nan::New("BTSerialPortBinding").ToLocalChecked(), t->GetFunction(ctx).ToLocalChecked());
}

BTSerialPortBinding::BTSerialPortBinding() :
    consumer(NULL) {
}

BTSerialPortBinding::~BTSerialPortBinding() {
}

NAN_METHOD(BTSerialPortBinding::New) {
    uv_mutex_init(&write_queue_mutex);
    ngx_queue_init(&write_queue);

    const char *usage = "usage: BTSerialPortBinding(address, channelID, callback, error)";
    if (info.Length() != 4) {
        return Nan::ThrowError(usage);
    }

    String::Utf8Value address(info.GetIsolate(), info[0]);

    int channelID = info[1]->Int32Value(Nan::GetCurrentContext()).ToChecked();
    if (channelID <= 0) {
        return Nan::ThrowTypeError("ChannelID should be a positive int value.");
    }

    BTSerialPortBinding* rfcomm = new BTSerialPortBinding();
    rfcomm->Wrap(info.This());

    connect_baton_t *baton = new connect_baton_t();
    baton->rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBinding>(info.This());
    baton->channelID = channelID;

    strcpy(baton->address, *address);
    baton->cb = new Nan::Callback(info[2].As<Function>());
    baton->ecb = new Nan::Callback(info[3].As<Function>());
    baton->request.data = baton;
    baton->rfcomm->Ref();

    uv_queue_work(uv_default_loop(), &baton->request, EIO_Connect, (uv_after_work_cb)EIO_AfterConnect);

    info.GetReturnValue().Set(info.This());
}

NAN_METHOD(BTSerialPortBinding::Write) {
    // usage
    if (info.Length() != 3) {
        return Nan::ThrowError("usage: write(buf, address, callback)");
    }
    // buffer
    if(!info[0]->IsObject() || !Buffer::HasInstance(info[0])) {
        return Nan::ThrowTypeError("First argument must be a buffer");
    }
    // string
    if (!info[1]->IsString()) {
        return Nan::ThrowTypeError("Second argument must be a string");
    }

    Local<Object> bufferObject = info[0].As<Object>();
    void* bufferData = Buffer::Data(bufferObject);
    size_t bufferLength = Buffer::Length(bufferObject);

    // string
    if (!info[1]->IsString()) {
        Nan::ThrowTypeError("Second argument must be a string");
    }
    String::Utf8Value addressParameter(info.GetIsolate(), info[1]);

    // callback
    if(!info[2]->IsFunction()) {
        return Nan::ThrowTypeError("Third argument must be a function");
    }

    write_baton_t *baton = new write_baton_t();
    memset(baton, 0, sizeof(write_baton_t));
    strcpy(baton->address, *addressParameter);
    baton->rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBinding>(info.This());
    baton->rfcomm->Ref();
    baton->buffer.Reset(bufferObject);
    baton->bufferData = bufferData;
    baton->bufferLength = bufferLength;
    baton->callback = new Nan::Callback(info[2].As<Function>());

    queued_write_t *queuedWrite = new queued_write_t();
    memset(queuedWrite, 0, sizeof(queued_write_t));
    queuedWrite->baton = baton;
    queuedWrite->req.data = queuedWrite;

    uv_mutex_lock(&write_queue_mutex);
    bool empty = ngx_queue_empty(&write_queue);

    ngx_queue_insert_tail(&write_queue, &queuedWrite->queue);

    if (empty) {
        uv_queue_work(uv_default_loop(), &queuedWrite->req, EIO_Write, (uv_after_work_cb)EIO_AfterWrite);
    }
    uv_mutex_unlock(&write_queue_mutex);

    return;
}

NAN_METHOD(BTSerialPortBinding::Close) {
    if (info.Length() != 1) {
        return Nan::ThrowError("usage: close(address)");
    }

    if (!info[0]->IsString()) {
        return Nan::ThrowTypeError("Argument should be a string value");
    }

    //TODO should be a better way to do this...
    String::Utf8Value addressParameter(info.GetIsolate(), info[0]);
    char addressArray[32];
    strncpy(addressArray, *addressParameter, 32);
    NSString *address = [NSString stringWithCString:addressArray encoding:NSASCIIStringEncoding];

    BluetoothWorker *worker = [BluetoothWorker getInstance: address];
    [worker disconnectFromDevice: address];

    return;
}

NAN_METHOD(BTSerialPortBinding::Read) {
    if (info.Length() != 1) {
        return Nan::ThrowError("usage: read(callback)");
    }

    Local<Function> cb = info[0].As<Function>();

    BTSerialPortBinding* rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBinding>(info.This());

    // callback with an error if the connection has been closed.
    if (rfcomm->consumer == NULL) {
        Local<Value> argv[2];

        argv[0] = Nan::Error("The connection has been closed");
        argv[1] = Nan::Undefined();

        Nan::Callback *nc = new Nan::Callback(cb);
        Nan::AsyncResource resource("bluetooth-serial-port:Read");
        nc->Call(2, argv, &resource);
    } else {
        read_baton_t *baton = new read_baton_t();
        baton->rfcomm = rfcomm;
        baton->cb = new Nan::Callback(cb);
        baton->request.data = baton;
        baton->rfcomm->Ref();

        uv_queue_work(uv_default_loop(), &baton->request, EIO_Read, (uv_after_work_cb)EIO_AfterRead);
    }

    return;
}
