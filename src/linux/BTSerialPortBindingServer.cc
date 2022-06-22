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
#include <memory>
#include <iostream>
#include <map>
#include "BTSerialPortBindingServer.h"

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

#define CLIENT_CLOSED_CONNECTION "Connection closed by the client"

static const uint16_t _SPP_UUID = 0x1101; // Serial Port Profile UUID

using namespace std;
using namespace node;
using namespace v8;

// BDADDR_ANY is defined as (&(bdaddr_t) {{0, 0, 0, 0, 0, 0}}) and
// BDADDR_LOCAL is defined as (&(bdaddr_t) {{0, 0, 0, 0xff, 0xff, 0xff}}) which
// is the address of temporary thus not allowed in C++
static const bdaddr_t _BDADDR_ANY = {0, 0, 0, 0, 0, 0};
static const bdaddr_t _BDADDR_LOCAL = {0, 0, 0, 0xff, 0xff, 0xff};

static int str2uuid(const char *uuid_str, uuid_t *uuid)
{
    uint32_t uuid_int[4];
    char *endptr;

    if(strlen(uuid_str) == 36) {
        // Parse uuid128 standard format: 12345678-9012-3456-7890-123456789012
        char buf[9] = { 0 };

        if(uuid_str[8] != '-' && uuid_str[13] != '-' &&
            uuid_str[18] != '-'  && uuid_str[23] != '-') {
            return 0;
        }
        // first 8-bytes
        strncpy(buf, uuid_str, 8);
        uuid_int[0] = htonl(strtoul(buf, &endptr, 16));
        if(endptr != buf + 8)
            return 0;

        // second 8-bytes
        strncpy(buf, uuid_str+9, 4);
        strncpy(buf+4, uuid_str+14, 4);
        uuid_int[1] = htonl(strtoul(buf, &endptr, 16));
        if(endptr != buf + 8)
            return 0;

        // third 8-bytes
        strncpy(buf, uuid_str+19, 4);
        strncpy(buf+4, uuid_str+24, 4);
        uuid_int[2] = htonl(strtoul(buf, &endptr, 16));
        if(endptr != buf + 8)
            return 0;

        // fourth 8-bytes
        strncpy(buf, uuid_str+28, 8);
        uuid_int[3] = htonl(strtoul(buf, &endptr, 16));
        if(endptr != buf + 8)
            return 0;

        if(uuid != NULL)
            sdp_uuid128_create(uuid, uuid_int);
    } else if (strlen(uuid_str) == 8) {
        // 32-bit reserved UUID
        uint32_t i = strtoul(uuid_str, &endptr, 16);
        if(endptr != uuid_str + 8)
            return 0;
        if(uuid != NULL)
            sdp_uuid32_create(uuid, i);
    } else if(strlen(uuid_str) == 4) {
        // 16-bit reserved UUID
        int i = strtol(uuid_str, &endptr, 16);
        if(endptr != uuid_str + 4)
            return 0;
        if(uuid != NULL)
            sdp_uuid16_create(uuid, i);
    } else {
        return 0;
    }

    return 1;
}


