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

`npm install bluetooth-serial-port`

# Documentation

## Basic usage

```javascript

var btSerial = new require('bluetooth-serial-port').BluetoothSerialPort();

btSerial.inquire();

btSerial.on('found', function(address, name) {
	btSerial.findSerialPortChannel(address, function(channel) {
		btSerial.connect(bluetoothAddress, channel, function() {
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
	});
});

```

## API

### BluetoothSerialPort

#### Event: ('data', data)

Emitted when data is read from the serial port connection.

* data - the data that was read

#### Event: ('failure', message)

Emitted when reading form the serial port connection results in an error. The connection is closed.

* message - an message describing the failure.

#### Event: ('found', address, name)

Emitted when a bluetooth device was found.

* address - the address of the device
* name - the name of the device

#### Event: ('finnished')

Emitted when the device inquiry execution did finnish.

#### BluetoothSerialPort.inquire()

Starts searching for bluetooth devices. When a device is found a 'found' event will be emitted.

#### BluetoothSerialPort.findSerialPortChannel(address, callback)

Checks if a device has a serial port service running and if it is found it passes the channel id to use for the RFCOMM connection.

* callback(channel) - called when finished looking for a serial port on the device. channel === -1 if no channel was found.

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
