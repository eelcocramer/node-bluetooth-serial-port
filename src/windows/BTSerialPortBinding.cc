/*
 * Copyright (c) 2013, Elmar Langholz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../BTSerialPortBinding.h"
#include "BluetoothHelpers.h"

using namespace std;
using namespace node;
using namespace v8;

uv_mutex_t write_queue_mutex;
ngx_queue_t write_queue;

void BTSerialPortBinding::EIO_Connect(uv_work_t *req) {
    connect_baton_t *baton = static_cast<connect_baton_t *>(req->data);

    baton->rfcomm->s = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (baton->rfcomm->s != SOCKET_ERROR) {
        SOCKADDR_BTH bluetoothSocketAddress = { 0 };
        int bluetoothSocketAddressSize = sizeof(SOCKADDR_BTH);
        int stringToAddressError = WSAStringToAddress(baton->address,
                                                      AF_BTH,
                                                      nullptr,
                                                      (LPSOCKADDR)&bluetoothSocketAddress,
                                                      &bluetoothSocketAddressSize);
        if (stringToAddressError != SOCKET_ERROR) {
            bluetoothSocketAddress.port = baton->channelID;
            baton->status = connect(baton->rfcomm->s,
                                   (LPSOCKADDR)&bluetoothSocketAddress,
                                   bluetoothSocketAddressSize);
            if (baton->status != SOCKET_ERROR) {
                unsigned long enableNonBlocking = 1;
                ioctlsocket(baton->rfcomm->s, FIONBIO, &enableNonBlocking);
            }
        }
    }
}

void BTSerialPortBinding::EIO_AfterConnect(uv_work_t *req) {
    Nan::HandleScope scope;

    connect_baton_t *baton = static_cast<connect_baton_t *>(req->data);

    Nan::TryCatch try_catch;
    Nan::AsyncResource resource("bluetooth-serial-port:Connect");
    if (baton->status == 0) {
        baton->cb->Call(0, nullptr, &resource);
    } else {
        if (baton->rfcomm->s != INVALID_SOCKET) {
            closesocket(baton->rfcomm->s);
        }

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
    baton = nullptr;
}

void BTSerialPortBinding::EIO_Write(uv_work_t *req) {
    queued_write_t *queuedWrite = static_cast<queued_write_t*>(req->data);
    write_baton_t *data = static_cast<write_baton_t*>(queuedWrite->baton);

    BTSerialPortBinding *rfcomm = data->rfcomm;
    int bytesToSend = data->bufferLength;
    data->result = 0;

    if (rfcomm->s != INVALID_SOCKET) {
        do{
            int bytesSent = send(rfcomm->s, (const char *)((char*)data->bufferData+data->result), bytesToSend, 0);
            if(bytesSent != SOCKET_ERROR) {
                bytesToSend -= bytesSent;
                data->result += bytesSent;
            } else {
                sprintf_s(data->errorString, "Writing attempt was unsuccessful");
                break;
            }
        } while(bytesToSend > 0);
    } else {
        sprintf_s(data->errorString, "Attempting to write to a closed connection");
    }
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
        argv[1] = Nan::New<v8::Integer>(static_cast<int32_t>(data->result));
    }

    Nan::AsyncResource resource("bluetooth-serial-port:Write");
    data->callback->Call(2, argv, &resource);

    uv_mutex_lock(&write_queue_mutex);
    ngx_queue_remove(&queuedWrite->queue);

    if (!ngx_queue_empty(&write_queue)) {
        // Always pull the next work item from the head of the queue
        ngx_queue_t *head = ngx_queue_head(&write_queue);
        queued_write_t *nextQueuedWrite = ngx_queue_data(head, queued_write_t, queue);
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
    unsigned char buf[1024]= { 0 };

    read_baton_t *baton = static_cast<read_baton_t *>(req->data);

    memset(buf, 0, _countof(buf));

    fd_set set;
    FD_ZERO(&set);
    FD_SET(baton->rfcomm->s, &set);

    if (select(static_cast<int>(baton->rfcomm->s) + 1, &set, nullptr, nullptr, nullptr) >= 0) {
        if (FD_ISSET(baton->rfcomm->s, &set)) {
            baton->size = recv(baton->rfcomm->s, (char *)buf, _countof(buf), 0);
        } else {
            // when no data is read from rfcomm the connection has been closed.
            baton->size = 0;
        }

        // determine if we read anything that we can copy.
        if (baton->size > 0) {
            memcpy_s(baton->result, _countof(baton->result), buf, baton->size);
        }
    }
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
        memcpy_s(Buffer::Data(resultBuffer), baton->size, baton->result, baton->size);
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
    baton = nullptr;
}

void BTSerialPortBinding::Init(Local<Object> target) {
    Nan::HandleScope scope;

    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);

    Isolate *isolate = target->GetIsolate();
    Local<Context> ctx = isolate->GetCurrentContext();

    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(Nan::New("BTSerialPortBinding").ToLocalChecked());

    Nan::SetPrototypeMethod(t, "write", Write);
    Nan::SetPrototypeMethod(t, "read", Read);
    Nan::SetPrototypeMethod(t, "close", Close);
    target->Set(ctx, Nan::New("BTSerialPortBinding").ToLocalChecked(), t->GetFunction(ctx).ToLocalChecked());
}

BTSerialPortBinding::BTSerialPortBinding() : s(INVALID_SOCKET) {
    initialized = BluetoothHelpers::Initialize();
}

BTSerialPortBinding::~BTSerialPortBinding() {
    if (initialized) {
        BluetoothHelpers::Finalize();
    }
}

NAN_METHOD(BTSerialPortBinding::New) {
    uv_mutex_init(&write_queue_mutex);
    ngx_queue_init(&write_queue);

    if (info.Length() != 4) {
        return Nan::ThrowError("usage: BTSerialPortBinding(address, channelID, callback, error)");
    }

    String::Utf8Value address(info.GetIsolate(), info[0]);
    int channelID = info[1]->Int32Value(Nan::GetCurrentContext()).ToChecked();
    if (channelID <= 0) {
        return Nan::ThrowTypeError("ChannelID should be a positive int value");
    }

    connect_baton_t *baton = new connect_baton_t();
    if (strcpy_s(baton->address, *address) != 0) {
        delete baton;
        return Nan::ThrowTypeError("Address (first argument) length is invalid");
    }

    BTSerialPortBinding *rfcomm = new BTSerialPortBinding();
    if (!rfcomm->Initialized) {
        delete baton;
        return Nan::ThrowTypeError("Unable to initialize socket library");
    }

    rfcomm->Wrap(info.This());

    baton->rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBinding>(info.This());
    baton->channelID = channelID;
    baton->status = SOCKET_ERROR;

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

    //NOTE: The address argument is currently only used in OSX.
    //      On windows each connection is handled by a separate object.

    // string
    if (!info[1]->IsString()) {
        return Nan::ThrowTypeError("Second argument must be a string");
    }

    // callback
    if(!info[2]->IsFunction()) {
       return Nan::ThrowTypeError("Third argument must be a function");
    }

    Local<Object> bufferObject = info[0].As<Object>();
    char *bufferData = Buffer::Data(bufferObject);
    size_t bufferLength = Buffer::Length(bufferObject);
    if (bufferLength > INT_MAX) {
        return Nan::ThrowTypeError("The size of the buffer is larger than supported");
    }

    write_baton_t *baton = new write_baton_t();
    memset(baton, 0, sizeof(write_baton_t));
    baton->rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBinding>(info.This());
    baton->rfcomm->Ref();
    baton->buffer.Reset(bufferObject);
    baton->bufferData = bufferData;
    baton->bufferLength = static_cast<int>(bufferLength);
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

NAN_METHOD(BTSerialPortBinding::Read) {
    if (info.Length() != 1) {
        return Nan::ThrowError("usage: read(callback)");
    }

    Local<Function> cb = Local<Function>::Cast(info[0]);

    BTSerialPortBinding *rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBinding>(info.This());

    // callback with an error if the connection has been closed.
    if (rfcomm->s == INVALID_SOCKET) {
        Local<Value> argv[2];

        argv[0] = Nan::Error("The connection has been closed");
        argv[1] = Nan::Undefined();

        Nan::AsyncResource resource("bluetooth-serial-port:Read");
        Nan::Callback *nc = new Nan::Callback(cb);
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

NAN_METHOD(BTSerialPortBinding::Close) {
    //NOTE: The address argument is currently only used in OSX.
    //      On windows each connection is handled by a separate object.

    if (info.Length() != 1) {
        return Nan::ThrowError("usage: close(address)");
    }

    BTSerialPortBinding *rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBinding>(info.This());

    if (rfcomm->s != INVALID_SOCKET) {
        shutdown(rfcomm->s, SD_BOTH);
        closesocket(rfcomm->s);
        rfcomm->s = INVALID_SOCKET;
    }

    return;
}
