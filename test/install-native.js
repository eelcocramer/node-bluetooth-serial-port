var assert = require("assert");
var installNative = require("../scripts/install-native.js");

assert.equal(installNative.isSupportedTarget("linux", "x64", 18), true);
assert.equal(installNative.isSupportedTarget("linux", "x64", 24), true);
assert.equal(installNative.isSupportedTarget("win32", "x64", 22), true);

assert.equal(installNative.isSupportedTarget("linux", "arm64", 22), false);
assert.equal(installNative.isSupportedTarget("darwin", "x64", 22), false);
assert.equal(installNative.isSupportedTarget("win32", "x64", 16), false);

console.log("install-native tests passed");
