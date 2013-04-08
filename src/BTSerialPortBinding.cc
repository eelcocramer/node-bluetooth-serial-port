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

class BTSerialPortBinding: ObjectWrap {
private:
    int s;
    
    struct connect_baton_t {
        BTSerialPortBinding *rfcomm;
        Persistent<Function> cb;
        Persistent<Function> ecb;
        char address[19];
        int status;
        int channel;
    };
    
    static void EIO_Connect(uv_work_t *req) {
        connect_baton_t *baton = static_cast<connect_baton_t *>(req->data);
        
        struct sockaddr_rc addr = { 0 };

        // allocate a socket
        baton->rfcomm->s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

        // set the connection parameters (who to connect to)
        addr.rc_family = AF_BLUETOOTH;
        addr.rc_channel = (uint8_t) baton->channel;
        str2ba( baton->address, &addr.rc_bdaddr );

        // connect to server
        baton->status = connect(baton->rfcomm->s, (struct sockaddr *)&addr, sizeof(addr));
    }
    
    static void EIO_AfterConnect(uv_work_t *req) {
        connect_baton_t *baton = static_cast<connect_baton_t *>(req->data);
        uv_unref((uv_handle_t*) &req);
        baton->rfcomm->Unref();
        
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
        
        baton->cb.Dispose();
        delete baton;
        delete req;
    }
    
    struct read_baton_t {
        BTSerialPortBinding *rfcomm;
        Persistent<Function> cb;
        char result[1024];
        int errorno;
        int size;
    };
 
    static void EIO_Read(uv_work_t *req) {
        char buf[1024]= { 0 };

        read_baton_t *baton = static_cast<read_baton_t *>(req->data);

        memset(buf, 0, sizeof(buf));
        
        baton->size = read(baton->rfcomm->s, buf, sizeof(buf));

        strcpy(baton->result, buf);
    }
    
    static void EIO_AfterRead(uv_work_t *req) {
        read_baton_t *baton = static_cast<read_baton_t *>(req->data);
        uv_unref((uv_handle_t*) &req);
        baton->rfcomm->Unref();
        
        TryCatch try_catch;
        
        Local<Value> argv[2];
        argv[0] = String::New(baton->result);
        argv[1] = Integer::New(baton->size);
        baton->cb->Call(Context::GetCurrent()->Global(), 2, argv);
        
        if (try_catch.HasCaught()) {
            FatalException(try_catch);
        }
        
        baton->cb.Dispose();
        delete baton;
        delete req;
    }
    
public:
    
    static Persistent<FunctionTemplate> s_ct;
    
    static void Init(Handle<Object> target) {
        HandleScope scope;
        
        Local<FunctionTemplate> t = FunctionTemplate::New(New);
        
        s_ct = Persistent<FunctionTemplate>::New(t);
        s_ct->InstanceTemplate()->SetInternalFieldCount(1);
        s_ct->SetClassName(String::NewSymbol("BTSerialPortBinding"));
        
        NODE_SET_PROTOTYPE_METHOD(s_ct, "write", Write);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "read", Read);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "close", Close);
        target->Set(String::NewSymbol("BTSerialPortBinding"), s_ct->GetFunction());
        target->Set(String::NewSymbol("BTSerialPortBinding"), s_ct->GetFunction());
        target->Set(String::NewSymbol("BTSerialPortBinding"), s_ct->GetFunction());
    }
    
    BTSerialPortBinding() : 
        s(0) {
        
    }
    
    ~BTSerialPortBinding() {
        
    }
    
    static Handle<Value> New(const Arguments& args) {
        HandleScope scope;

        const char *usage = "usage: BTSerialPortBinding(address, channel, callback, error)";
        if (args.Length() != 4) {
            return ThrowException(Exception::Error(String::New(usage)));
        }
        
        String::Utf8Value address(args[0]);
        
        int channel = args[1]->Int32Value(); 
        if (channel <= 0) { 
          return ThrowException(Exception::Error(String::New("Channel should be a positive int value.")));
        }
        
        Local<Function> cb = Local<Function>::Cast(args[2]);
        Local<Function> ecb = Local<Function>::Cast(args[3]);
        
        BTSerialPortBinding* rfcomm = new BTSerialPortBinding();
        rfcomm->Wrap(args.This());

        connect_baton_t *baton = new connect_baton_t();
        baton->rfcomm = rfcomm;
        baton->channel = channel;
        strcpy(baton->address, *address);
        baton->cb = Persistent<Function>::New(cb);
        baton->ecb = Persistent<Function>::New(ecb);
        rfcomm->Ref();

        uv_work_t *req = new uv_work_t;
        req->data = baton;
        uv_queue_work(uv_default_loop(), req, EIO_Connect, (uv_after_work_cb)EIO_AfterConnect);
        uv_ref((uv_handle_t *) &req);

        return args.This();
    }
    
    
    static Handle<Value> Write(const Arguments& args) {
        HandleScope scope;
        
        const char *usage = "usage: write(str)";
        if (args.Length() != 1) {
            return ThrowException(Exception::Error(String::New(usage)));
        }
        
        const char *should_be_a_string = "str must be a string";
        if (!args[0]->IsString()) {
            return ThrowException(Exception::Error(String::New(should_be_a_string)));
        }
        
        String::Utf8Value str(args[0]);
        
        BTSerialPortBinding* rfcomm = ObjectWrap::Unwrap<BTSerialPortBinding>(args.This());
        
        write(rfcomm->s, *str, str.length() + 1);
        
        return Undefined();
    }
 
    static Handle<Value> Close(const Arguments& args) {
        HandleScope scope;
        
        const char *usage = "usage: close()";
        if (args.Length() != 0) {
            return ThrowException(Exception::Error(String::New(usage)));
        }
        
        BTSerialPortBinding* rfcomm = ObjectWrap::Unwrap<BTSerialPortBinding>(args.This());
        
        close(rfcomm->s);
        
        return Undefined();
    }
 
    static Handle<Value> Read(const Arguments& args) {
        HandleScope scope;
        
        const char *usage = "usage: read(callback)";
        if (args.Length() != 1) {
            return ThrowException(Exception::Error(String::New(usage)));
        }
        
        Local<Function> cb = Local<Function>::Cast(args[0]);
                
        BTSerialPortBinding* rfcomm = ObjectWrap::Unwrap<BTSerialPortBinding>(args.This());
        
        read_baton_t *baton = new read_baton_t();
        baton->rfcomm = rfcomm;
        baton->cb = Persistent<Function>::New(cb);
        rfcomm->Ref();

        uv_work_t *req = new uv_work_t;
        req->data = baton;
        uv_queue_work(uv_default_loop(), req, EIO_Read, (uv_after_work_cb)EIO_AfterRead);
        uv_ref((uv_handle_t *) &req);

        return Undefined();
    }
    
};

Persistent<FunctionTemplate> BTSerialPortBinding::s_ct;

extern "C" {
    void init (Handle<Object> target) {
        HandleScope scope;
        BTSerialPortBinding::Init(target);
    }

    NODE_MODULE(BTSerialPortBinding, init);
}
