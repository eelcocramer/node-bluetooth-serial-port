# RFCOMM (Serial Bluetooth) communication for Node.js

This node module lets you communicate with Bluetooth devices in Node.js. The goal is have an easy to use API. This module is great for communicating with Bluetooth enabled Arduino devices.

# Limitations

* Only tested on Linux
* Only available on Linux and BSD like systems
* Not available for Windows and Mac OS X

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
```
