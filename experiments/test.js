/*
 * Copyright (c) 2012-2013, Eelco Cramer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

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

var discovery = IOBluetoothDeviceInquiryDelegate($.NSObject.extend('IOBluetoothDeviceInquiryDelegate'));

var bdi = $.IOBluetoothDeviceInquiry('alloc')('initWithDelegate', discovery);


//bdi('setDelegate', discovery);

console.log(bdi('start'));

$.CFRunLoopRun();
var readline = require('readline');
var rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout
});

rl.question('What is your favorite food?', function(answer) {
  console.log('Oh, so your favorite food is ' + answer);
	pool('drain');
});

