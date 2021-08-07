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

(function() {
    "use strict";

    var util = require('util');
    var DeviceINQ = require("../lib/device-inquiry.js").DeviceINQ;
    var BluetoothSerialPort = require("../lib/bluetooth-serial-port.js").BluetoothSerialPort;
    var serial = new BluetoothSerialPort();

    serial.on('found', function(address, name) {
        console.log('Found: ' + address + ' with name ' + name);

        serial.findSerialPortChannel(address, function(channel) {
            console.log('Found RFCOMM channel for serial port on ' + name + ': ' + channel);

            if (name !== 'linvor') return;

            console.log('Attempting to connect...');

            serial.connect(address, channel, function() {
                console.log('Connected. Sending data...');
                var buf = Buffer.from('10011010101s');
                console.log('Size of buf = ' + buf.length);

                serial.on('data', function(buffer) {
                    console.log('Size of data buf = ' + buffer.length);
                    console.log(buffer.toString('utf-8'));
                });

                serial.write(buf, function(err, count) {
                    if (err) {
                        console.log('Error received: ' + err);
                    } else {
                        console.log('Bytes writen is: ' + count);
                    }

                    setTimeout(function() {
                        serial.write(buf, function(err, count) {
                            if (err) {
                                console.log('Error received: ' + err);
                            } else {
                                console.log('Bytes writen is: ' + count);
                            }

                            setTimeout(function() {
                                serial.close();
                                console.log('Closed and ready');
                            }, 5000);
                        });
                    }, 5000);
                });
            });
        });
    });

    serial.on('close', function() {
        console.log('connection has been closed (remotely?)');
    });

    serial.on('finished', function() {
        console.log('scan did finish');
    });

    serial.inquire();
})();
