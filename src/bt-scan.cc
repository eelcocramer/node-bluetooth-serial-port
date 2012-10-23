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

static Handle<Value> SetTarget (const Arguments&);
extern "C" void init (Handle<Object>);

// Scans for bluetooth devices in the background.
// this function happens on the thread pool
// doing v8 things in here will make bad happen.
static int DoScan (eio_req *req) {
  struct scan_callback * sr = (struct scan_callback *)req->data;

  inquiry_info *ii = NULL;
  int max_rsp, num_rsp;
  int dev_id, sock, len, flags;
  int i;
  char addr[19] = { 0 };
  char name[248] = { 0 };

  dev_id = hci_get_route(NULL);
  sock = hci_open_dev( dev_id );
  if (dev_id < 0 || sock < 0) {
      perror("opening socket");
      exit(1);
  }

  len  = 8;
  max_rsp = 255;
  flags = IREQ_CACHE_FLUSH;
  ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));
  
  num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
  if( num_rsp < 0 ) perror("hci_inquiry");

  scan_result *item = NULL;
  
  for (i = 0; i < num_rsp; i++) {
      ba2str(&(ii+i)->bdaddr, addr);
      memset(name, 0, sizeof(name));
      if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name), 
          name, 0) < 0)
      strcpy(name, "[unknown]");
      
      item = (scan_result *)malloc(sizeof(struct scan_result));
      strcpy(item->name, name);
      strcpy(item->address, addr);
//      printf("%s  %s\n", addr, name);
  }

  req->result = item;

  free( ii );
  close( sock );

  return 0;
}

static int DoScan_After (eio_req *req) {
  HandleScope scope;
  ev_unref(EV_DEFAULT_UC);
  struct scan_callback * sr = (struct scan_callback *)req->data;
  Local<Value> argv[3];
  argv[0] = Local<Value>::New(req->result);
  TryCatch try_catch;
  sr->cb->Call(Context::GetCurrent()->Global(), 1, argv);
  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
  sr->cb.Dispose();
  free(sr);
  free(req->result);
  return 0;
}

/*
 * Sets the target value of the binary light
 */
static Handle<Value> SetTarget (const Arguments& args) {
  HandleScope scope;
  const char *usage = "usage: setTarget(val)";
  if (args.Length() != 1) {
    return ThrowException(Exception::Error(String::New(usage)));
  }
  
  int val = args[0]->Int32Value();

  return Undefined();
}

/*
 * Sets the target value of the binary light
 */
static Handle<Value> ListenToStatus (const Arguments& args) {
  HandleScope scope;
  const char *usage = "usage: listenToStatus(cb)";
  if (args.Length() != 1) {
    return ThrowException(Exception::Error(String::New(usage)));
  }
  
  Local<Function> cb = Local<Function>::Cast(args[0]);

  return Undefined();
}

/*
 * Sets the target value of the binary light
 */
static Handle<Value> Scan (const Arguments& args) {
  HandleScope scope;
  const char *usage = "usage: Scan(cb)";
  if (args.Length() != 1) {
    return ThrowException(Exception::Error(String::New(usage)));
  }
  
  Local<Function> cb = Local<Function>::Cast(args[0]);

  scan_callback *sc = (scan_callback *)malloc(sizeof(struct scan_callback));
  sc->cb = Persistent<Function>::New(cb);
  
  eio_custom(DoScan, EIO_PRI_DEFAULT, DoScan_After, sc);
  ev_ref(EV_DEFAULT_UC);

  return Undefined();
}

extern "C" void init (Handle<Object> target) {
  HandleScope scope;
  NODE_SET_METHOD(target, "setTarget", SetTarget);
  NODE_SET_METHOD(target, "listenToStatus", ListenToStatus);
  NODE_SET_METHOD(target, "scan", Scan);
}