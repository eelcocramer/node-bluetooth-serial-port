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
#include <string.h>
#include <stdlib.h>
#include <node_object_wrap.h>
#include "../DeviceINQ.h"
#include "BluetoothHelpers.h"

using namespace std;
using namespace node;
using namespace v8;

void DeviceINQ::EIO_SdpSearch(uv_work_t *req) {
    sdp_baton_t *baton = static_cast<sdp_baton_t *>(req->data);

    // Default, no channel is found
    baton->channelID = -1;

    // Construct windows socket bluetooth variables
    DWORD flags = LUP_FLUSHCACHE | LUP_RETURN_ADDR;
    DWORD querySetSize = sizeof(WSAQUERYSET);
    WSAQUERYSET *querySet = (WSAQUERYSET *)malloc(querySetSize);
    if (querySet == nullptr) {
        return;
    }

    ZeroMemory(querySet, querySetSize);
    querySet->dwSize = querySetSize;
    querySet->dwNameSpace = NS_BTH;
    querySet->lpServiceClassId = (LPGUID)&SerialPortServiceClass_UUID;
    querySet->dwNumberOfCsAddrs = 0;
    querySet->lpszContext = baton->address;

    // Initiate client device inquiry
    HANDLE lookupServiceHandle;
    int lookupServiceError = WSALookupServiceBegin(querySet, flags, &lookupServiceHandle);
    if (lookupServiceError != SOCKET_ERROR) {
        // Iterate over each found bluetooth service
        bool inquiryComplete = false;
        while (!inquiryComplete) {
            // For each bluetooth service retrieve its corresponding details
            lookupServiceError = WSALookupServiceNext(lookupServiceHandle, flags, &querySetSize, querySet);
            if (lookupServiceError != SOCKET_ERROR) {
                char address[19] = { 0 };
                SOCKADDR_BTH *bluetoothSocketAddress = (SOCKADDR_BTH *)querySet->lpcsaBuffer->RemoteAddr.lpSockaddr;
                baton->channelID = bluetoothSocketAddress->port;
                inquiryComplete = true;
            } else {
                int lookupServiceErrorNumber = WSAGetLastError();
                if (lookupServiceErrorNumber == WSAEFAULT) {
                    free(querySet);
                    querySet = (WSAQUERYSET *)malloc(querySetSize);
                    if (querySet == nullptr) {
                        WSALookupServiceEnd(lookupServiceHandle);
                        return;
                    }
                } else if (lookupServiceErrorNumber == WSA_E_NO_MORE) {
                    // No more services where found
                    inquiryComplete = true;
                } else {
                    // Unhandled error
                    inquiryComplete = true;
                }
            }
        }
    } else {
        int lookupServiceErrorNumber = WSAGetLastError();
        if (lookupServiceErrorNumber != WSASERVICE_NOT_FOUND) {
            free(querySet);
            return;
        }
    }

    free(querySet);
    WSALookupServiceEnd(lookupServiceHandle);
}

void DeviceINQ::EIO_AfterSdpSearch(uv_work_t *req) {
    sdp_baton_t *baton = static_cast<sdp_baton_t *>(req->data);

    TryCatch try_catch;

    Handle<Value> argv[] = {
        NanNew(baton->channelID)
    };
    baton->cb->Call(1, argv);

    if (try_catch.HasCaught()) {
        FatalException(try_catch);
    }

    baton->inquire->Unref();
    delete baton->cb;
    delete baton;
    baton = nullptr;
}

void DeviceINQ::Init(Handle<Object> target) {
    NanScope();

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(NanNew("DeviceINQ"));

    NODE_SET_PROTOTYPE_METHOD(t, "inquire", Inquire);
    NODE_SET_PROTOTYPE_METHOD(t, "findSerialPortChannel", SdpSearch);
    NODE_SET_PROTOTYPE_METHOD(t, "listPairedDevices", ListPairedDevices);
    target->Set(NanNew("DeviceINQ"), t->GetFunction());
    target->Set(NanNew("DeviceINQ"), t->GetFunction());
    target->Set(NanNew("DeviceINQ"), t->GetFunction());
}

DeviceINQ::DeviceINQ() {
    initialized = BluetoothHelpers::Initialize();
}

DeviceINQ::~DeviceINQ() {
    if (initialized) {
        BluetoothHelpers::Finalize();
    }
}

NAN_METHOD(DeviceINQ::New) {
    NanScope();

    if (args.Length() != 0) {
        NanThrowError("usage: DeviceINQ()");
    }

    DeviceINQ *inquire = new DeviceINQ();
    if (!inquire->Initialized) {
        NanThrowError("Unable to initialize socket library");
    }

    inquire->Wrap(args.This());
    NanReturnValue(args.This());
}

