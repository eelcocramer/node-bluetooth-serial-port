/*
 * Copyright (c) 2016, Juan Gómez
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NODE_BTSP_SRC_SERIAL_PORT_BINDING_SERVER_H
#define NODE_BTSP_SRC_SERIAL_PORT_BINDING_SERVER_H

#include <node.h>
#include <uv.h>
#include <nan.h>
#include <memory>
#include "ngx-queue.h"
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

class BTSerialPortBindingServer : public Nan::ObjectWrap {
    public:
        static void Init(v8::Local<v8::Object> exports);
        static NAN_METHOD(Write);
        static NAN_METHOD(Close);
        static NAN_METHOD(Read);
        static NAN_METHOD(DisconnectClient);
        static NAN_METHOD(IsOpen);

    private:

        struct listen_baton_t {
            BTSerialPortBindingServer *rfcomm;
            uv_work_t request;
            Nan::Callback* cb;
            Nan::Callback* ecb;
            char clientAddress[40];
            int status;
            int listeningChannelID;
            char errorString[1024];
            uuid_t uuid;
        };

        struct read_baton_t {
            BTSerialPortBindingServer *rfcomm;
            uv_work_t request;
            Nan::Callback* cb;
            unsigned char result[1024];
            int errorno;
            int size;
            bool isDisconnect;
            bool isClose;
        };

        struct write_baton_t {
            BTSerialPortBindingServer *rfcomm;
            char address[40];
            void* bufferData;
            size_t bufferLength;
            Nan::Persistent<v8::Object> buffer;
            Nan::Callback* callback;
            size_t result;
            char errorString[1024];
        };

        struct queued_write_t {
            uv_work_t req;
            ngx_queue_t queue;
            write_baton_t* baton;
        };

        int s;
        int rep[2];
        int mClientSocket = 0;

        listen_baton_t * mListenBaton = nullptr;
        sdp_session_t * mSdpSession = nullptr;

        uv_mutex_t mWriteQueueMutex;
        ngx_queue_t mWriteQueue;


        BTSerialPortBindingServer();
        ~BTSerialPortBindingServer();

        static NAN_METHOD(New);
        static void EIO_Listen(uv_work_t *req);
        static void EIO_AfterListen(uv_work_t *req);
        static void EIO_Write(uv_work_t *req);
        static void EIO_AfterWrite(uv_work_t *req);
        static void EIO_Read(uv_work_t *req);
        static void EIO_AfterRead(uv_work_t *req);

        void AdvertiseAndAccept();
        void Advertise();
        void CloseClientSocket();


        class ClientWorker : public Nan::AsyncWorker {
            public:
                ClientWorker(Nan::Callback *cb, listen_baton_t *baton);
                ~ClientWorker();
                void Execute() override;
                void HandleOKCallback() override;
            private:
                listen_baton_t * mBaton;

        };
};

#endif
