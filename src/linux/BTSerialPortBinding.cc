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
    connect_baton_t *baton = static_cast<connect_baton_t *>(req->data);
    
    TryCatch try_catch;
    
    if (baton->status == 0) {
        baton->cb->Call(Context::GetCurrent()->Global(), 0, NULL);
    } else {
        Local<Value> argv[1];
        argv[0] = String::New("Cannot connect.");
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
     
void BTSerialPortBinding::EIO_Read(uv_work_t *req) {
    char buf[1024]= { 0 };

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

        strcpy(baton->result, buf);
    }
}
    
void BTSerialPortBinding::EIO_AfterRead(uv_work_t *req) {
    read_baton_t *baton = static_cast<read_baton_t *>(req->data);
    
    TryCatch try_catch;
    
    Local<Value> argv[2];
    argv[0] = String::New(baton->result);
    argv[1] = Integer::New(baton->size);
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
    s(0) {
}

BTSerialPortBinding::~BTSerialPortBinding() {
    
}
    
Handle<Value> BTSerialPortBinding::New(const Arguments& args) {
    HandleScope scope;

    const char *usage = "usage: BTSerialPortBinding(address, channelID, callback, error)";
    if (args.Length() != 4) {
        return ThrowException(Exception::Error(String::New(usage)));
    }
    
    String::Utf8Value address(args[0]);
    
    int channelID = args[1]->Int32Value(); 
    if (channelID <= 0) { 
      return ThrowException(Exception::Error(String::New("ChannelID should be a positive int value.")));
    }

    Local<Function> cb = Local<Function>::Cast(args[2]);
    Local<Function> ecb = Local<Function>::Cast(args[3]);
    
    BTSerialPortBinding* rfcomm = new BTSerialPortBinding();
    rfcomm->Wrap(args.This());

    connect_baton_t *baton = new connect_baton_t();
    baton->rfcomm = ObjectWrap::Unwrap<BTSerialPortBinding>(args.This());
    baton->channelID = channelID;

    // allocate an error pipe
    if (pipe(baton->rfcomm->rep) == -1) {
      return ThrowException(Exception::Error(String::New("Cannot create pipe for reading.")));
    }

    int flags = fcntl(baton->rfcomm->rep[0], F_GETFL, 0);
    fcntl(baton->rfcomm->rep[0], F_SETFL, flags | O_NONBLOCK);

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
    
    const char *usage = "usage: write(str, address)";
    if (args.Length() != 2) {
        return ThrowException(Exception::Error(String::New(usage)));
    }

    //NOTE: The address argument is currently only used in OSX.
    //      On linux each connection is handled by a separate object.
    
    const char *should_be_a_string = "str must be a string";
    if (!args[0]->IsString()) {
        return ThrowException(Exception::Error(String::New(should_be_a_string)));
    }
    
    String::Utf8Value str(args[0]);
    
    BTSerialPortBinding* rfcomm = ObjectWrap::Unwrap<BTSerialPortBinding>(args.This());
    
    const char *has_been_closed = "connection has been closed";
    if (rfcomm->s == 0) {
        return ThrowException(Exception::Error(String::New(has_been_closed)));
    }

    int result = write(rfcomm->s, *str, str.length());

    const char *write_error = "write was unsuccessful";
    if (result != str.length()) {
        return ThrowException(Exception::Error(String::New(write_error)));
    }
    
    return Undefined();
}
 
Handle<Value> BTSerialPortBinding::Close(const Arguments& args) {
    HandleScope scope;
    
    const char *usage = "usage: close(address)";
    if (args.Length() != 1) {
        return ThrowException(Exception::Error(String::New(usage)));
    }
    
    //NOTE: The address argument is currently only used in OSX.
    //      On linux each connection is handled by a separate object.

    BTSerialPortBinding* rfcomm = ObjectWrap::Unwrap<BTSerialPortBinding>(args.This());

    if (rfcomm->s != 0) {
        close(rfcomm->s);
        write(rfcomm->rep[1], "close", (strlen("close")+1));
        rfcomm->s = 0;
    }    
    
    return Undefined();
}
 
Handle<Value> BTSerialPortBinding::Read(const Arguments& args) {
    HandleScope scope;
    
    const char *usage = "usage: read(callback)";
    if (args.Length() != 1) {
        return ThrowException(Exception::Error(String::New(usage)));
    }

    Local<Function> cb = Local<Function>::Cast(args[0]);
            
    BTSerialPortBinding* rfcomm = ObjectWrap::Unwrap<BTSerialPortBinding>(args.This());

    const char *has_been_closed = "connection has been closed";
    if (rfcomm->s == 0) {
        return ThrowException(Exception::Error(String::New(has_been_closed)));
    }
    
    read_baton_t *baton = new read_baton_t();
    baton->rfcomm = rfcomm;
    baton->cb = Persistent<Function>::New(cb);
    baton->request.data = baton;
    baton->rfcomm->Ref();

    uv_queue_work(uv_default_loop(), &baton->request, EIO_Read, (uv_after_work_cb)EIO_AfterRead);

    return Undefined();
}
