# Bluetooth serial port communication for Node.js

This node module lets you communicate over Bluetooth serial port with devices using Node.js. The goal is have an easy to use API. This module is great for communicating with Bluetooth enabled Arduino devices.

# Limitations

* Only tested on Linux
* Only available on Linux and BSD like systems
* Not available for Windows and Mac OS X
* Currently all data is passed as strings

# Pre-requests

* Needs bluetooth development packages to build.
* The bluetooth device should already be paired.

# Install

`npm install rfcomm`

# Documentation

## Basic usage

```javascript

var btSerial = new require('bluetooth-serial-port').BluetoothSerialPort();

btSerial.connect(bluetoothAddress, function() {
	console.log('connected');
	
	btSerial.write('my data');
	
	btSerial.on('data', function(data) {
		console.log(data);
	});
}, function () {
	console.log('cannot connect');
});

// close the connection when you're ready
btSerial.close();
```

## API

### BluetoothSerialPort

#### Event: 'data'

Emitted when data is read from the serial port connection.

#### Event: 'error'

Emitted when reading form the serial port connection results in an error. The connection is closed.

#### BluetoothSerialPort.connect(bluetoothAddress[, successCallback, errorCallback])

Connects to a remote bluetooth device.

* bluetoothAddress - the address of the remote Bluetooth device.
* [successCallback] - called when a connection has been established.
* [errorCallback(msg)] - called when the connection attempt results in an error.

#### BluetoothSerialPort.close()

Closes the connection.

#### BluetoothSerialPort.write(data)

Writes a string to the serial port connection.

* data - the data string to be written.

* Throws an exception when is called before a connection has been established.
