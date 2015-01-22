# Bluetooth serial port communication for Node.js

This node module lets you communicate over Bluetooth serial port with devices using Node.js. The goal is have an easy to use API. This module is great for communicating with Bluetooth enabled Arduino devices.

If you have any problems make sure to [checkout the FAQ](https://github.com/eelcocramer/node-bluetooth-serial-port/issues?q=label%3AFAQ).

## RELEASE NOTES

### 1.2.1

* Fixes issues compile issues on OS X and Linux when using newer versions of [nan](https://github.com/rvagg/nan).

### 1.2.0

> *PLEASE NOTE* This release is not yet available for the Windows platform because the [compilation needs to be verified first](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/53). As there are no major functional changes this release and the release, hopefully, will fix major issues on OSX I decided to go forward without Windows support for the moment. This will hopefully be fixed in the next release. I will update as soon as possible.

* Fixes an [issue on OSX](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/46) where multiple reads would result into a corrupted read buffer.
* [Improves](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/51) the implementation of the Bluetooth worker on OSX.
* Better [performance](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/35) while writing to the Bluetooth connection on OSX.
* Keeps the [reader loop from reading from a closed connection]((https://github.com/eelcocramer/node-bluetooth-serial-port/issues/47).
* Will work on both node v0.8.x, v0.10.x and node v0.11.x on OSX and Linux (Windows to be done).
* When trying to write to closed connection the `write` function will not throw an exception anymore but will call the callback as per documentation.
* Adds a `closed` event that fires when a connection is closed either by the user or remotely.

### 1.1.4

* Fixes an compile issue on Windows.

### 1.1.3

* Fixes [segfault](https://github.com/eelcocramer/node-bluetooth-serial-port/pull/29) that occurs when a buffer is invalidated by the garbage collector.
* Adds experimental support for listing paired devices.

### 1.1.2

* Updates the documentation to reflect the changes made in version 1.1.0.

### 1.1.1

* Fixes typo in readme.

### 1.1.0

* Fixes [buffer overflow on close()](https://github.com/eelcocramer/node-bluetooth-serial-port/pull/26) in Mac OSX.
* Adds failure callback that is called when no channel can be found.
* Fixes an [issue on Mac OSX](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/23) where a write action would fail when the MTU was exceeded.
* Fixes an [issue on Mac OSX](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/24) where data would not be written asynchronously.

### 1.0.5

* Updates the code example in the README.
* Adds `win32` to the supported OS'es in the `package.json`.

### 1.0.4

* Added windows support.

### 1.0.3

* Fixes an [issue on Linux](https://github.com/eelcocramer/node-bluetooth-serial-port/pull/11) where reading from a closed or reset connection would result into a SEGFAULT.

### 1.0.2

* Updates the documentation.
* Fixes an issue where memory is freed incorrectly after closing a connection on OS X.
* Improves the timeout mechanism that is used for getting the Bluetooth service records on a remote device on OS X.

### 1.0.1

* No code changes, only updates the documentation.

### 1.0.0

* Makes the write function asynchrone.
* Takes a [Buffer](http://nodejs.org/api/buffer.html) as the input for the write function in favor of a String.
* Reads data into a [Buffer](http://nodejs.org/api/buffer.html) object instead of using a String.
* Improves error handling when calling the native addon.

### 0.2.1

* Fixes issue where calling `close` on a connection would result in an `Abort trap: 6` error on OS X.

### 0.2.0

* Experimental support for OS X!
* `findSerialPortChannel` does not invoke callback anymore when no channel was found.
* `found` event now emits the Bluetooth address as the value of the name parameter `name` when the name of the device could not be determined (used to be `[undefined]`).

## Pre-requests on Linux

* Needs Bluetooth development packages to build

`apt-get install build-essential libbluetooth-dev`

## Pre-request on OS X

* Needs XCode and XCode command line tools installed.

## Pre-request on Windows

* Needs Visual Studio (Visual C++) and its command line tools installed.
* Needs Python 2.x installed and accessible from the command line path.

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

			btSerial.write(new Buffer('my data', 'utf-8'), function(err, bytesWritten) {
				if (err) console.log(err);
			});

			btSerial.on('data', function(buffer) {
				console.log(buffer.toString('utf-8'));
			});
		}, function () {
			console.log('cannot connect');
		});

		// close the connection when you're ready
		btSerial.close();
	}, function() {
		console.log('found nothing');
	});
});

btSerial.inquire();
```

## API

### BluetoothSerialPort

#### Event: ('data', buffer)

Emitted when data is read from the serial port connection.

* buffer - the data that was read into a [Buffer](http://nodejs.org/api/buffer.html) object.

### Event: ('closed')

Emitted when a connection was closed either by the user (i.e. calling `close` or remotely).

#### Event: ('failure', err)

Emitted when reading from the serial port connection results in an error. The connection is closed.

* err - an [Error object](http://docs.nodejitsu.com/articles/errors/what-is-the-error-object) describing the failure.

#### Event: ('found', address, name)

Emitted when a bluetooth device was found.

* address - the address of the device
* name - the name of the device (or the address if the name is unavailable)

#### Event: ('finished')

Emitted when the device inquiry execution did finish.

#### BluetoothSerialPort.inquire()

Starts searching for bluetooth devices. When a device is found a 'found' event will be emitted.

#### BluetoothSerialPort.findSerialPortChannel(address, callback[, errorCallback])

Checks if a device has a serial port service running and if it is found it passes the channel id to use for the RFCOMM connection.

* callback(channel) - called when finished looking for a serial port on the device.
* errorCallback - called the search finished but no serial port channel was found on the device.

#### BluetoothSerialPort.connect(bluetoothAddress[, successCallback, errorCallback])

Connects to a remote bluetooth device.

* bluetoothAddress - the address of the remote Bluetooth device.
* [successCallback] - called when a connection has been established.
* [errorCallback(err)] - called when the connection attempt results in an error. The parameter is an [Error object](http://docs.nodejitsu.com/articles/errors/what-is-the-error-object).

#### BluetoothSerialPort.close()

Closes the connection.

#### BluetoothSerialPort.isOpen()

Check whether the connection is open or not.

#### BluetoothSerialPort.write(buffer, callback)

Writes a [Buffer](http://nodejs.org/api/buffer.html) to the serial port connection.

* buffer - the [Buffer](http://nodejs.org/api/buffer.html) to be written.
* callback(err, bytesWritten) - is called when the write action has been completed. When the `err` parameter is set an error has occured, in that case `err` is an [Error object](http://docs.nodejitsu.com/articles/errors/what-is-the-error-object). When `err` is not set the write action was successful and `bytesWritten` contains the amount of bytes that is written to the connection.

#### BluetoothSerialPort.listPairedDevices(callback)

__ONLY ON OSX__

Lists the devices that are currently paired with the host.

* callback(pairedDevices) - is called when the paired devices object has been populated. See the [pull request](https://github.com/eelcocramer/node-bluetooth-serial-port/pull/30) for more information on the `pairedDevices` object.

## LICENSE

This module is available under a [FreeBSD license](http://opensource.org/licenses/BSD-2-Clause), see the [LICENSE file](https://github.com/eelcocramer/node-bluetooth-serial-port/blob/master/LICENSE.md) for details.
