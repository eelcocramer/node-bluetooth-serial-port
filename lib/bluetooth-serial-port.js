#!/usr/bin/env node

/*
 * Bluetooth UPnP Proxy demo
 *
 * Copyright 2012 Eelco Cramer, TNO
 */
 
var upnp = require('upnp-device');
var BinaryLight = require('./upnp/BinaryLight')
var BluetoothSerialPort = require('bluetooth-serial-port').BluetoothSerialPort;
var serial = new BluetoothSerialPort();
var winston = require('winston');
var connected = false;

var logger = new (winston.Logger)({
    transports: [
      new (winston.transports.Console)({ colorize: 'true' })
    ]
});

serial.on('found', function(address, name) {
    if (address == '00:11:09:06:06:81') {
        if (connected) {
            winston.warn('Already connected to the light.');
            return;
        }
        
        winston.info('Light found, looking for serial port.');
        serial.findSerialPortChannel(address, function (channel) {
            winston.info('Found channel.');
            if (channel == -1) {
                winston.error('No serial port found!');
            } else {
                serial.connect(address, 1, function() {
                    winston.info('Connected to the bluetooth binary light device on ' + name);
                    connected = true;

                    var binaryLight = upnp.createMyDevice(BinaryLight, 'Bluetooth UPnP Binary Light Demo @ ' + name);

                    binaryLight.on('ready', function() {
                        binaryLight.ssdpAnnounce()

                        serial.write('s'); // get the current status value

                        binaryLight.services.SwitchPower.addListener(function(value) {
                            winston.info('Switch light to: ' + value);
                            serial.write(value);
                        });
                    });

                    serial.on('data', function(data) {
                        winston.debug('Data received from light: ' + data);

                        if (data === '0' || data === '1') {
                            binaryLight.services.SwitchPower.stateVars['Status'] = data;
                        }
                    });

                    var stop = function(doNotExit) {
                        if (doNotExit) {
                            winston.info('Stopping the session.');
                        } else {
                            winston.info('Stopping the proxy.');
                        }
                        
                        if (serial) {
                            serial.close();
                        }
                        
                        if (binaryLight) {
                            binaryLight.ssdpBroadcast('byebye');
                            binaryLight.httpServer.close();
                        }
                        
                        if (!doNotExit) {
                            process.exit();
                        }
                    };

                    serial.on('failure', function(msg) {
                        winston.error(msg);
                        stop(true);
                        process.removeListener('SIGINT', stop);
                        connected = false;
                    });

                    process.on('SIGINT', stop);
                });
            }
        });
    }    
});

var finnished = true;

var inqueryLoop = function() {
    if (!connected && finnished) {
        winston.info('Start bluetooth discovery.');
        finnished = false;
        serial.inquire();
    }
};

serial.on('finnished', function() {
    winston.info('Bluetooth discovery finnished.');
    finnished = true;
});

inqueryLoop();
setInterval(inqueryLoop, 60000);
