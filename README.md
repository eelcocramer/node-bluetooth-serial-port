# Bluetooth serial port communication for Node.js

## Consider a donation

This piece of open source software is free (as in speech) to [use, modify and distribute](LICENSE.md), no strings attached. If this software saves you some cash and you want to do something for the good please consider making a donation to a [charity](https://www.doctorswithoutborders.org). Any charity will do. 🙏

> MacOS support has (temporarily) been dropped as of version v3.0.0 of the module. If you use the module on a Mac please stay on v2 of this module.

> _DEPRECATED_ Currently I have no plans to add support for NodeJS version 1.13 and up. If you want to help out by adding support please contact me. It was a great experience maintaining this project for almost 8 years. It was great to see people step in to improve this project. Thank you all!

[![Build status](https://dl.circleci.com/status-badge/img/gh/eelcocramer/node-bluetooth-serial-port/tree/master.svg?style=svg)](https://dl.circleci.com/status-badge/redirect/gh/eelcocramer/node-bluetooth-serial-port/tree/master)
[![Build status](https://ci.appveyor.com/api/projects/status/4p1r3ddoid98qc7k?svg=true)](https://ci.appveyor.com/project/eelcocramer/node-bluetooth-serial-port)

This node module lets you communicate over Bluetooth serial port with devices using NodeJS. The goal is have an easy to use API. This module is great for communicating with Bluetooth enabled Arduino devices.

If you have any problems make sure to [checkout the FAQ](https://github.com/eelcocramer/node-bluetooth-serial-port/issues?q=label%3AFAQ).

## New in the last release

-   Changes node version to `lts` in the Dockerfile

Check the [release notes](RELEASE_NOTES.md) for an overview of the change history.

## Prerequisites on Linux

-   Needs Bluetooth development packages to build

`apt-get install build-essential libbluetooth-dev`

### Note on RFCOMM Server Sockets

As the initial implementation of the RFCOMM server sockets is based on BlueZ4, in order to work with SDP we need to change the bluetooth service configuration file by modifing the systemd unit file: bluetooth.service:

(Debian based distro)

`sudo vim /lib/systemd/system/bluetooth.service`

(RedHat based distro)

`sudo vim /usr/lib/systemd/system/bluetooth.service`

and adding the --compat flag to the ExecStart value:

`ExecStart=/usr/lib/bluetooth/bluetoothd `**`--compat`**

Finally, restart the service:

```bash
sudo systemctl daemon-reload
sudo systemctl restart bluetooth
```

## Prerequisites on Windows

-   Needs Visual Studio (Visual C++) and its command line tools installed.
-   Needs Python 2.x installed and accessible from the command line path.

## Install

`npm install bluetooth-serial-port`

## Test build Linux using docker

`docker build -t bluetooth-serial-port .`

# Documentation

## Basic client usage

```javascript
var btSerial = new (require("bluetooth-serial-port").BluetoothSerialPort)();

btSerial.on("found", function (address, name) {
    btSerial.findSerialPortChannel(
        address,
        function (channel) {
            btSerial.connect(
                address,
                channel,
                function () {
                    console.log("connected");

                    btSerial.write(
                        Buffer.from("my data", "utf-8"),
                        function (err, bytesWritten) {
                            if (err) console.log(err);
                        }
                    );

                    btSerial.on("data", function (buffer) {
                        console.log(buffer.toString("utf-8"));
                    });
                },
                function () {
                    console.log("cannot connect");
                }
            );

            // close the connection when you're ready
            btSerial.close();
        },
        function () {
            console.log("found nothing");
        }
    );
});

btSerial.inquire();
```

## Basic server usage (only on Linux)

```javascript
var server = new (require("bluetooth-serial-port").BluetoothSerialPortServer)();

var CHANNEL = 10; // My service channel. Defaults to 1 if omitted.
var UUID = "38e851bc-7144-44b4-9cd8-80549c6f2912"; // My own service UUID. Defaults to '1101' if omitted

server.on("data", function (buffer) {
    console.log("Received data from client: " + buffer);

    // ...

    console.log("Sending data to the client");
    server.write(Buffer.from("..."), function (err, bytesWritten) {
        if (err) {
            console.log("Error!");
        } else {
            console.log("Send " + bytesWritten + " to the client!");
        }
    });
});

server.listen(
    function (clientAddress) {
        console.log("Client: " + clientAddress + " connected!");
    },
    function (error) {
        console.error("Something wrong happened!:" + error);
    },
    { uuid: UUID, channel: CHANNEL }
);
```

## API

### BluetoothSerialPort

#### Event: ('data', buffer)

Emitted when data is read from the serial port connection.

-   buffer - the data that was read into a [Buffer](http://nodejs.org/api/buffer.html) object.

#### Event: ('closed')

Emitted when a connection was closed either by the user (i.e. calling `close` or remotely).

#### Event: ('failure', err)

Emitted when reading from the serial port connection results in an error. The connection is closed.

-   err - an [Error object](http://docs.nodejitsu.com/articles/errors/what-is-the-error-object) describing the failure.

#### Event: ('found', address, name)

Emitted when a bluetooth device was found.

-   address - the address of the device
-   name - the name of the device (or the address if the name is unavailable)

#### Event: ('finished')

Emitted when the device inquiry execution did finish.

#### BluetoothSerialPort.inquire()

Starts searching for bluetooth devices. When a device is found a 'found' event will be emitted.

#### BluetoothSerialPort.inquireSync()

Starts searching synchronously for bluetooth devices. When a device is found a 'found' event will be emitted.

#### BluetoothSerialPort.findSerialPortChannel(address, callback[, errorCallback])

Checks if a device has a serial port service running and if it is found it passes the channel id to use for the RFCOMM connection.

-   callback(channel) - called when finished looking for a serial port on the device.
-   errorCallback - called the search finished but no serial port channel was found on the device.
    Connects to a remote bluetooth device.

-   bluetoothAddress - the address of the remote Bluetooth device.
-   channel - the channel to connect to.
-   [successCallback] - called when a connection has been established.
-   [errorCallback(err)] - called when the connection attempt results in an error. The parameter is an [Error object](http://docs.nodejitsu.com/articles/errors/what-is-the-error-object).

#### BluetoothSerialPort.close()

Closes the connection.

#### BluetoothSerialPort.isOpen()

Check whether the connection is open or not.

#### BluetoothSerialPort.write(buffer, callback)

Writes a [Buffer](http://nodejs.org/api/buffer.html) to the serial port connection.

-   buffer - the [Buffer](http://nodejs.org/api/buffer.html) to be written.
-   callback(err, bytesWritten) - is called when the write action has been completed. When the `err` parameter is set an error has occured, in that case `err` is an [Error object](http://docs.nodejitsu.com/articles/errors/what-is-the-error-object). When `err` is not set the write action was successful and `bytesWritten` contains the amount of bytes that is written to the connection.

#### BluetoothSerialPort.listPairedDevices(callback)

**NOT AVAILABLE ON LINUX**

Lists the devices that are currently paired with the host.

-   callback(pairedDevices) - is called when the paired devices object has been populated. See the [pull request](https://github.com/eelcocramer/node-bluetooth-serial-port/pull/30) for more information on the `pairedDevices` object.

### BluetoothSerialPortServer

#### BluetoothSerialPortServer.listen(callback[, errorCallback, options])

Listens for an incoming bluetooth connection. It will automatically advertise the server via SDP

-   callback(address) - is called when a new client is connecting.
-   errorCallback(err) - is called when an error occurs.
-   options - An object with these properties:

    -   uuid - [String] The UUID of the server. If omitted the default value will be 1101 (corresponding to Serial Port Profile UUID). Can be a 16 bit or 32 bit UUID.
    -   channel - [Number] The RFCOMM channel the server is listening on, in the range of 1-30. If omitted the default value will be 1.

        Example:
        `var options = { uuid: 'ffffffff-ffff-ffff-ffff-fffffffffff1', channel: 10 }`

#### BluetoothSerialPortServer.write(buffer, callback)

Writes data from a buffer to a connection.

-   buffer - the buffer to send over the connection.
-   callback(err, len) - called when the data is send or an error did occur. `error` contains the error is appropriated. `len` has the number of bytes that were written to the connection.

#### BluetoothSerialPortServer.close()

Stops the server.

#### BluetoothSerialPortServer.disconnectClient()

Disconnects the currently-connected client and re-listens and re-publishes to SDP.

#### BluetoothSerialPortServer.isOpen()

Checks is a server is listening or not.

#### Event: ('data', buffer)

Emitted when data is read from the serial port connection.

-   buffer - the data that was read into a [Buffer](http://nodejs.org/api/buffer.html) object.

#### Event: ('disconnected')

Emitted when a connection was disconnected (i.e. from calling `disconnectClient` or if the bluetooth device disconnects (turned off or goes out of range)).

#### Event: ('closed')

Emitted when the server is closed (i.e. from calling `close` or as the result of a non-disconnect error).

#### Event: ('failure', err)

Emitted when reading from the serial port connection results in an error. The connection is closed.

-   err - an [Error object](http://docs.nodejitsu.com/articles/errors/what-is-the-error-object) describing the failure.

## Typescript support

The type script declaration file is bundled with this module.

```typescript
import * as btSerial from "bluetooth-serial-port";

btSerial.findSerialPortChannel(address: string, (channel: number) => {
    btSerial.connect(address: string, channel: number, () => {
        btSerial.write(Buffer.from("yes"), (err) => {
	    if (err) {
                console.error(err);
            }
        });
    }, (err?: Error) => {
            if (err) {
                console.error(err);
            }
        });
        btSerial.on("data", (buffer: Buffer) => console.log(buffer.toString("ascii")));
}, () => {
        console.error("Cannot find channel!");
});
```

## LICENSE

This module is available under a [FreeBSD license](http://opensource.org/licenses/BSD-2-Clause), see the [LICENSE file](https://github.com/eelcocramer/node-bluetooth-serial-port/blob/master/LICENSE.md) for details.
