const BluetoothSerialPort = require('../lib/bluetooth-serial-port');
const rfcomm = new BluetoothSerialPort.BluetoothSerialPort();
rfcomm.listPairedDevices(function (list) {
		console.log(JSON.stringify(list,null,2));
});