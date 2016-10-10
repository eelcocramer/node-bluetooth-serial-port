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

#ifndef NODE_BTSP_SRC_DEVICE_INQ_H
#define NODE_BTSP_SRC_DEVICE_INQ_H

#include <node.h>
#include <uv.h>
#include <nan.h>

struct bt_list {
    char addr[19];
    char name[248];
};

struct bt_inquiry {
    int num_rsp;
    bt_list *devices;
};

class DeviceINQ : public Nan::ObjectWrap {
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
        
        static void Init(v8::Handle<v8::Object> exports);
        static void EIO_SdpSearch(uv_work_t *req);
        static void EIO_AfterSdpSearch(uv_work_t *req);
        static bt_inquiry doInquire();
        
    private:
        struct sdp_baton_t {
            DeviceINQ *inquire;
            uv_work_t request;
            Nan::Callback* cb;
            int channelID;
            char address[40];
        };

        DeviceINQ();
        ~DeviceINQ();

        static NAN_METHOD(New);
        static NAN_METHOD(Inquire);
        static NAN_METHOD(InquireSync);
        static NAN_METHOD(SdpSearch);
        static NAN_METHOD(ListPairedDevices);

};

#endif