void BTSerialPortBindingServer::EIO_Listen(uv_work_t *req) {
    listen_baton_t * baton = static_cast<listen_baton_t *>(req->data);

    struct sockaddr_rc addr = {
        0x00,
        { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
        0x00
    };

    // allocate a socket
    baton->rfcomm->s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

    // set the connection parameters (who to connect to)
    addr.rc_family = AF_BLUETOOTH;
    bacpy(&addr.rc_bdaddr, &_BDADDR_ANY);
    addr.rc_channel = (uint8_t) baton->listeningChannelID;

    baton->status = bind(baton->rfcomm->s, (struct sockaddr *)&addr, sizeof(addr));
    if(baton->status){
         sprintf(baton->errorString, "Couldn't bind bluetooth socket. errno:%d", errno);
         return;
    }

    baton->status = listen(baton->rfcomm->s, 1); // Bluetooth only accepts one connection at a time
    if(baton->status){
        sprintf(baton->errorString, "Couldn't listen on bluetooth socket. errno:%d", errno);
        return;
    }

}

void BTSerialPortBindingServer::EIO_AfterListen(uv_work_t *req) {
    Nan::HandleScope scope;

    listen_baton_t * baton = static_cast<listen_baton_t *>(req->data);

    Nan::TryCatch try_catch;

    if (baton->status != 0) {
        Local<Value> argv[] = {
            Nan::Error(baton->errorString)
        };

        Nan::AsyncResource resource("bluetooth-serial-port:server.Listen");
        baton->ecb->Call(1, argv, &resource);
        return;
    }

    if (try_catch.HasCaught()) {
        Nan::FatalException(try_catch);
    }

    baton->rfcomm->AdvertiseAndAccept();
}

void BTSerialPortBindingServer::AdvertiseAndAccept() {
    // Register the service via SDP daemon
    this->Advertise();

    // Accept an incoming connection asynchronously
    auto callback = new Nan::Callback(this->mListenBaton->cb->GetFunction());
    AsyncQueueWorker(new ClientWorker(callback, this->mListenBaton));
}

void BTSerialPortBindingServer::EIO_Write(uv_work_t *req) {
    queued_write_t *queuedWrite = static_cast<queued_write_t*>(req->data);
    write_baton_t *data = static_cast<write_baton_t*>(queuedWrite->baton);

    BTSerialPortBindingServer* rfcomm = data->rfcomm;

    if (!rfcomm->mClientSocket || rfcomm->mClientSocket == -1) {
        sprintf(data->errorString, "Attempting to write to a closed connection");
    }

    data->result = ::write(rfcomm->mClientSocket, data->bufferData, data->bufferLength);

    if (data->result != data->bufferLength) {
        sprintf(data->errorString, "Writing attempt was unsuccessful");
    }
}

void BTSerialPortBindingServer::EIO_AfterWrite(uv_work_t *req) {
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


    Nan::AsyncResource resource("bluetooth-serial-port:server.Write");
    data->callback->Call(2, argv, &resource);

    uv_mutex_lock(&data->rfcomm->mWriteQueueMutex);
    ngx_queue_remove(&queuedWrite->queue);

    if (!ngx_queue_empty(&data->rfcomm->mWriteQueue)) {
        ngx_queue_t* head = ngx_queue_head(&data->rfcomm->mWriteQueue);
        queued_write_t* nextQueuedWrite = ngx_queue_data(head, queued_write_t, queue);
        uv_queue_work(uv_default_loop(), &nextQueuedWrite->req, EIO_Write, (uv_after_work_cb)EIO_AfterWrite);
    }
    uv_mutex_unlock(&data->rfcomm->mWriteQueueMutex);

    data->buffer.Reset();
    delete data->callback;
    data->rfcomm->Unref();
    delete data;
    delete queuedWrite;
}

void BTSerialPortBindingServer::EIO_Read(uv_work_t *req) {
    unsigned char buf[1024]= { 0 };

    read_baton_t *baton = static_cast<read_baton_t *>(req->data);

    memset(buf, 0, sizeof(buf));

    fd_set set;
    FD_ZERO(&set);
    FD_SET(baton->rfcomm->mClientSocket, &set);
    FD_SET(baton->rfcomm->rep[0], &set);

    int nfds = (baton->rfcomm->mClientSocket > baton->rfcomm->rep[0]) ? baton->rfcomm->mClientSocket : baton->rfcomm->rep[0];

    if (pselect(nfds + 1, &set, NULL, NULL, NULL, NULL) >= 0) {
        if (FD_ISSET(baton->rfcomm->mClientSocket, &set)) {
            baton->size = ::read(baton->rfcomm->mClientSocket, buf, sizeof(buf));
            if(baton->size < 0){
                baton->errorno = errno;
            }else if (baton->size > 0) {
                memcpy(baton->result, buf, baton->size);
            }
        }else if(FD_ISSET(baton->rfcomm->rep[0], &set)) {
          int size = ::read(baton->rfcomm->rep[0], buf, sizeof(buf));
          if(size<0){
            baton->errorno = errno;
          }
          buf[size] = '\0';
          std::string strBuffer(reinterpret_cast<const char *>(buf));
          if(strBuffer == "close") {
            baton->isClose = true;
            baton->size = 0;
          } else if (strBuffer == "disconnect") {
            baton->isDisconnect = true;
          }
        }else{
          baton->size = 0;
        }
    }
}

void BTSerialPortBindingServer::EIO_AfterRead(uv_work_t *req) {
    Nan::HandleScope scope;

    read_baton_t *baton = static_cast<read_baton_t *>(req->data);

    Nan::TryCatch try_catch;

    Local<Value> argv[2];

    Nan::AsyncResource resource("bluetooth-serial-port:server.Read");
    if (baton->size <= 0) {
        char msg[512];
        sprintf(msg, "Error reading from connection: errno: %d", baton->errorno);
        argv[0] = Nan::Error(msg);
        argv[1] = Nan::Undefined();
        if(baton->errorno == ECONNRESET || baton->errorno == ETIMEDOUT || baton->size == 0 || baton->isDisconnect){
            baton->rfcomm->CloseClientSocket();
            if (!baton->isClose) {
                baton->rfcomm->AdvertiseAndAccept();
            }
            argv[0] = Nan::Error(CLIENT_CLOSED_CONNECTION);
            baton->cb->Call(2, argv, &resource);
        }
        return;
    }

    Local<Object> resultBuffer = Nan::NewBuffer(baton->size).ToLocalChecked();
    memcpy(Buffer::Data(resultBuffer), baton->result, baton->size);

    argv[0] = Nan::Undefined();
    argv[1] = resultBuffer;

    baton->cb->Call(2, argv, &resource);

    if (try_catch.HasCaught()) {
        Nan::FatalException(try_catch);
    }

    baton->rfcomm->Unref();
    delete baton->cb;
    delete baton;
}

void BTSerialPortBindingServer::Init(Local<Object> target) {
    Nan::HandleScope scope;

    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(Nan::New("BTSerialPortBindingServer").ToLocalChecked());

    Isolate *isolate = target->GetIsolate();
    Local<Context> ctx = isolate->GetCurrentContext();

    Nan::SetPrototypeMethod(t, "write", Write);
    Nan::SetPrototypeMethod(t, "read", Read);
    Nan::SetPrototypeMethod(t, "close", Close);
    Nan::SetPrototypeMethod(t, "disconnectClient", DisconnectClient);
    Nan::SetPrototypeMethod(t, "isOpen", IsOpen);

    Nan::Set(target, Nan::New("BTSerialPortBindingServer").ToLocalChecked(), t->GetFunction(ctx).ToLocalChecked());
}

BTSerialPortBindingServer::BTSerialPortBindingServer() :
    s(0) {
    mListenBaton = new listen_baton_t();
}

BTSerialPortBindingServer::~BTSerialPortBindingServer() {
    if (mListenBaton->ecb) { mListenBaton->ecb->Reset(); }
    if (mListenBaton->cb) { mListenBaton->cb->Reset(); }
    delete mListenBaton;
}

NAN_METHOD(BTSerialPortBindingServer::New) {


    if(info.Length() != 3){
        return Nan::ThrowError("usage: BTSerialPortBindingServer(successCallback, errorCallback, options)");
    }

    // callback
    if(!info[0]->IsFunction()) {
        return Nan::ThrowTypeError("First argument must be a function");
    }


    // callback
    if(!info[1]->IsFunction()) {
        return Nan::ThrowTypeError("Second argument must be a function");
    }

    // Object {}
    if(!info[2]->IsObject()) {
        return Nan::ThrowTypeError("Third argument must be an object with this properties: uuid, channel");
    }


    BTSerialPortBindingServer *rfcomm = new BTSerialPortBindingServer();
    uv_mutex_init(&rfcomm->mWriteQueueMutex);
    ngx_queue_init(&rfcomm->mWriteQueue);
    listen_baton_t * baton = rfcomm->mListenBaton;
    rfcomm->Wrap(info.This());

    Local<Object> jsOptions = Local<Object>::Cast(info[2]);
    Isolate *isolate = jsOptions->GetIsolate();
    Local<Context> ctx = isolate->GetCurrentContext();

    Local<Array> properties = jsOptions->GetPropertyNames(ctx).ToLocalChecked();
    int n = properties->Length();
    std::map<std::string, std::string> options;

    for (int i = 0; i < n ; i++) {
        Local<Value>  property = Nan::Get(properties, Nan::New<Integer>(i)).ToLocalChecked();
        string propertyName = std::string(*String::Utf8Value(isolate, property));
        Local<Value> optionValue = Nan::Get(jsOptions, property).ToLocalChecked();
        options[propertyName] = std::string(*String::Utf8Value(isolate, optionValue));
    }

    if(!str2uuid(options["uuid"].c_str(), &baton->uuid)){
        return Nan::ThrowError("The UUID is invalid");
    }

    baton->rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBindingServer>(info.This());

    // allocate an error pipe
    if (pipe(baton->rfcomm->rep) == -1) {
        return Nan::ThrowError("Cannot create pipe for reading.");
    }

    int flags = fcntl(baton->rfcomm->rep[0], F_GETFL, 0);
    fcntl(baton->rfcomm->rep[0], F_SETFL, flags | O_NONBLOCK);

    baton->cb = new Nan::Callback(info[0].As<Function>());
    baton->ecb = new Nan::Callback(info[1].As<Function>());
    baton->listeningChannelID = std::stoi(options["channel"]);
    baton->request.data = baton;
    baton->rfcomm->Ref();

    uv_queue_work(uv_default_loop(), &baton->request, EIO_Listen, (uv_after_work_cb)EIO_AfterListen);

    info.GetReturnValue().Set(info.This());
}


void BTSerialPortBindingServer::Advertise() {

    listen_baton_t * baton = mListenBaton;
    uint8_t rfcomm_channel = (uint8_t) baton->listeningChannelID;

    std::string service_name = "RFCOMM custom service";
    const char *service_dsc = "An RFCOMM listening socket";

    uuid_t root_uuid, l2cap_uuid, rfcomm_uuid;
    sdp_list_t *l2cap_list = 0,
               *rfcomm_list = 0,
               *root_list = 0,
               *proto_list = 0,
               *service_class_list = 0,
               *access_proto_list = 0;
    sdp_data_t *channel = 0;

    sdp_record_t *record = sdp_record_alloc();

    sdp_set_service_id(record, baton->uuid);

    service_class_list = sdp_list_append(0, &baton->uuid);
    sdp_set_service_classes(record, service_class_list);

    sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
    root_list = sdp_list_append(0, &root_uuid);
    sdp_set_browse_groups(record, root_list);

    sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
    l2cap_list = sdp_list_append(0, &l2cap_uuid);
    proto_list = sdp_list_append(0, l2cap_list);

    sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
    channel = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
    rfcomm_list = sdp_list_append(0, &rfcomm_uuid);
    sdp_list_append(rfcomm_list, channel);
    sdp_list_append(proto_list, rfcomm_list);

    // Attach protocol information to service record
    access_proto_list = sdp_list_append(0, proto_list);
    sdp_set_access_protos(record, access_proto_list);

    // Set the service name. If the UUID of the service
    // is the one from Serial Port profile
    if(baton->uuid.value.uuid16 == _SPP_UUID){
        service_name = "Serial Port";
    }

    sdp_set_info_attr(record, service_name.c_str(), NULL, service_dsc);

    // Connect to the local SDP server
    mSdpSession = sdp_connect(&_BDADDR_ANY, &_BDADDR_LOCAL, SDP_RETRY_IF_BUSY);
    if(mSdpSession == NULL){
        baton->status = -1;
        sprintf(baton->errorString, "Cannot connect to SDP Daemon. errno: %d", errno);
        goto cleanup;
    }

    // Register the service record.
    if(sdp_record_register(mSdpSession, record, 0) == -1){
        baton->status = -1;
        sprintf(baton->errorString, "Cannot register SDP record. errno: %d", errno);
        goto cleanup;
    }

cleanup:
    // cleanup
    sdp_data_free(channel);
    sdp_list_free(l2cap_list, 0);
    sdp_list_free(rfcomm_list, 0);
    sdp_list_free(service_class_list, 0);
    sdp_list_free(root_list, 0);
    sdp_list_free(access_proto_list, 0);
    sdp_record_free(record);
}

void BTSerialPortBindingServer::CloseClientSocket() {
    // close the socket to the client
    if (mClientSocket != 0) {
        shutdown(mClientSocket, SHUT_RDWR);
        close(mClientSocket);
        mClientSocket = 0;
    }
}

NAN_METHOD(BTSerialPortBindingServer::Write) {
    // usage
    if (info.Length() != 2) {
        return Nan::ThrowError("usage: write(buf, callback)");
    }

    // buffer
    if(!info[0]->IsObject() || !Buffer::HasInstance(info[0])) {
        return Nan::ThrowTypeError("First argument must be a buffer");
    }

    Local<Object> bufferObject = info[0].As<Object>();
    char* bufferData = Buffer::Data(bufferObject);
    size_t bufferLength = Buffer::Length(bufferObject);

    // callback
    if(!info[1]->IsFunction()) {
        return Nan::ThrowTypeError("Second argument must be a function");
    }

    write_baton_t *baton = new write_baton_t();
    memset(baton, 0, sizeof(write_baton_t));
    baton->rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBindingServer>(info.This());
    baton->rfcomm->Ref();
    baton->buffer.Reset(bufferObject);
    baton->bufferData = bufferData;
    baton->bufferLength = bufferLength;
    baton->callback = new Nan::Callback(info[1].As<Function>());

    queued_write_t *queuedWrite = new queued_write_t();
    memset(queuedWrite, 0, sizeof(queued_write_t));
    queuedWrite->baton = baton;
    queuedWrite->req.data = queuedWrite;

    uv_mutex_lock(&baton->rfcomm->mWriteQueueMutex);
    bool empty = ngx_queue_empty(&baton->rfcomm->mWriteQueue);

    ngx_queue_insert_tail(&baton->rfcomm->mWriteQueue, &queuedWrite->queue);

    if (empty) {
        uv_queue_work(uv_default_loop(), &queuedWrite->req, EIO_Write, (uv_after_work_cb)EIO_AfterWrite);
    }
    uv_mutex_unlock(&baton->rfcomm->mWriteQueueMutex);

    return;
}

NAN_METHOD(BTSerialPortBindingServer::Close) {
    BTSerialPortBindingServer* rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBindingServer>(info.This());

    // closing pipes
    if(rfcomm->rep[1]){
        int len = ::write(rfcomm->rep[1], "close", (strlen("close")+1));
        if(len < 0 && errno != EWOULDBLOCK) {
            return Nan::ThrowError("Cannot write to pipe!");
        }
    }
    if(rfcomm->rep[0] != 0) close(rfcomm->rep[0]);
    if(rfcomm->rep[1] != 0) close(rfcomm->rep[1]);

    rfcomm->rep[0] = rfcomm->rep[1] = 0;

    // Close the connection with the SDP server
    if (rfcomm->mSdpSession){
        sdp_close(rfcomm->mSdpSession);
        rfcomm->mSdpSession = nullptr;
    }

    // close client socket
    rfcomm->CloseClientSocket();

    // Close server socket
    if (rfcomm->s != 0) {
        close(rfcomm->s);
        rfcomm->s = 0; // Inform `ClientWorker` to stop `pselect` loop to accept connections if ongoing.
    }

    // Call unref so we can be garbage collected (rest of cleanup is in the destructor)
    rfcomm->Unref();

    return;
}

NAN_METHOD(BTSerialPortBindingServer::Read) {
    const char *usage = "usage: read(callback)";
    if (info.Length() != 1) {
        return Nan::ThrowError(usage);
    }

    Local<Function> cb = info[0].As<Function>();

    BTSerialPortBindingServer* rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBindingServer>(info.This());

    // callback with an error if the connection has been closed.
    if (rfcomm->mClientSocket == 0) {
        Local<Value> argv[2];

        argv[0] = Nan::Error(CLIENT_CLOSED_CONNECTION);
        argv[1] = Nan::Undefined();

        Nan::AsyncResource resource("bluetooth-serial-port:server.Read");
        std::unique_ptr<Nan::Callback> nc(new Nan::Callback(cb));
        nc->Call(2, argv, &resource);
        return;
    }
    read_baton_t *baton = new read_baton_t();
    baton->rfcomm = rfcomm;
    baton->cb = new Nan::Callback(cb);
    baton->request.data = baton;
    baton->rfcomm->Ref();
    uv_queue_work(uv_default_loop(), &baton->request, EIO_Read, (uv_after_work_cb)EIO_AfterRead);
}

NAN_METHOD(BTSerialPortBindingServer::DisconnectClient) {
    BTSerialPortBindingServer* rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBindingServer>(info.This());

    // send disconnect to internal pipe to stop blocking read operation and to disconnect the client socket
    // if we try to disconnect the client socket here, we might get a read error on the client socket
    // before reading the disconnect message from the pipe
    if(rfcomm->rep[1]){
        int len = ::write(rfcomm->rep[1], "disconnect", (strlen("disconnect")+1));
        if(len < 0 && errno != EWOULDBLOCK) {
            return Nan::ThrowError("Cannot write to pipe!");
        }
    }
}

NAN_METHOD(BTSerialPortBindingServer::IsOpen) {
    BTSerialPortBindingServer* rfcomm = Nan::ObjectWrap::Unwrap<BTSerialPortBindingServer>(info.This());
    bool b = (rfcomm->mClientSocket != 0);
    info.GetReturnValue().Set(b);
}

BTSerialPortBindingServer::ClientWorker::ClientWorker(Nan::Callback * cb, listen_baton_t * baton) :
    AsyncWorker(cb),
    mBaton(baton)
{
}

BTSerialPortBindingServer::ClientWorker::~ClientWorker()
{}


void BTSerialPortBindingServer::ClientWorker::Execute(){
    if(mBaton == nullptr){
        return Nan::ThrowError("listen_baton_t is null!");
    }

    struct sockaddr_rc clientAddress = {
        0x00,
        { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
        0x00
    };
    socklen_t clientAddrLen = sizeof(clientAddress);

    int select_status;
    while (mBaton->rfcomm->s != 0) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(mBaton->rfcomm->s, &fds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 333;

        select_status = select(mBaton->rfcomm->s + 1, &fds, NULL, NULL, &timeout);
        if (select_status > 0) {
            break;
        }
        else if (select_status == -1) {
            return Nan::ThrowError("error pselect'ing socket!");
        }
    }

    if (mBaton->rfcomm->s == 0) {
        // In middle of closing
        return;
    }

    mBaton->rfcomm->mClientSocket = accept(mBaton->rfcomm->s, (struct sockaddr *)&clientAddress, &clientAddrLen);

    ba2str(&clientAddress.rc_bdaddr, mBaton->clientAddress);
}

void BTSerialPortBindingServer::ClientWorker::HandleOKCallback(){
    Nan::HandleScope scope;

    if(mBaton->rfcomm->mSdpSession){
        // Close the connection with the SDP server so it stops advertising the service
        sdp_close(mBaton->rfcomm->mSdpSession);
        mBaton->rfcomm->mSdpSession = nullptr;
    }

    if(mBaton->rfcomm->mClientSocket == -1){
        mBaton->status = -1;
        sprintf(mBaton->errorString, "accept() failed!. errno: %d", errno);
        return;
    }

    Local<Value> argv[] = {
        Nan::New<v8::String>((mBaton->clientAddress)).ToLocalChecked()
    };

    Nan::AsyncResource resource("bluetooth-serial-port:server.HandleOK");
    callback->Call(1, argv, &resource);

}
