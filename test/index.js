var bt = require('../lib/bluetooth-serial-port.js');
var Bt = new bt.BluetoothSerialPort();

console.log('Checking client...');

[
    'inquire', 'findSerialPortChannel', 'connect', 'write', 'on', 'close'
].forEach(function(fun) {
    if (typeof Bt[fun] !== 'function')
        throw new Error("Assert failed: " + fun +
                        "should be a function but is " + (typeof Bt[fun]));
});

console.log('Ok!');

if (process.platform === 'linux') {
    console.log('Checking server...');

    var ServerBt = new bt.BluetoothSerialPortServer();
    [
        'listen', 'write', 'on', 'close'
    ].forEach(function(fun) {
        if (typeof ServerBt[fun] !== 'function')
            throw new Error("Assert failed: " + fun +
                            "should be a function but is " + (typeof ServerBt[fun]));
    });

    console.log('Ok!');
}

process.exit(0);

