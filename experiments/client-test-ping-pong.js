/*
 * Copyright (c) 2012-2013, Eelco Cramer, Juan Gomez Mosquera
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
    
    if(!process.argv[2]){
	console.log("Usage:\n");
	console.log(process.argv[0] + " " + process.argv[1] + " <severName>\n");
	console.log("Where:\n<serverName>\tThe name being advertised by the Bluetooth Server\n");
	process.exit(-1);
    }

    var serverName = process.argv[2];
	

    var util = require('util');
    var DeviceINQ = require("../lib/device-inquiry.js").DeviceINQ;
    var BluetoothSerialPort = require("../lib/bluetooth-serial-port.js").BluetoothSerialPort;
    var serial = new BluetoothSerialPort();
    const MAX_MSGS_SENT = 10;
    var keepScanning = false;

    serial.on('found', function (address, name) {
        console.log('Found: ' + address + ' with name ' + name);

        serial.findSerialPortChannel(address, function(channel) {
            console.log('Found RFCOMM channel for serial port on ' + name + ': ' + channel);

            if (name !== serverName) return;

            console.log('Attempting to connect...');

            serial.connect(address, channel, function() {
		keepScanning  = false;
    		let packetsSent = 0;
                console.log('Connected. Sending data...');
                let buf = Buffer.from('PING');
                console.log('Size of buf = ' + buf.length);

		serial.on('failure', function(err){
			console.log('Something wrong happened!!: err = ' + err);
		});

                serial.on('data', function(buffer) {
                    console.log('Received: Size of data buf = ' + buffer.length);
                    console.log(buffer.toString('utf-8'));
		    serial.write(buf, function(err,count){
                    	if(err){
                       		console.log('Error received: ' + err);
				return;
	                }
        	        
			console.log('Sent: Bytes written: ' + count);
			packetsSent++;
			console.log('Sent: count = ' + packetsSent);
			if(packetsSent == MAX_MSGS_SENT){
				console.log('' + MAX_MSGS_SENT + ' sent!. Closing connection');
				serial.close();
				process.exit(0);
			}
		    });
                });

                serial.write(buf, function(err, count) {
                    if (err) {
                        console.log('Error received: ' + err);
                    } else {
                        console.log('Bytes writen is: ' + count);
                    }
                });
            });
        });
    });

    serial.on('close', function() {
        console.log('connection has been closed (remotely?)');
    });

    serial.on('finished', function() {
        console.log('Scan finished.');
	if(keepScanning == true){
		console.log('Rescanning..');
		serial.inquire();
	}
    });

    serial.inquire();
})();
