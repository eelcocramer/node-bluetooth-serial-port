#ifndef BT_SERIAL_PORT_BINDING_H
#define BT_SERIAL_PORT_BINDING_H

#include <node.h>

class BTSerialPortBinding : public node::ObjectWrap {
	public:
	    static v8::Persistent<v8::FunctionTemplate> s_ct;
		static void Init(v8::Handle<v8::Object> exports);
		static v8::Handle<v8::Value> Write(const v8::Arguments& args);
		static v8::Handle<v8::Value> Close(const v8::Arguments& args);
		static v8::Handle<v8::Value> Read(const v8::Arguments& args);

	private:
		struct connect_baton_t {
		    BTSerialPortBinding *rfcomm;
		    uv_work_t request;
		    v8::Persistent<v8::Function> cb;
		    v8::Persistent<v8::Function> ecb;
		    char address[19];
		    int status;
		    int channel;
		};

		struct read_baton_t {
		    BTSerialPortBinding *rfcomm;
		    uv_work_t request;
		    v8::Persistent<v8::Function> cb;
		    char result[1024];
		    int errorno;
		    int size;
		};

	    int s;

  		BTSerialPortBinding();
  		~BTSerialPortBinding();

		static v8::Handle<v8::Value> New(const v8::Arguments& args);
		static void EIO_Connect(uv_work_t *req);
		static void EIO_AfterConnect(uv_work_t *req);
		static void EIO_Read(uv_work_t *req);
		static void EIO_AfterRead(uv_work_t *req);
};

#endif