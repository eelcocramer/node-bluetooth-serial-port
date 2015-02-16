var bt = require('bluetooth-serial-port');
var Bt = new bt.BluetoothSerialPort();

[
    'inquire', 'findSerialPortChannel', 'connect', 'write', 'on', 'close'
].forEach(function(fun) {
    if (typeof Bt[fun] !== 'function')
        throw new Error("Assert failed: " + fun +
                        "should be a function but is " + (typeof Bt[fun]));
});

process.exit(0);

