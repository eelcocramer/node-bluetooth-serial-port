/*******************************************************************************
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* Copyright Eelco Cramer, 2012
*******************************************************************************/

(function() {
 	"use strict";

    var util = require('util');
    var EventEmitter = require('events').EventEmitter;
    var rfcomm = require("../build/Release/rfcomm-binding");

    /**
     * Creates an instance of the bluetooth-serial object.
     * @constructor
     * @name Connection
     * @param rpcHandler The RPC handler.
     */
    function RFCOMM() {
    	EventEmitter.call(this);
    }
    
    util.inherits(RFCOMM, EventEmitter);
    exports.RFCOMM = RFCOMM;

    RFCOMM.prototype.connect = function(address, successCallback, errorCallback) {
        var self = this;
        
        this.connection = new rfcomm.RFCOMMBinding(address, function(err) {
            self.buffer = new Array();
            self.eventLoop = setInterval(eventLoop, 100, self);
            successCallback();
        }, function () {
            if (errorCallback) {
                errorCallback(err);
            }
        });
    }
    
    RFCOMM.prototype.write = function(data) {
        if (this.buffer) {
            this.buffer.push(data);
        } else {
            throw('Not connected yet.');
        }
    }
    
    RFCOMM.prototype.close = function() {
        if (this.eventLoop) {
            clearInterval(this.eventLoop);
        }
        
        if (this.connection) {
            this.connection.close();
            this.connection = undefined;
        }
    }

    var eventLoop = function(self) {
        if (self.buffer && self.buffer.length > 0) {
            for (var i in self.buffer) {
                self.connection.write(self.buffer[i]);
            }
            
            self.buffer = new Array();
        }

        process.nextTick(function() {
            if (self.connection) {
                self.connection.read(function(data, size) {
                    if (size >= 0) {
                        self.emit('data', data);
                    } else {
                        self.close();
                        self.emit('error', 'Error reading from the connection. The connection is lost.');
                    }
                });
            }
        });
    }

})();
