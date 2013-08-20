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
    BluetoothWorker *worker = [BluetoothWorker getInstance];
    // create pipe to communicate with delegate
    pipe_t *pipe = pipe_new(sizeof(int), 0);

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
    connect_baton_t *baton = static_cast<connect_baton_t *>(req->data);
    
    TryCatch try_catch;

    if (baton->status == 0) {
        baton->cb->Call(Context::GetCurrent()->Global(), 0, NULL);
    } else {
        Local<Value> argv[1];
        argv[0] = Exception::Error(String::New("Cannot connect"));
        baton->ecb->Call(Context::GetCurrent()->Global(), 1, argv);
    }
    
    if (try_catch.HasCaught()) {
        FatalException(try_catch);
    }
    
    baton->rfcomm->Unref();
    baton->cb.Dispose();

    delete baton;
    baton = NULL;
}

void BTSerialPortBinding::EIO_Write(uv_work_t *req) {
    queued_write_t *queuedWrite = static_cast<queued_write_t*>(req->data);
    write_baton_t *data = static_cast<write_baton_t*>(queuedWrite->baton);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    BluetoothWorker *worker = [BluetoothWorker getInstance];
    NSString *address = [NSString stringWithCString:data->address encoding:NSASCIIStringEncoding];

    if ([worker writeSync: data->bufferData length: data->bufferLength toDevice: address] != kIOReturnSuccess) {
        sprintf(data->errorString, "Write was unsuccessful");
    } else {
        data->result = data->bufferLength;
    }
    
    [pool release];
}

void BTSerialPortBinding::EIO_AfterWrite(uv_work_t *req) {
    queued_write_t *queuedWrite = static_cast<queued_write_t*>(req->data);
    write_baton_t *data = static_cast<write_baton_t*>(queuedWrite->baton);

    Handle<Value> argv[2];
    if (data->errorString[0]) {
        argv[0] = Exception::Error(String::New(data->errorString));
        argv[1] = Undefined();
    } else {
        argv[0] = Undefined();
        argv[1] = v8::Int32::New(data->result);
    }

    Function::Cast(*data->callback)->Call(Context::GetCurrent()->Global(), 2, argv);

    uv_mutex_lock(&write_queue_mutex);
    ngx_queue_remove(&queuedWrite->queue);

    if (!ngx_queue_empty(&write_queue)) {
        // Always pull the next work item from the head of the queue
        ngx_queue_t* head = ngx_queue_head(&write_queue);
        queued_write_t* nextQueuedWrite = ngx_queue_data(head, queued_write_t, queue);
        uv_queue_work(uv_default_loop(), &nextQueuedWrite->req, EIO_Write, (uv_after_work_cb)EIO_AfterWrite);
    }
    uv_mutex_unlock(&write_queue_mutex);

    data->buffer.Dispose();
    data->callback.Dispose();
    data->rfcomm->Unref();

    delete data;
    delete queuedWrite;
}
     
void BTSerialPortBinding::EIO_Read(uv_work_t *req) {
    unsigned int buf[1024] = { 0 };

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
    read_baton_t *baton = static_cast<read_baton_t *>(req->data);
    
    TryCatch try_catch;

    Handle<Value> argv[2];

    if (baton->size < 0) {
        argv[0] = Exception::Error(String::New("Error reading from connection"));
        argv[1] = Undefined();
    } else {
        Buffer *buffer = Buffer::New(baton->size);
        memcpy(Buffer::Data(buffer), baton->result, baton->size);
        Local<Object> globalObj = Context::GetCurrent()->Global();
        Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));
        Handle<Value> constructorArgs[3] = { buffer->handle_, Integer::New(baton->size), Integer::New(0) };
        Local<Object> resultBuffer = bufferConstructor->NewInstance(3, constructorArgs);

        argv[0] = Undefined();
        argv[1] = resultBuffer;
    }

    baton->cb->Call(Context::GetCurrent()->Global(), 2, argv);
    
    if (try_catch.HasCaught()) {
        FatalException(try_catch);
    }
    
    baton->rfcomm->Unref();
    baton->cb.Dispose();
    delete baton;
    baton = NULL;
}
    
void BTSerialPortBinding::Init(Handle<Object> target) {
    HandleScope scope;
    
    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    
    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(String::NewSymbol("BTSerialPortBinding"));
    
    NODE_SET_PROTOTYPE_METHOD(t, "write", Write);
    NODE_SET_PROTOTYPE_METHOD(t, "read", Read);
    NODE_SET_PROTOTYPE_METHOD(t, "close", Close);
    target->Set(String::NewSymbol("BTSerialPortBinding"), t->GetFunction());
    target->Set(String::NewSymbol("BTSerialPortBinding"), t->GetFunction());
    target->Set(String::NewSymbol("BTSerialPortBinding"), t->GetFunction());
}
    
BTSerialPortBinding::BTSerialPortBinding() : 
    consumer(NULL) {
}

