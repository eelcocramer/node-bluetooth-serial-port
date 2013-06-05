# Bluetooth serial port communication for Node.js

This node module lets you communicate over Bluetooth serial port with devices using Node.js. The goal is have an easy to use API. This module is great for communicating with Bluetooth enabled Arduino devices.



## RELEASE NOTES

### 0.2.1

* Fixes issue where calling `close` on a connection would result in an `Abort trap: 6` error on OS X.

### 0.2.0

* Experimental support for OS X!
* `findSerialPortChannel` does not invoke callback anymore when no channel was found.
* `found` event now emits the Bluetooth address as the value of the name parameter `name` when the name of the device could not be determined (used to be `[undefined]`).

## Limitations

* Available on Linux
* Experimental support for OS X
* Not available for Windows
* Data is passed as strings

## Pre-requests on Linux

* Needs Bluetooth development packages to build

`apt-get install libbluetooth-dev`

## Pre-request on OS X

* Needs XCode and XCode command line tools installed.

## Install

`npm install bluetooth-serial-port`

# Documentation

## Basic usage

```javascript

var btSerial = new (require('bluetooth-serial-port')).BluetoothSerialPort();

btSerial.on('found', function(address, name) {
	btSerial.findSerialPortChannel(address, function(channel) {
		btSerial.connect(address, channel, function() {
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

btSerial.inquire();
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
* name - the name of the device (or the address if the name is unavailable)

#### Event: ('finished')

Emitted when the device inquiry execution did finish.

#### BluetoothSerialPort.inquire()

Starts searching for bluetooth devices. When a device is found a 'found' event will be emitted.

#### BluetoothSerialPort.findSerialPortChannel(address, callback)

Checks if a device has a serial port service running and if it is found it passes the channel id to use for the RFCOMM connection.

* callback(channel) - called when finished looking for a serial port on the device.

#### BluetoothSerialPort.connect(bluetoothAddress[, successCallback, errorCallback])

Connects to a remote bluetooth device.

* bluetoothAddress - the address of the remote Bluetooth device.
* [successCallback] - called when a connection has been established.
* [errorCallback(msg)] - called when the connection attempt results in an error.

#### BluetoothSerialPort.close()

Closes the connection.

#### BluetoothSerialPort.isOpen()

Check whether the connection is open or not.

#### BluetoothSerialPort.write(data)

Writes a string to the serial port connection.

* data - the data string to be written.

* Throws an exception when is called before a connection has been established.

## LICENSE

This module is available under a [FreeBSD license](http://opensource.org/licenses/BSD-2-Clause), see the [LICENSE file](https://raw.github.com/eelcocramer/node-bluetooth-serial-port/master/LICENSE) for details.