NAN_METHOD(DeviceINQ::Inquire) {
    NanScope();

    if (args.Length() != 0) {
        NanThrowError("usage: inquire()");
    }

    // Construct windows socket bluetooth variables
    DWORD flags = LUP_CONTAINERS | LUP_FLUSHCACHE | LUP_RETURN_NAME | LUP_RETURN_ADDR;
    DWORD querySetSize = sizeof(WSAQUERYSET);
    WSAQUERYSET *querySet = (WSAQUERYSET *)malloc(querySetSize);
    if (querySet == nullptr) {
        NanThrowError("Out of memory: Unable to allocate memory resource for inquiry");
    }

    ZeroMemory(querySet, querySetSize);
    querySet->dwSize = querySetSize;
    querySet->dwNameSpace = NS_BTH;

    // Initiate client device inquiry
    HANDLE lookupServiceHandle;
    int lookupServiceError = WSALookupServiceBegin(querySet, flags, &lookupServiceHandle);
    if (lookupServiceError != SOCKET_ERROR) {
        // Iterate over each found bluetooth service
        bool inquiryComplete = false;
        while (!inquiryComplete) {
            // For each bluetooth service retrieve its corresponding details
            lookupServiceError = WSALookupServiceNext(lookupServiceHandle, flags, &querySetSize, querySet);
            if (lookupServiceError != SOCKET_ERROR) {
                char address[40] = { 0 };
                DWORD addressLength = _countof(address);
                SOCKADDR_BTH *bluetoothSocketAddress = (SOCKADDR_BTH *)querySet->lpcsaBuffer->RemoteAddr.lpSockaddr;
                BTH_ADDR bluetoothAddress = bluetoothSocketAddress->btAddr;

                // Emit the corresponding event if we were able to retrieve the address
                int addressToStringError = WSAAddressToString(querySet->lpcsaBuffer->RemoteAddr.lpSockaddr,
                                                              sizeof(SOCKADDR_BTH),
                                                              nullptr,
                                                              address,
                                                              &addressLength);
                if (addressToStringError != SOCKET_ERROR) {
                    // Strip any leading and trailing parentheses is encountered
                    char strippedAddress[19] = { 0 };
                    auto addressString = sscanf_s(address, "(" "%18[^)]" ")", strippedAddress) == 1
                                         ? NanNew(strippedAddress)
                                         : NanNew(address);

                    Local<Value> argv[3] = {
                        NanNew("found"),
                        addressString,
                        NanNew(querySet->lpszServiceInstanceName)
                    };

                    NanMakeCallback(args.This(), "emit", 3, argv);
                }
            } else {
                int lookupServiceErrorNumber = WSAGetLastError();
                if (lookupServiceErrorNumber == WSAEFAULT) {
                    free(querySet);
                    querySet = (WSAQUERYSET *)malloc(querySetSize);
                    if (querySet == nullptr) {
                        WSALookupServiceEnd(lookupServiceHandle);
                        NanThrowError("Out of memory: Unable to allocate memory resource for inquiry");
                    }
                } else if (lookupServiceErrorNumber == WSA_E_NO_MORE) {
                    // No more services where found
                    inquiryComplete = true;
                } else {
                    // Unhandled error
                    inquiryComplete = true;
                }
            }
        }
    } else {
        int lookupServiceErrorNumber = WSAGetLastError();
        if (lookupServiceErrorNumber != WSASERVICE_NOT_FOUND) {
            free(querySet);
            NanThrowError("Unable to initiate client device inquiry");
        }
    }

    free(querySet);
    WSALookupServiceEnd(lookupServiceHandle);

    Local<Value> argv[1] = {
        NanNew("finished")
    };

    NanMakeCallback(args.This(), "emit", 1, argv);

    NanReturnUndefined();
}

NAN_METHOD(DeviceINQ::SdpSearch) {
    NanScope();

    if (args.Length() != 2) {
        NanThrowError("usage: findSerialPortChannel(address, callback)");
    }

    if (!args[0]->IsString()) {
        NanThrowTypeError("First argument should be a string value");
    }

    if(!args[1]->IsFunction()) {
        NanThrowTypeError("Second argument must be a function");
    }

    sdp_baton_t *baton = new sdp_baton_t();
    String::Utf8Value address(args[0]);
    if (strcpy_s(baton->address, *address) != 0) {
        delete baton;
        NanThrowTypeError("Address (first argument) length is invalid");
    }

    Local<Function> cb = args[1].As<Function>();
    DeviceINQ *inquire = ObjectWrap::Unwrap<DeviceINQ>(args.This());

    baton->inquire = inquire;
    baton->cb = new NanCallback(cb);
    baton->channelID = -1;
    baton->request.data = baton;
    baton->inquire->Ref();

    uv_queue_work(uv_default_loop(), &baton->request, EIO_SdpSearch, (uv_after_work_cb)EIO_AfterSdpSearch);

    NanReturnUndefined();
}

NAN_METHOD(DeviceINQ::ListPairedDevices) {
    NanScope();

    const char *usage = "usage: listPairedDevices(callback)";
    if (args.Length() != 1) {
        NanThrowError(usage);
    }

    if(!args[0]->IsFunction()) {
        NanThrowTypeError("First argument must be a function");
    }
    Local<Function> cb = args[0].As<Function>();
    Local<Array> resultArray = Local<Array>(NanNew<Array>());

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
    cb->Call(NanGetCurrentContext()->Global(), 1, argv);

    NanReturnUndefined();
}
