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

        this.found = function (address, name) {
            self.emit('found', address, name);
        }

        this.finish = function () {
            self.emit('finished');
        }
    }

    util.inherits(BluetoothSerialPort, EventEmitter);
    exports.BluetoothSerialPort = BluetoothSerialPort;

    BluetoothSerialPort.prototype.listPairedDevices = function (callback) {
        this.inq.listPairedDevices(callback);
    };

    BluetoothSerialPort.prototype.inquire = function () {
        this.inq.inquire(this.found, this.finish);
    };

    BluetoothSerialPort.prototype.inquireSync = function () {
        this.inq.inquireSync(this.found, this.finish);
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
                                self.removeListener('data', dataListener);  // remove it to prevent
                                self.close();                               // calling self.on many
                                self.emit('failure', err);                  // times
                            }
                        });
                    }
                });
            },
            dataListener = function (buffer) { // event listenner
                if (buffer.length <= 0) {
                        // we are done reading. The remote device might have closed the device
                        // or we have closed it ourself. Lets cleanup our side anyway...
                        self.close();
                    } else {
                        read();
                    }
            },
            connection = new btSerial.BTSerialPortBinding(address, channel, function () {
                self.address = address;
                self.buffer = [];
                self.connection = connection;
                self.isReading = false;

                self.on('data', dataListener); // add listener to event 'data'

                read();

                successCallback();
            }, function (err) {
                // cleaning up the the failed connection
                connection.close(address);

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
            cb.call(err);
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

(function () {
    "use strict";

    if (process.platform !== 'linux') {
        return;
    }

    var util = require('util'),
        EventEmitter = require('events').EventEmitter,
        btSerial = require('bindings')('BluetoothSerialPortServer.node'),
        _SERIAL_PORT_PROFILE_UUID = '1101',
        _DEFAULT_SERVER_CHANNEL = 1,
        _ERROR_CLIENT_CLOSED_CONNECTION = 'Error: Connection closed by the client';

    /**
     * Creates an instance of the bluetooth-serial-server object.
     * @constructor
     */
    function BluetoothSerialPortServer() {
        EventEmitter.call(this);
        this.inDisconnect = false;
    }

    util.inherits(BluetoothSerialPortServer, EventEmitter);
    exports.BluetoothSerialPortServer = BluetoothSerialPortServer;

    BluetoothSerialPortServer.prototype.listen = function (successCallback, errorCallback, options) {
        var _options = {};

        if(errorCallback &&
            typeof errorCallback !== 'function'){
            _options = errorCallback;
            errorCallback = null;
        }

        if(successCallback &&
            typeof successCallback !== 'function'){
            if(errorCallback)
            errorCallback("successCallback must be a function!");
            else
            this.emit("failure", "successCallback must be a function!");

            return;
        }

        options = options || _options;
        options.uuid = options.uuid || _SERIAL_PORT_PROFILE_UUID;
        options.channel = options.channel || _DEFAULT_SERVER_CHANNEL;

        var self = this;
        var read = function () {
            process.nextTick(function(){
                if (self.server) {
                    self.server.read(function (err, buffer) {
                        if (!err && buffer) {
                            self.emit('data', buffer);
                            read();
                        }else if (self.inDisconnect) {
                            // We were told to disconnect, and now we've disconnected, so emit disconnected
                            self.inDisconnect = false;
                            self.emit('disconnected');
                        }else if(err != _ERROR_CLIENT_CLOSED_CONNECTION){
                            self.emit('failure', err);
                        }else{
                            // The client closed the connection, this is not a failure
                            // so we trigger the event but the RFCOMM socket still can
                            // receive new connections
                            self.emit('closed');
                        }
                    });
                }
            });
        };
        self.server = new btSerial.BTSerialPortBindingServer(function (clientAddress) {
            read();
            successCallback(clientAddress);
        }, function (err) {
            // cleaning up the the failed connection
            if(this.server)
                this.server.close();
            if (typeof errorCallback === 'function') {
                errorCallback(err);
            }
        }, options);

    };

    BluetoothSerialPortServer.prototype.write = function (buffer, callback) {
        if (this.server) {
            this.server.write(buffer, callback);
        } else {
            callback(new Error("Not connected"));
        }
    };

    BluetoothSerialPortServer.prototype.disconnectClient = function() {
        if (this.server) {
            this.inDisconnect = true;
            this.server.disconnectClient();
        }
    };

    BluetoothSerialPortServer.prototype.close = function () {
        if (this.server) {
            this.server.close();
            this.server = undefined;
        }
    };

    BluetoothSerialPortServer.prototype.isOpen = function () {
        if (this.server) {
            return this.server.isOpen();
        } else {
            return false;
        }
    };
}());