BTSerialPortBinding::~BTSerialPortBinding() {
}
    
Handle<Value> BTSerialPortBinding::New(const Arguments& args) {
    HandleScope scope;

    uv_mutex_init(&write_queue_mutex);
    ngx_queue_init(&write_queue);

    const char *usage = "usage: BTSerialPortBinding(address, channelID, callback, error)";
    if (args.Length() != 4) {
        return scope.Close(ThrowException(Exception::Error(String::New(usage))));
    }
    
    String::Utf8Value address(args[0]);
    
    int channelID = args[1]->Int32Value(); 
    if (channelID <= 0) { 
        return scope.Close(ThrowException(Exception::TypeError(String::New("ChannelID should be a positive int value."))));
    }

    Local<Function> cb = Local<Function>::Cast(args[2]);
    Local<Function> ecb = Local<Function>::Cast(args[3]);
    
    BTSerialPortBinding* rfcomm = new BTSerialPortBinding();
    rfcomm->Wrap(args.This());

    connect_baton_t *baton = new connect_baton_t();
    baton->rfcomm = ObjectWrap::Unwrap<BTSerialPortBinding>(args.This());
    baton->channelID = channelID;

    strcpy(baton->address, *address);
    baton->cb = Persistent<Function>::New(cb);
    baton->ecb = Persistent<Function>::New(ecb);
    baton->request.data = baton;
    baton->rfcomm->Ref();

    uv_queue_work(uv_default_loop(), &baton->request, EIO_Connect, (uv_after_work_cb)EIO_AfterConnect);

    return args.This();
}
    
Handle<Value> BTSerialPortBinding::Write(const Arguments& args) {
    HandleScope scope;

    // usage    
    if (args.Length() != 3) {
        return scope.Close(ThrowException(Exception::Error(String::New("usage: write(buf, address, callback)"))));
    }
    
    // buffer
    if(!args[0]->IsObject() || !Buffer::HasInstance(args[0])) {
        return scope.Close(ThrowException(Exception::TypeError(String::New("First argument must be a buffer"))));
    }
    Persistent<Object> buffer = Persistent<Object>::New(args[0]->ToObject());
    void* bufferData = Buffer::Data(buffer);
    size_t bufferLength = Buffer::Length(buffer);

    // string
    if (!args[1]->IsString()) {
        return scope.Close(ThrowException(Exception::TypeError(String::New("Second argument must be a string"))));
    }
    String::Utf8Value addressParameter(args[1]);

    // callback
    if(!args[2]->IsFunction()) {
        return scope.Close(ThrowException(Exception::TypeError(String::New("Third argument must be a function"))));
    }
    v8::Local<v8::Value> callback = args[2];

    write_baton_t *baton = new write_baton_t();
    memset(baton, 0, sizeof(write_baton_t));
    strcpy(baton->address, *addressParameter);
    baton->rfcomm = ObjectWrap::Unwrap<BTSerialPortBinding>(args.This());
    baton->rfcomm->Ref();
    baton->buffer = buffer;
    baton->bufferData = bufferData;
    baton->bufferLength = bufferLength;
    baton->callback = v8::Persistent<v8::Value>::New(callback);

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

    return scope.Close(v8::Undefined());
}
 
Handle<Value> BTSerialPortBinding::Close(const Arguments& args) {
    HandleScope scope;
    
    if (args.Length() != 1) {
        return scope.Close(ThrowException(Exception::Error(String::New("usage: close(address)"))));
    }
    
    if (!args[0]->IsString()) {
        return scope.Close(ThrowException(Exception::TypeError(String::New("Argument should be a string value"))));
    }

    //TODO should be a better way to do this...
    String::Utf8Value addressParameter(args[0]);
    char addressArray[16];
    strcpy(addressArray, *addressParameter);
    NSString *address = [NSString stringWithCString:addressArray encoding:NSASCIIStringEncoding];

    BluetoothWorker *worker = [BluetoothWorker getInstance];
    [worker disconnectFromDevice: address];

    return Undefined();
}
 
Handle<Value> BTSerialPortBinding::Read(const Arguments& args) {
    HandleScope scope;
    
    if (args.Length() != 1) {
        return scope.Close(ThrowException(Exception::Error(String::New("usage: read(callback)"))));
    }

    Local<Function> cb = Local<Function>::Cast(args[0]);
            
    BTSerialPortBinding* rfcomm = ObjectWrap::Unwrap<BTSerialPortBinding>(args.This());

    if (rfcomm->consumer == NULL) {
        return scope.Close(ThrowException(Exception::Error(String::New("connection has been closed"))));
    }
    
    read_baton_t *baton = new read_baton_t();
    baton->rfcomm = rfcomm;
    baton->cb = Persistent<Function>::New(cb);
    baton->request.data = baton;
    baton->rfcomm->Ref();

    uv_queue_work(uv_default_loop(), &baton->request, EIO_Read, (uv_after_work_cb)EIO_AfterRead);

    return Undefined();
}
