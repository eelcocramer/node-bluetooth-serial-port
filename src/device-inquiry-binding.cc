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

class DeviceINQ: ObjectWrap {
private:
    
    struct inquire_baton_t {
        DeviceINQ *inquire;
        Persistent<Function> cb;
    };
    
    static void EIO_Inquire(uv_work_t *req) {
        inquire_baton_t *baton = static_cast<inquire_baton_t *>(req->data);

        // do stuff
        inquiry_info *ii = NULL;
        int max_rsp, num_rsp;
        int dev_id, sock, len, flags;
        int i;
        char addr[19] = { 0 };
        char name[248] = { 0 };

        dev_id = hci_get_route(NULL);
        sock = hci_open_dev( dev_id );
        if (dev_id < 0 || sock < 0) {
            //perror("opening socket");
            return;
        }

        len  = 8;
        max_rsp = 255;
        flags = IREQ_CACHE_FLUSH;
        ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));

        num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
        // if( num_rsp < 0 ) perror("hci_inquiry");

        for (i = 0; i < num_rsp; i++) {
          ba2str(&(ii+i)->bdaddr, addr);
          memset(name, 0, sizeof(name));
          if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name), 
              name, 0) < 0)
          strcpy(name, "[unknown]");

          TryCatch try_catch;

          Local<Value> argv[2];
          argv[0] = String::New(addr);
          argv[1] = String::New(name);
          baton->cb->Call(Context::GetCurrent()->Global, 2, argv);

          if (try_catch.HasCaught()) {
              FatalException(try_catch);
          }
        }

        free( ii );
        close( sock );
    }
    
    static void EIO_Inquire(uv_work_t *req) {
        inquire_baton_t *baton = static_cast<inquire_baton_t *>(req->data);
        uv_unref((uv_handle_t*) &req);
        baton->inquire->Unref();
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
        s_ct->InstanceTemplate()->SetInternalFieldCount(0);
        s_ct->SetClassName(String::NewSymbol("DeviceINQ"));
        
        NODE_SET_PROTOTYPE_METHOD(s_ct, "inquire", Inquire);
        target->Set(String::NewSymbol("DeviceINQ"), s_ct->GetFunction());
    }
    
    RFCOMMBinding() {
        
    }
    
    ~RFCOMMBinding() {
        
    }
    
    static Handle<Value> New(const Arguments& args) {
        HandleScope scope;

        const char *usage = "usage: DeviceINQ()";
        if (args.Length() != 0) {
            return ThrowException(Exception::Error(String::New(usage)));
        }

        return args.This();
    }
 
    static Handle<Value> Inquire(const Arguments& args) {
        HandleScope scope;
        
        const char *usage = "usage: inquire(callback)";
        if (args.Length() != 1) {
            return ThrowException(Exception::Error(String::New(usage)));
        }
        
        Local<Function> cb = Local<Function>::Cast(args[0]);
                
        DeviceINQ* inquire = ObjectWrap::Unwrap<DeviceINQ>(args.This());
        
        inquire_baton_t *baton = new inquire_baton_t();
        baton->inquire = inquire;
        baton->cb = Persistent<Function>::New(cb);
        inquire->Ref();

        uv_work_t *req = new uv_work_t;
        req->data = baton;
        uv_queue_work(uv_default_loop(), req, EIO_Inquire, EIO_AfterInquire);
        uv_ref((uv_handle_t *) &req);

        return Undefined();
    }
    
};

Persistent<FunctionTemplate> DeviceINQ::s_ct;

extern "C" {
    void init (Handle<Object> target) {
        HandleScope scope;
        DeviceINQ::Init(target);
    }

    NODE_MODULE(DeviceINQ, init);
}
