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
    var EventEmitter = require('events').EventEmitter;
    var btSerial = require('bindings')('BluetoothSerialPort.node');
    var DeviceINQ = require("./device-inquiry.js").DeviceINQ;

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

        this.inq.on('found', function(address, name) {
            self.emit('found', address, name);
        });

        this.inq.on('finished', function() {
            self.emit('finished');
        });
    }

    util.inherits(BluetoothSerialPort, EventEmitter);
    exports.BluetoothSerialPort = BluetoothSerialPort;

    BluetoothSerialPort.prototype.inquire = function() {
        this.inq.inquire();
    };

    BluetoothSerialPort.prototype.findSerialPortChannel = function(address, callback) {
        this.inq.findSerialPortChannel(address, callback);
    };

    BluetoothSerialPort.prototype.connect = function(address, channel, successCallback, errorCallback) {
        var self = this;

        var connection = new btSerial.BTSerialPortBinding(address, channel, function(err) {
            self.buffer = [];
            self.connection = connection;
            self.isReading = false;
            self.eventLoop = setInterval(eventLoop, 100, self);
            successCallback();
        }, function (err) {
            if (errorCallback) {
                errorCallback(err);
            }
        });
    };

    BluetoothSerialPort.prototype.write = function(data) {
        if (this.buffer) {
            this.buffer.push(data);
        } else {
            throw('Not connected yet.');
        }
    };

    BluetoothSerialPort.prototype.close = function() {
        if (this.eventLoop) {
            clearInterval(this.eventLoop);
        }

        if (this.connection) {
            this.connection.close();
            this.connection = undefined;
        }
    };

    var eventLoop = function(self) {
        if (self.buffer && self.buffer.length > 0) {
            for (var i in self.buffer) {
                if (self.connection) {
                    self.connection.write(self.buffer[i]);
                } else {
                    throw('Invalid state. No connection object to write to.');
                }
            }

            self.buffer = [];
        }
        if (!self.isReading) {
            process.nextTick(function() {
                if (self.connection) {
                    self.isReading = true;
                    self.connection.read(function(data, size) {
                        self.isReading = false;
                        if (size >= 0) {
                            self.emit('data', data);
                        } else {
                            self.close();
                            self.emit('failure', 'Error reading from the connection. The connection is lost.');
                        }
                    });
                }
            });
        }
    };

})();
