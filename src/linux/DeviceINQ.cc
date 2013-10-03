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

void DeviceINQ::EIO_SdpSearch(uv_work_t *req) {
    sdp_baton_t *baton = static_cast<sdp_baton_t *>(req->data);

    // default, no channel is found
    baton->channelID = -1;

    uuid_t svc_uuid;
    bdaddr_t target;
    bdaddr_t source = { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };
    sdp_list_t *response_list = NULL, *search_list, *attrid_list;
    sdp_session_t *session = 0;

    str2ba(baton->address, &target);
    
    // connect to the SDP server running on the remote machine
    // session = sdp_connect(BDADDR_ANY, &target, SDP_RETRY_IF_BUSY);
    session = sdp_connect(&source, &target, SDP_RETRY_IF_BUSY);
    
    if (!session) {
        return;
    }
    
    // specify the UUID of the application we're searching for
    sdp_uuid16_create(&svc_uuid, SERIAL_PORT_PROFILE_ID);
    search_list = sdp_list_append(NULL, &svc_uuid);

    // specify that we want a list of all the matching applications' attributes
    uint32_t range = 0x0000ffff;
    attrid_list = sdp_list_append(NULL, &range);

    // get a list of service records that have the serial port UUID
    sdp_service_search_attr_req( session, search_list, SDP_ATTR_REQ_RANGE, attrid_list, &response_list);

    sdp_list_t *r = response_list;

    // go through each of the service records
    for (; r; r = r->next ) {
        sdp_record_t *rec = (sdp_record_t*) r->data;
        sdp_list_t *proto_list;

        // get a list of the protocol sequences
        if( sdp_get_access_protos( rec, &proto_list ) == 0 ) {
            sdp_list_t *p = proto_list;

            // go through each protocol sequence
            for( ; p ; p = p->next ) {
                sdp_list_t *pds = (sdp_list_t*)p->data;

                // go through each protocol list of the protocol sequence
                for( ; pds ; pds = pds->next ) {

                    // check the protocol attributes
                    sdp_data_t *d = (sdp_data_t*)pds->data;
                    int proto = 0;
                    for( ; d; d = d->next ) {
                        switch( d->dtd ) { 
                            case SDP_UUID16:
                            case SDP_UUID32:
                            case SDP_UUID128:
                                proto = sdp_uuid_to_proto( &d->val.uuid );
                                break;
                            case SDP_UINT8:
                                if( proto == RFCOMM_UUID ) {
                                    baton->channelID = d->val.int8;
                                    return; // stop if channel is found
                                }
                                break;
                        }
                    }
                }
                sdp_list_free((sdp_list_t*)p->data, 0 );
            }
            sdp_list_free(proto_list, 0 );
        }

        sdp_record_free( rec );
    }

    sdp_close(session);
}

void DeviceINQ::EIO_AfterSdpSearch(uv_work_t *req) {
    sdp_baton_t *baton = static_cast<sdp_baton_t *>(req->data);

    TryCatch try_catch;
    
    Local<Value> argv[1];
    argv[0] = Integer::New(baton->channelID);
    baton->cb->Call(Context::GetCurrent()->Global(), 1, argv);
    
    if (try_catch.HasCaught()) {
        FatalException(try_catch);
    }

    baton->inquire->Unref();
    baton->cb.Dispose();
    delete baton;
    baton = NULL;
}

void DeviceINQ::Init(Handle<Object> target) {
    HandleScope scope;
    
    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(String::NewSymbol("DeviceINQ"));
    
    NODE_SET_PROTOTYPE_METHOD(t, "inquire", Inquire);
    NODE_SET_PROTOTYPE_METHOD(t, "findSerialPortChannel", SdpSearch);
    target->Set(String::NewSymbol("DeviceINQ"), t->GetFunction());
    target->Set(String::NewSymbol("DeviceINQ"), t->GetFunction());
}
    
DeviceINQ::DeviceINQ() {
        
}
    
DeviceINQ::~DeviceINQ() {
        
}
    
Handle<Value> DeviceINQ::New(const Arguments& args) {
    HandleScope scope;

    const char *usage = "usage: DeviceINQ()";
    if (args.Length() != 0) {
        return scope.Close(ThrowException(Exception::Error(String::New(usage))));
    }

    DeviceINQ* inquire = new DeviceINQ();
    inquire->Wrap(args.This());

    return args.This();
}
 
Handle<Value> DeviceINQ::Inquire(const Arguments& args) {
    HandleScope scope;

    const char *usage = "usage: inquire()";
    if (args.Length() != 0) {
        return scope.Close(ThrowException(Exception::Error(String::New(usage))));
    }
    
    // do the bluetooth magic
    inquiry_info *ii = NULL;
    int max_rsp, num_rsp;
    int dev_id, sock, len, flags;
    int i;
    char addr[19] = { 0 };
    char name[248] = { 0 };

    dev_id = hci_get_route(NULL);
    sock = hci_open_dev( dev_id );
    if (dev_id < 0 || sock < 0) {
        return scope.Close(ThrowException(Exception::Error(String::New("opening socket"))));
    }

    len  = 8;
    max_rsp = 255;
    flags = IREQ_CACHE_FLUSH;
    ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));

    num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
    // if( num_rsp < 0 ) {
    //     return ThrowException(Exception::Error(String::New("hci inquiry")));
    // }

    for (i = 0; i < num_rsp; i++) {
      ba2str(&(ii+i)->bdaddr, addr);
      memset(name, 0, sizeof(name));
      if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name), 
          name, 0) < 0)
        strcpy(name, addr);

      // fprintf(stderr, "%s [%s]\n", addr, name);

      Local<Value> argv[3] = {
          String::New("found"),
          String::New(addr),
          String::New(name)
      };

      MakeCallback(args.This(), "emit", 3, argv);
    }

    free( ii );
    close( sock );

    Local<Value> argv[1] = {
        String::New("finished")
    };

    MakeCallback(args.This(), "emit", 1, argv);

    return Undefined();
}
    
Handle<Value> DeviceINQ::SdpSearch(const Arguments& args) {
    HandleScope scope;
    
    const char *usage = "usage: findSerialPortChannel(address, callback)";
    if (args.Length() != 2) {
        return scope.Close(ThrowException(Exception::Error(String::New(usage))));
    }
    
    if (!args[0]->IsString()) {
        return scope.Close(ThrowException(Exception::TypeError(String::New("First argument should be a string value"))));
    }
    String::Utf8Value address(args[0]);

    if(!args[1]->IsFunction()) {
        return scope.Close(ThrowException(Exception::TypeError(String::New("Second argument must be a function"))));
    }
    Local<Function> cb = Local<Function>::Cast(args[1]);
    
    DeviceINQ* inquire = ObjectWrap::Unwrap<DeviceINQ>(args.This());
    
    sdp_baton_t *baton = new sdp_baton_t();
    baton->inquire = inquire;
    baton->cb = Persistent<Function>::New(cb);
    strcpy(baton->address, *address);
    baton->channelID = -1;
    baton->request.data = baton;
    baton->inquire->Ref();

    uv_queue_work(uv_default_loop(), &baton->request, EIO_SdpSearch, (uv_after_work_cb)EIO_AfterSdpSearch);

    return Undefined();
}
