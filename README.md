# RFCOMM (Serial Bluetooth) communication for Node.js

This node module lets you communicate with Bluetooth devices in Node.js. The goal is have an easy to use API. This module is great for communicating with Bluetooth enabled Arduino devices.

# Limitations

* Only tested on Linux
* Only available on Linux and BSD like systems
* Not available for Windows and Mac OS X
* Currently all data is passed as strings

# Install

`npm install rfcomm`

# Documentation

## Basic usage

```javascript

var rfcomm = new require('rfcomm').RFCOMM();

rfcomm.connect(bluetoothAddress, function() {
	console.log('connected');
	
	rfcomm.write('my data');
	
	rfcomm.on('data', function(data) {
		console.log(data);
	});
}, function () {
	console.log('cannot connect');
});

// close the connection when you're ready
rfcomm.close();
```

## API

### rfcomm.RFCOMM

#### Event: 'data'

Emitted when data is read from the RFCOMM connection.

#### Event: 'error'

Emitted when reading form the RFCOMM connection results in an error. The connection is closed.

#### RFCOMM.connect(bluetoothAddress[, successCallback, errorCallback])

Connects to a remote bluetooth device.

* bluetoothAddress - the address of the remote Bluetooth device.
* [successCallback] - called when a connection has been established.
* [errorCallback(msg)] - called when the connection attempt results in an error.

#### RFCOMM.close()

Closes the connection.

#### RFCOMM.write(data)

Writes a string to the RFCOMM connection.

* data - the data string to be written.

* Throws an exception when is called before a connection has been established.
