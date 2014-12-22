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

/*jslint node: true*/
/*global require*/

(function () {
    "use strict";

    var util = require('util'),
        EventEmitter = require('events').EventEmitter,
        btSerial = require('bindings')('BluetoothSerialPort.node'),
        DeviceINQ = require("./device-inquiry.js").DeviceINQ;

    /**
     * Creates an instance of the bluetooth-serial object.
     * @constructor
     * @name Connection
     * @param rpcHandler The RPC handler.
     */
    function BluetoothSerialPort() {
        EventEmitter.call(this);
        this.inq = new DeviceINQ();

        var self = this;

        this.inq.on('found', function (address, name) {
            self.emit('found', address, name);
        });

        this.inq.on('finished', function () {
            self.emit('finished');
        });
    }

    util.inherits(BluetoothSerialPort, EventEmitter);
    exports.BluetoothSerialPort = BluetoothSerialPort;

    BluetoothSerialPort.prototype.listPairedDevices = function (callback) {
        this.inq.listPairedDevices(callback);
    };

    BluetoothSerialPort.prototype.inquire = function () {
        this.inq.inquire();
    };

    BluetoothSerialPort.prototype.findSerialPortChannel = function (address, successCallback, errorCallback) {
        this.inq.findSerialPortChannel(address, function (channel) {
            if (channel >= 0) {
                successCallback(channel);
            } else if (errorCallback) {
                errorCallback();
            }
        });
    };

    BluetoothSerialPort.prototype.connect = function (address, channel, successCallback, errorCallback) {
        var self = this,
            read = function () {
                process.nextTick(function () {
                    if (self.connection) {
                        self.connection.read(function (err, buffer) {
                            if (!err && buffer) {
                                self.emit('data', buffer);
                            } else {
                                self.close();
                                self.emit('failure', err);
                            }
                        });
                    }
                });
            },
            connection = new btSerial.BTSerialPortBinding(address, channel, function () {
                self.address = address;
                self.buffer = [];
                self.connection = connection;
                self.isReading = false;

                self.on('data', function (buffer) {
                    if (buffer.length <= 0) {
                        // we are done reading. The remote device might have closed the device
                        // or we have closed it ourself. Lets cleanup our side anyway...
                        self.close();
                    } else {
                        read();
                    }
                });

                read();

                successCallback();
            }, function (err) {
                if (errorCallback) {
                    errorCallback(err);
                }
            });
    };

    BluetoothSerialPort.prototype.write = function (buffer, cb) {
        if (this.connection) {
            this.connection.write(buffer, this.address, cb);
        } else {
            var err = new Error("Not connected");
            cb.call(undefined, err);
        }
    };

    BluetoothSerialPort.prototype.close = function () {
        if (this.connection) {
            this.connection.close(this.address);
            this.connection = undefined;
        }

        this.emit('closed');
    };

    BluetoothSerialPort.prototype.isOpen = function () {
        return this.connection !== undefined;
    };
}());
