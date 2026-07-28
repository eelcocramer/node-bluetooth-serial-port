// scripts/create-prebuilds.js
'use strict';

var cp = require('child_process');
var fs = require('fs');
var path = require('path');

var strip = process.argv.includes('--strip');
var abi = process.versions.modules;
var platform = process.platform;
var arch = process.arch;
var rootDir = path.join(__dirname, '..');
var releaseDir = path.join(rootDir, 'build', 'Release');
var outDir = path.join(rootDir, 'prebuilds', platform + '-' + arch);
var modules = ['BluetoothSerialPort'];
if (platform === 'linux') {
  modules.push('BluetoothSerialPortServer');
}

fs.mkdirSync(outDir, { recursive: true });

modules.forEach(function(name) {
  var src = path.join(releaseDir, name + '.node');

  if (!fs.existsSync(src)) {
    console.error('Error: ' + src + ' not found. Run "npm run install-release" first.');
    process.exit(1);
  }

  var dest = path.join(outDir, name + '.abi' + abi + '.node');
  fs.copyFileSync(src, dest);

  if (strip && platform !== 'win32') {
    try {
      cp.execSync('strip "' + dest + '"', { stdio: 'inherit' });
    } catch (e) {
      console.warn('Warning: strip failed for ' + dest + ' (' + e.message + ')');
    }
  }

  console.log('Created ' + path.relative(rootDir, dest));
});
