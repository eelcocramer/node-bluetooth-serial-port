var assert = require("assert");
var installNative = require("../scripts/install-native.js");

assert.equal(installNative.isSupportedTarget("linux", "x64", 18), true);
assert.equal(installNative.isSupportedTarget("linux", "x64", 24), true);
assert.equal(installNative.isSupportedTarget("win32", "x64", 22), true);

assert.equal(installNative.isSupportedTarget("linux", "arm64", 22), false);
assert.equal(installNative.isSupportedTarget("darwin", "x64", 22), false);
assert.equal(installNative.isSupportedTarget("win32", "x64", 16), false);

assert.equal(installNative.exitCode({ status: 0 }), 0);
assert.equal(installNative.exitCode({ status: 1 }), 1);
assert.equal(installNative.exitCode({ status: null }), 1);

console.log("install-native tests passed");
