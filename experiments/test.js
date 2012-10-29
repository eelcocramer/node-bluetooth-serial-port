var $ = require('NodObjC');
var IOBluetoothDeviceInquiryDelegate = require('./IOBluetoothDeviceInquiryDelegate.js');


// First you need to "import" the Framework
$.import('Foundation');
$.import('IOBluetooth');

console.log($.IOBluetoothDeviceInquiry);

// Setup the recommended NSAutoreleasePool instance
var pool = $.NSAutoreleasePool('alloc')('init');

// var discovery = $.NSObject.extend('IOBluetoothDeviceInquiryDelegate');
// discovery.addMethod('deviceInquiryDeviceFound:', 'v@:@', function(self, _device, device) {
//     console.log('blaat');
// });
// 
// discovery.addMethod('deviceInquiryComplete:', 'v@:@', function(self, _error, error, _aborted, aborted)  {
//     console.log('blaat');
// });
// 
// discovery.addMethod('deviceInquiryStarted:', 'v@:@', function(self)  {
//     console.log('blaat123');
// });

var bdi = $.IOBluetoothDeviceInquiry('alloc')('init');
console.log('blat');

var discovery = IOBluetoothDeviceInquiryDelegate($.NSObject.extend('IOBluetoothDeviceInquiryDelegate'));

bdi('setDelegate', discovery);

console.log('blat');

console.log(bdi('start'));

console.log('start');

$.CFRunLoopRun();

pool('drain');
