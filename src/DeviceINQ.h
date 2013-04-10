#ifndef DEVICE_INQ_H
#define DEVICE_INQ_H

#include <node.h>

class DeviceINQ : public node::ObjectWrap {
	public:
		static void Init(v8::Handle<v8::Object> exports);
		static void EIO_SdpSearch(uv_work_t *req);
		static void EIO_AfterSdpSearch(uv_work_t *req);

	private:
	  	struct sdp_baton_t {
	        DeviceINQ *inquire;
		    uv_work_t request;
	        v8::Persistent<v8::Function> cb;
	        int channel;
	        char address[19];
	    };
	
  		DeviceINQ();
  		~DeviceINQ();

		static v8::Handle<v8::Value> New(const v8::Arguments& args);
		static v8::Handle<v8::Value> Inquire(const v8::Arguments& args);
		static v8::Handle<v8::Value> SdpSearch(const v8::Arguments& args);
};

#endif