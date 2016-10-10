const BluetoothSerialPort = require('../lib/bluetooth-serial-port');

const rfcomm = new BluetoothSerialPort.BluetoothSerialPort();

rfcomm.on('found', function (address, name) {
	console.log('found device:', name, 'with address:', address);
});

rfcomm.on('finished', function () {
  console.log('inquiry finished');
});

console.log('start inquiry');
rfcomm.inquireSync();
console.log('should be displayed after the end of inquiry');