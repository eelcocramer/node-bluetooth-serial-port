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

class RFCOMMBinding: ObjectWrap {
private:
    int s;
    
    struct connect_baton_t {
        RFCOMMBinding *rfcomm;
        Persistent<Function> cb;
        Persistent<Function> ecb;
        char address[19];
        int status;
    };
    
    static void EIO_Connect(uv_work_t *req) {
        connect_baton_t *baton = static_cast<connect_baton_t *>(req->data);
        
        struct sockaddr_rc addr = { 0 };

        // allocate a socket
        baton->rfcomm->s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

        // set the connection parameters (who to connect to)
        addr.rc_family = AF_BLUETOOTH;
        addr.rc_channel = (uint8_t) 1;
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
        RFCOMMBinding *rfcomm;
        Persistent<Function> cb;
        char result[1024];
        int errorno;
        int size;
    };
 
    static void EIO_Read(uv_work_t *req) {
        printf("test123");

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
        
        printf("Errorno = %d - Size = %d", baton->errorno, baton->size);
        
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
        s_ct->SetClassName(String::NewSymbol("RFCOMMBinding"));
        
        NODE_SET_PROTOTYPE_METHOD(s_ct, "write", Write);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "read", Read);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "close", Close);
        target->Set(String::NewSymbol("RFCOMMBinding"), s_ct->GetFunction());
        target->Set(String::NewSymbol("RFCOMMBinding"), s_ct->GetFunction());
        target->Set(String::NewSymbol("RFCOMMBinding"), s_ct->GetFunction());
    }
    
    RFCOMMBinding() : 
        s(0) {
        
    }
    
    ~RFCOMMBinding() {
        
    }
    
    static Handle<Value> New(const Arguments& args) {
        HandleScope scope;

        const char *usage = "usage: RFCOMMBinding(address, callback, error)";
        if (args.Length() != 3) {
            return ThrowException(Exception::Error(String::New(usage)));
        }
        
        String::Utf8Value address(args[0]);
        Local<Function> cb = Local<Function>::Cast(args[1]);
        Local<Function> ecb = Local<Function>::Cast(args[2]);
        
        RFCOMMBinding* rfcomm = new RFCOMMBinding();
        rfcomm->Wrap(args.This());

        connect_baton_t *baton = new connect_baton_t();
        baton->rfcomm = rfcomm;
        strcpy(baton->address, *address);
        baton->cb = Persistent<Function>::New(cb);
        baton->ecb = Persistent<Function>::New(ecb);
        rfcomm->Ref();

        uv_work_t *req = new uv_work_t;
        req->data = baton;
        uv_queue_work(uv_default_loop(), req, EIO_Connect, EIO_AfterConnect);
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
        
        RFCOMMBinding* rfcomm = ObjectWrap::Unwrap<RFCOMMBinding>(args.This());
        
        write(rfcomm->s, *str, str.length() + 1);
        
        return Undefined();
    }
 
    static Handle<Value> Close(const Arguments& args) {
        HandleScope scope;
        
        const char *usage = "usage: close()";
        if (args.Length() != 0) {
            return ThrowException(Exception::Error(String::New(usage)));
        }
        
        RFCOMMBinding* rfcomm = ObjectWrap::Unwrap<RFCOMMBinding>(args.This());
        
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
                
        RFCOMMBinding* rfcomm = ObjectWrap::Unwrap<RFCOMMBinding>(args.This());
  
        
        read_baton_t *baton = new read_baton_t();
        baton->rfcomm = rfcomm;
        baton->cb = Persistent<Function>::New(cb);
        rfcomm->Ref();

        uv_work_t *req = new uv_work_t;
        req->data = baton;
        uv_queue_work(uv_default_loop(), req, EIO_Read, EIO_AfterRead);
        uv_ref((uv_handle_t *) &req);

        return Undefined();
    }
    
};

Persistent<FunctionTemplate> RFCOMMBinding::s_ct;

extern "C" {
    void init (Handle<Object> target) {
        HandleScope scope;
        RFCOMMBinding::Init(target);
    }

    NODE_MODULE(RFCOMMBinding, init);
}
