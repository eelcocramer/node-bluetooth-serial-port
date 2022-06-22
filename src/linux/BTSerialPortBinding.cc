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


    #include <bluetooth/bluetooth.h>
    #include <bluetooth/hci.h>
    #include <bluetooth/hci_lib.h>
    #include <bluetooth/sdp.h>
    #include <bluetooth/sdp_lib.h>
    #include <bluetooth/rfcomm.h>
}

using namespace std;
using namespace node;
using namespace v8;

static uv_mutex_t write_queue_mutex;
static ngx_queue_t write_queue;

void BTSerialPortBinding::EIO_Connect(uv_work_t *req) {
    connect_baton_t *baton = static_cast<connect_baton_t *>(req->data);

    struct sockaddr_rc addr = {
        0x00,
        { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
        0x00
    };

    // allocate a socket
    baton->rfcomm->s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

    // set the connection parameters (who to connect to)
    addr.rc_family = AF_BLUETOOTH;
    addr.rc_channel = (uint8_t) baton->channelID;
    str2ba( baton->address, &addr.rc_bdaddr );

    // connect to server
    baton->status = connect(baton->rfcomm->s, (struct sockaddr *)&addr, sizeof(addr));

    int sock_flags = fcntl(baton->rfcomm->s, F_GETFL, 0);
    fcntl(baton->rfcomm->s, F_SETFL, sock_flags | O_NONBLOCK);
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

    BTSerialPortBinding* rfcomm = data->rfcomm;
    int bytesToSend = data->bufferLength;
    data->result = 0;

    if (rfcomm->s != 0) {
        do {
            int bytesSent = write(rfcomm->s, data->bufferData+data->result, bytesToSend);
            if (bytesSent >= 0) {
                bytesToSend -= bytesSent;
                data->result += bytesSent;
            } else {
                sprintf(data->errorString, "Writing attempt was unsuccessful");
                break;
            }
        } while (bytesToSend > 0);
    } else {
        sprintf(data->errorString, "Attempting to write to a closed connection");
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
    unsigned char buf[1024]= { 0 };

    read_baton_t *baton = static_cast<read_baton_t *>(req->data);

    memset(buf, 0, sizeof(buf));

    fd_set set;
    FD_ZERO(&set);
    FD_SET(baton->rfcomm->s, &set);
    FD_SET(baton->rfcomm->rep[0], &set);

    int nfds = (baton->rfcomm->s > baton->rfcomm->rep[0]) ? baton->rfcomm->s : baton->rfcomm->rep[0];

    if (pselect(nfds + 1, &set, NULL, NULL, NULL, NULL) >= 0) {
        if (FD_ISSET(baton->rfcomm->s, &set)) {
            baton->size = read(baton->rfcomm->s, buf, sizeof(buf));
        } else {
            // when no data is read from rfcomm the connection has been closed.
            baton->size = 0;
        }

        // determine if we read anything that we can copy.
        if (baton->size > 0) {
            memcpy(baton->result, buf, baton->size);
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
    Nan::Set(target, Nan::New("BTSerialPortBinding").ToLocalChecked(), t->GetFunction(ctx).ToLocalChecked());
}

BTSerialPortBinding::BTSerialPortBinding() :
    s(0) {
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

    // allocate an error pipe
    if (pipe(baton->rfcomm->rep) == -1) {
        Nan::ThrowError("Cannot create pipe for reading.");
    }

    int flags = fcntl(baton->rfcomm->rep[0], F_GETFL, 0);
    fcntl(baton->rfcomm->rep[0], F_SETFL, flags | O_NONBLOCK);

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

    Local<Object> bufferObject = info[0].As<Object>();
    char* bufferData = Buffer::Data(bufferObject);
    size_t bufferLength = Buffer::Length(bufferObject);

    // string
    if (!info[1]->IsString()) {
        return Nan::ThrowTypeError("Second argument must be a string");
    }
    //NOTE: The address argument is currently only used in OSX.
    //      On linux each connection is handled by a separate object.

    // callback
    if(!info[2]->IsFunction()) {
        return Nan::ThrowTypeError("Third argument must be a function");
    }

    write_baton_t *baton = new write_baton_t();
    memset(baton, 0, sizeof(write_baton_t));
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
    const char *usage = "usage: close(address)";
    if (info.Length() != 1) {
        return Nan::ThrowError(usage);
    }

    //NOTE: The address argument is currently only used in OSX.
    //      On linux each connection is handled by a separate object.

    BTSerialPortBinding* rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBinding>(info.This());

    if (rfcomm->s != 0) {
        shutdown(rfcomm->s, SHUT_RDWR);
        close(rfcomm->s);
        write(rfcomm->rep[1], "close", (strlen("close")+1));
        rfcomm->s = 0;
    }

    // closing pipes
    close(rfcomm->rep[0]);
    close(rfcomm->rep[1]);

    return;
}

NAN_METHOD(BTSerialPortBinding::Read) {
    const char *usage = "usage: read(callback)";
    if (info.Length() != 1) {
        return Nan::ThrowError(usage);
    }

    Local<Function> cb = info[0].As<Function>();

    BTSerialPortBinding* rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBinding>(info.This());

    // callback with an error if the connection has been closed.
    if (rfcomm->s == 0) {
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
