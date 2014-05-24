(function() {
    "use strict";

    var util = require('util');
    var DeviceINQ = require("../lib/device-inquiry.js").DeviceINQ;
    var BluetoothSerialPort = require("../lib/bluetooth-serial-port.js").BluetoothSerialPort;
    var serial = new BluetoothSerialPort();

    serial.listPairedDevices(function(pairedDevices) {
        pairedDevices.forEach(function(device) {
            console.log(device);
        });
    })
})();
