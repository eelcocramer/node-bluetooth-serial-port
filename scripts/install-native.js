// scripts/install-native.js
'use strict';

var cp = require('child_process');
var fs = require('fs');
var path = require('path');

var rootDir = path.join(__dirname, '..');
var abi = process.versions.modules;
var platform = process.platform;
var arch = process.arch;
var prebuildDir = path.join(rootDir, 'prebuilds', platform + '-' + arch);

var moduleNames = ['BluetoothSerialPort'];
if (platform === 'linux') {
  moduleNames.push('BluetoothSerialPortServer');
}

function hasAllPrebuilds() {
  for (var i = 0; i < moduleNames.length; i++) {
    var filename = moduleNames[i] + '.abi' + abi + '.node';
    if (!fs.existsSync(path.join(prebuildDir, filename))) {
      return false;
    }
  }
  return true;
}

if (hasAllPrebuilds()) {
  console.log('Using bundled prebuilds from ' + path.relative(rootDir, prebuildDir));
  process.exit(0);
}

console.log('No matching prebuilds found for ' + platform + '-' + arch + ' abi' + abi + '; building from source...');
var bin = process.platform === 'win32' ? 'node-gyp.cmd' : 'node-gyp';
var result = cp.spawnSync(bin, ['rebuild'], {
  stdio: 'inherit',
  shell: process.platform === 'win32',
  windowsHide: true
});

if (result.error) {
  throw result.error;
}
process.exit(result.status || 0);
