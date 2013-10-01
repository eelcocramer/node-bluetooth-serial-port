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

#ifndef NODE_BTSP_SRC_SERIAL_PORT_BINDING_H
#define NODE_BTSP_SRC_SERIAL_PORT_BINDING_H

#include <node.h>

#ifdef __APPLE__
#import <Foundation/NSObject.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothDeviceInquiry.h>
#import "pipe.h"
#endif

class BTSerialPortBinding : public node::ObjectWrap {
    private:
#ifdef _WINDOWS_
        bool initialized;

        bool GetInitializedProperty() {
            return initialized;
        }
#endif

    public:
#ifdef _WINDOWS_
        __declspec(property(get = GetInitializedProperty)) bool Initialized;
#endif

        static v8::Persistent<v8::FunctionTemplate> s_ct;
        static void Init(v8::Handle<v8::Object> exports);
        static v8::Handle<v8::Value> Write(const v8::Arguments& args);
        static v8::Handle<v8::Value> Close(const v8::Arguments& args);
        static v8::Handle<v8::Value> Read(const v8::Arguments& args);

    private:
        struct connect_baton_t {
            BTSerialPortBinding *rfcomm;
            uv_work_t request;
            v8::Persistent<v8::Function> cb;
            v8::Persistent<v8::Function> ecb;
            char address[40];
            int status;
            int channelID;
        };

        struct read_baton_t {
            BTSerialPortBinding *rfcomm;
            uv_work_t request;
            v8::Persistent<v8::Function> cb;
#ifdef _WINDOWS_
            unsigned char result[1024];
#else
            unsigned int result[1024];
#endif
            int errorno;
            int size;
        };

        struct write_baton_t {
            BTSerialPortBinding *rfcomm;
            char address[40];
            void* bufferData;
            int bufferLength;
            v8::Persistent<v8::Object> buffer;
            v8::Persistent<v8::Value> callback;
            size_t result;
            char errorString[1024];
        };

        struct queued_write_t {
            uv_work_t req;
            ngx_queue_t queue;
            write_baton_t* baton;
        };


#ifdef __APPLE__
        pipe_consumer_t *consumer;
#else
#ifdef _WINDOWS_
        SOCKET s;
#else
        int s;
        int rep[2];
#endif
#endif

        BTSerialPortBinding();
        ~BTSerialPortBinding();

        static v8::Handle<v8::Value> New(const v8::Arguments& args);
        static void EIO_Connect(uv_work_t *req);
        static void EIO_AfterConnect(uv_work_t *req);
        static void EIO_Write(uv_work_t *req);
        static void EIO_AfterWrite(uv_work_t *req);
        static void EIO_Read(uv_work_t *req);
        static void EIO_AfterRead(uv_work_t *req);
};

#endif
