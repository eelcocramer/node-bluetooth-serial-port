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
    	this.buffer = new Array();

    }
    
    util.inherits(RFCOMM, EventEmitter);
    exports.RFCOMM = RFCOMM;

    RFCOMM.prototype.connect = function(address, successCallback, errorCallback) {
        var self = this;
        
        this.connection = new rfcomm.RFCOMMBinding(address, function() {
            setInterval(eventLoop, 100, self);
            successCallback();
        }, errorCallback);
    }
    
    RFCOMM.prototype.write = function(data) {
        this.buffer.push(data);
    }
    
    var eventLoop = function(self) {
        if (self.buffer && self.buffer.length > 0) {
            for (var i in self.buffer) {
                console.log('sending: ' + self.buffer[i]);
                self.connection.write(self.buffer[i]);
            }
            
            self.buffer = new Array();
        }

        process.nextTick(function() {
            self.connection.read(function(data) {
                if (data !== -1) {
                    self.emit('data', data);
                }
            });
        });
    }

})();
