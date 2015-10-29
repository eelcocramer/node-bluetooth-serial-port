# Bluetooth serial port communication for Node.js

[![Build Status](https://travis-ci.org/eelcocramer/node-bluetooth-serial-port.svg)](https://travis-ci.org/eelcocramer/node-bluetooth-serial-port)
[![Build status](https://ci.appveyor.com/api/projects/status/4p1r3ddoid98qc7k?svg=true)](https://ci.appveyor.com/project/eelcocramer/node-bluetooth-serial-port)

This node module lets you communicate over Bluetooth serial port with devices using Node.js. The goal is have an easy to use API. This module is great for communicating with Bluetooth enabled Arduino devices.

If you have any problems make sure to [checkout the FAQ](https://github.com/eelcocramer/node-bluetooth-serial-port/issues?q=label%3AFAQ).

## New in this release

* Fixes a critical [runtime error](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/83) on all platforms.
* Improves travis build.

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

## Test build Linux using docker

`docker build -t bluetooth-serial-port .`

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

#### BluetoothSerialPort.connect(bluetoothAddress, channel[, successCallback, errorCallback])

Connects to a remote bluetooth device.

* bluetoothAddress - the address of the remote Bluetooth device.
* channel - the channel to connect to.
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
