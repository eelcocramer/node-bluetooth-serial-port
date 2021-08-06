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
    Nan::HandleScope scope;

    sdp_baton_t *baton = static_cast<sdp_baton_t *>(req->data);

    Nan::TryCatch try_catch;

    Local<Value> argv[] = {
        Nan::New(baton->channelID)
    };

    Nan::AsyncResource resource("bluetooth-serial-port:SdpSearch");
    baton->cb->Call(1, argv, &resource);

    if (try_catch.HasCaught()) {
        Nan::FatalException(try_catch);
    }

    baton->inquire->Unref();
    delete baton->cb;
    delete baton;
    baton = NULL;
}

void DeviceINQ::Init(Local<Object> target) {
    Nan::HandleScope scope;

    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(Nan::New("DeviceINQ").ToLocalChecked());

    Isolate *isolate = target->GetIsolate();
    Local<Context> ctx = isolate->GetCurrentContext();

    Nan::SetPrototypeMethod(t, "inquireSync", InquireSync);
    Nan::SetPrototypeMethod(t, "inquire", Inquire);
    Nan::SetPrototypeMethod(t, "findSerialPortChannel", SdpSearch);
    Nan::SetPrototypeMethod(t, "listPairedDevices", ListPairedDevices);
    target->Set(ctx, Nan::New("DeviceINQ").ToLocalChecked(), t->GetFunction(ctx).ToLocalChecked());
}

bt_inquiry DeviceINQ::doInquire() {

  // do the bluetooth magic
  inquiry_info *ii = NULL;
  int max_rsp, num_rsp;
  int dev_id, sock, len, flags;
  int i;
  char addr[19] = { 0 };
  char name[248] = { 0 };

  bt_inquiry inquiryResult;

  dev_id = hci_get_route(NULL);
  sock = hci_open_dev( dev_id );
  if (dev_id < 0 || sock < 0) {
    Nan::ThrowError("opening socket");
  }

  len  = 8;
  max_rsp = 255;
  flags = IREQ_CACHE_FLUSH;
  ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));

  num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
  // if( num_rsp < 0 ) {
  //     return ThrowException(Exception::Error(String::New("hci inquiry")));
  // }
  inquiryResult.num_rsp = num_rsp;
  inquiryResult.devices = (bt_device*)malloc(num_rsp * sizeof(bt_device));

  for (i = 0; i < num_rsp; i++) {
    ba2str(&(ii+i)->bdaddr, addr);
    memset(name, 0, sizeof(name));
    if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name),
        name, 0) < 0)
      strcpy(name, addr);

    //fprintf(stderr, "%s [%s]\n", addr, name);
    strcpy(inquiryResult.devices[i].address, addr);
    strcpy(inquiryResult.devices[i].name, name);
  }

  free( ii );
  close( sock );
  return inquiryResult;
}

DeviceINQ::DeviceINQ() {

}

DeviceINQ::~DeviceINQ() {

}

NAN_METHOD(DeviceINQ::New) {
    const char *usage = "usage: DeviceINQ()";
    if (info.Length() != 0) {
        return Nan::ThrowError(usage);
    }

    DeviceINQ* inquire = new DeviceINQ();
    inquire->Wrap(info.This());

    info.GetReturnValue().Set(info.This());
}

NAN_METHOD(DeviceINQ::InquireSync) {
    const char *usage = "usage: inquireSync(found, callback)";
    if (info.Length() != 2) {
        return Nan::ThrowError(usage);
    }

    Nan::AsyncResource resource("bluetooth-serial-port:InquireSync");
    Nan::Callback *found = new Nan::Callback(info[0].As<Function>());
    Nan::Callback *callback = new Nan::Callback(info[1].As<Function>());

    bt_inquiry inquiryResult = DeviceINQ::doInquire();
    for (int i = 0; i < inquiryResult.num_rsp; i++) {
      Local<Value> argv[] = {
        Nan::New(inquiryResult.devices[i].address).ToLocalChecked(),
        Nan::New(inquiryResult.devices[i].name).ToLocalChecked()
      };
      found->Call(2, argv, &resource);
    }

    Local<Value> argv[] = {};
    callback->Call(0, argv, &resource);
    return;
}

class InquireWorker : public Nan::AsyncWorker {
 public:
  InquireWorker(Nan::Callback* found, Nan::Callback *callback)
    : Nan::AsyncWorker(callback), found(found) {}
  ~InquireWorker() {}

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
    inquiryResult = DeviceINQ::doInquire();
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback () {
    Nan::HandleScope scope;

    Nan::AsyncResource resource("bluetooth-serial-port:Inquire");
    for (int i = 0; i < inquiryResult.num_rsp; i++) {
      Local<Value> argv[] = {
        Nan::New(inquiryResult.devices[i].address).ToLocalChecked(),
        Nan::New(inquiryResult.devices[i].name).ToLocalChecked()
      };
      found->Call(2, argv, &resource);
    }

    Local<Value> argv[] = {};
    callback->Call(0, argv, &resource);
  }

  private:
    bt_inquiry inquiryResult;
    Nan::Callback* found;
};

// Asynchronous access to the `Inquire()` function
NAN_METHOD(DeviceINQ::Inquire) {
  const char *usage = "usage: inquire(found, callback)";
  if (info.Length() != 2) {
      return Nan::ThrowError(usage);
  }
  Nan::Callback *found = new Nan::Callback(info[0].As<Function>());
  Nan::Callback *callback = new Nan::Callback(info[1].As<Function>());

  Nan::AsyncQueueWorker(new InquireWorker(found, callback));
}

NAN_METHOD(DeviceINQ::SdpSearch) {
    const char *usage = "usage: findSerialPortChannel(address, callback)";
    if (info.Length() != 2) {
        return Nan::ThrowError(usage);
    }

    if (!info[0]->IsString()) {
        return Nan::ThrowTypeError("First argument should be a string value");
    }
    String::Utf8Value address(info.GetIsolate(), info[0]);

    if(!info[1]->IsFunction()) {
        return Nan::ThrowTypeError("Second argument must be a function");
    }

    Local<Function> cb = info[1].As<Function>();

    DeviceINQ* inquire = Nan::ObjectWrap::Unwrap<DeviceINQ>(info.This());

    sdp_baton_t *baton = new sdp_baton_t();
    baton->inquire = inquire;
    baton->cb = new Nan::Callback(cb);
    strcpy(baton->address, *address);
    baton->channelID = -1;
    baton->request.data = baton;
    baton->inquire->Ref();

    uv_queue_work(uv_default_loop(), &baton->request, EIO_SdpSearch, (uv_after_work_cb)EIO_AfterSdpSearch);

    return;
}

NAN_METHOD(DeviceINQ::ListPairedDevices) {
    const char *usage = "usage: listPairedDevices(callback)";
    if (info.Length() != 1) {
        return Nan::ThrowError(usage);
    }

    if(!info[0]->IsFunction()) {
       return Nan::ThrowTypeError("First argument must be a function");
    }
    Local<Function> cb = info[0].As<Function>();

    Local<Array> resultArray = Local<Array>(Nan::New<Array>());

    // TODO: build an array of objects representing a paired device:
    // ex: {
    //   name: 'MyBluetoothDeviceName',
    //   address: '12-34-56-78-90',
    //   services: [
    //     { name: 'SPP', channel: 1 },
    //     { name: 'iAP', channel: 2 }
    //   ]
    // }

    Local<Value> argv[1] = {
        resultArray
    };
    cb->Call(Nan::GetCurrentContext(), Nan::GetCurrentContext()->Global(), 1, argv);

    return;
}
