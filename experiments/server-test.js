/*
 * Copyright (c) 2016 - Juan Gómez Mosquera
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
    var BluetoothSerialPortServer = require("../lib/bluetooth-serial-port.js").BluetoothSerialPortServer;
    var server = new BluetoothSerialPortServer();

    const CHANNEL = 10;

    server.on('data', function(buffer){
        console.log("Received data from client: " + buffer);
        if(buffer == "END"){
            console.log("Finishing connection!");
            server.close();
            return;
        }

        var buf = new Buffer("PONG");
        console.log("Sending a PONG to the client...");
            server.write(buf, function(error, bytesWritten){
            if(error){
                console.error("Something went wrong sending the PONG message: bytesWritten = " + error);
            }else{
                console.log("PONG sent! (" + bytesWritten + " bytes)");
            }
        });
    });

    server.on('closed', function(){
      console.log("Client closed the connection!");
    });

    server.on('failure', function(err){
      console.log("Something wrong happened!: " + err);
    });

    server.listen(function(clientAddress){
        console.log("Client: " + clientAddress + " connected!");
    }, function(error){
        console.log("Something wrong happened while setting up the server for listening: error = " + error);
    }, { /*uuid: UUID,*/ channel: CHANNEL });

})();
