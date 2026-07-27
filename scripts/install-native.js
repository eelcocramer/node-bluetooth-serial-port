"use strict";

var childProcess = require("child_process");

var SUPPORTED_TARGETS = {
    linux: { x64: [18, 20, 22, 24] },
    win32: { x64: [18, 20, 22, 24] }
};

function isSupportedTarget(platform, arch, nodeMajor) {
    var platformTargets = SUPPORTED_TARGETS[platform];
    if (!platformTargets) return false;

    var archTargets = platformTargets[arch];
    if (!archTargets) return false;

    return archTargets.indexOf(nodeMajor) >= 0;
}

function run(command, args) {
    return childProcess.spawnSync(command, args, {
        stdio: "inherit",
        shell: process.platform === "win32"
    });
}

function installNative() {
    var nodeMajor = Number(process.versions.node.split(".")[0]);
    var supported = isSupportedTarget(process.platform, process.arch, nodeMajor);

    if (!supported) {
        console.log("[bluetooth-serial-port] No prebuilt binary for this target, building from source.");
        return run("node-gyp", ["configure", "build"]).status || 1;
    }

    var prebuildResult = run("prebuild-install", ["--binary-name", "BluetoothSerialPort.node", "--verbose"]);
    if (prebuildResult.status === 0) {
        return 0;
    }

    console.warn("[bluetooth-serial-port] Prebuilt download failed, building from source.");
    return run("node-gyp", ["configure", "build"]).status || 1;
}

if (require.main === module) {
    process.exit(installNative());
}

module.exports = {
    isSupportedTarget: isSupportedTarget,
    installNative: installNative
};
