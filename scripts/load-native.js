// scripts/load-native.js
'use strict';

var fs = require('fs');
var path = require('path');

/**
 * Loads a named native module (.node file).
 * Resolution order:
 *   1. prebuilds/{platform}-{arch}/{name}.abi{abi}.node  (bundled prebuild)
 *   2. build/Release/{name}.node                         (source-compiled release)
 *   3. build/Debug/{name}.node                           (source-compiled debug)
 *
 * @param {string} packageRoot  Absolute path to the package root directory.
 * @param {string} name         Module name without extension (e.g. 'BluetoothSerialPort').
 * @returns {object} The native binding.
 */
module.exports = function loadNative(packageRoot, name) {
  var abi = process.versions.modules;
  var platform = process.platform;
  var arch = process.arch;

  var candidates = [
    path.join(packageRoot, 'prebuilds', platform + '-' + arch, name + '.abi' + abi + '.node'),
    path.join(packageRoot, 'build', 'Release', name + '.node'),
    path.join(packageRoot, 'build', 'Debug', name + '.node')
  ];

  for (var i = 0; i < candidates.length; i++) {
    if (fs.existsSync(candidates[i])) {
      return require(candidates[i]);
    }
  }

  throw new Error(
    'Could not find native module "' + name + '". Searched:\n' +
    candidates.map(function(p) { return '  ' + p; }).join('\n')
  );
};
