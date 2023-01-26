## RELEASE NOTES

### 3.0.1

* Fixes typescript definition for `listPairedDevices`.

### 3.0.0

* Fixes building issues with recent version of NodeJS.
* Drops support for Darwin / MacOS for the moment. If you want to use the module on a Mac please install v2.2.7.

### 2.2.7

* Changes node version to `lts` in the Dockerfile

### 2.2.6

* Adds support for node v12
* Node v6 and v7 are no longer supported

### 2.2.5

* Fixes issue #97 where the server process does not exit correctly.

### 2.2.4

* Fixes an error where the module would not return correctly when parameter assertion would fail.

### 2.2.3

* Fixes regression when closing a connection

### 2.2.2

* Adds the status code to the error message when a connection fails
* Fixes typescript example

### 2.2.1

* Fixes regression

### 2.2.0

* Adds support for multiple serial port servers [#197](https://github.com/eelcocramer/node-bluetooth-serial-port/pull/197)

### 2.1.8

* Fixes issue #193 where, on Linux, not all bytes where written to the connection. In this fix the Linux implementation now mimics the other implementations.

### 2.1.7

* Adds support for node v10

### 2.1.6

* Fixes an issue on Windows and Linux where `close` would not terminate a connection properly.
* Improves the README.

### 2.1.5

* Fixes an issue in the typescript definitions.
* Fixes an issue where, on Windows, `listPairedDevices` is showing unpaired devices.

### 2.1.4

* Removes erroneous print statement
* Adds `on` to the TypeScript declaration file

### 2.1.3

* Implements `listPairedDevices` on Windows
* Fixes a memory leak that occurs when trying to reconnect to a device repeatedly
* Adds support for TypeScript
* Removes support for node v0.10, v0.12 and iojs
* Adds support for node v7

### 2.1.2

* `listen()` method will not exit until there's an explicit call to `close()`.
* If a client disconnects, `listen()` can still handle new connections.
* Calling `listen()` more than once is not allowed.
* Better error handling in the server part.
* New server and client test in the experiments folder

### 2.1.1

* Fixes a segmentation fault that can occur on OSX

### 2.1.0

* Adds asynchronous searching for devices
* Asynchronous searching replaces the old synchronous `inquire` function so without touching your code you will be using the asynchronous search function.
* Moves synchronous searching for devices to a new `inquireSync` function

### 2.0.0

* Improvements and API changes of the experimental RFCOMM server socket (Linux only)
* No changes in the client

### 1.3.1

* Fixes compilation problem on Windows.
* Adds experimental RFCOMM server socket (Linux only)

### 1.2.6

* Fixes a critical [runtime error](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/83) on all platforms.
* Improves travis build.

### 1.2.5

* Compatible to NodeJS 4.x
* Dockerfile to test build Linux from OSX or Win

### 1.2.4

* The release notes for previous version have moved to [a separate file](RELEASE_NOTES.md).
* Fixes an issue where the `Error` object is passed in the 2nd argument of the callback function when a `write` is called before the remote device was connected.
* Fixes a [segfault on OSX](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/74).
* Fixes a [buffer overflow on OSX](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/76).

### 1.2.3

* [Updates nan](https://github.com/eelcocramer/node-bluetooth-serial-port/pull/67) for io.js 2.x.x compatibility.

### 1.2.2

* Adds [cross-platform continues integration](https://github.com/eelcocramer/node-bluetooth-serial-port/pull/58) to the repository. Thanks @mackwic.
* [Re-enables windows](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/53) support.
* Fixes an issue where [pipes and file descriptors are leaked](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/57) when a connection could not be established.

### 1.2.1

* Fixes issues compile issues on OS X and Linux when using newer versions of [nan](https://github.com/rvagg/nan).

### 1.2.0

> *PLEASE NOTE* This release is not yet available for the Windows platform because the [compilation needs to be verified first](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/53). As there are no major functional changes this release and the release, hopefully, will fix major issues on OSX I decided to go forward without Windows support for the moment. This will hopefully be fixed in the next release. I will update as soon as possible.

* Fixes an [issue on OSX](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/46) where multiple reads would result into a corrupted read buffer.
* [Improves](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/51) the implementation of the Bluetooth worker on OSX.
* Better [performance](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/35) while writing to the Bluetooth connection on OSX.
* Keeps the [reader loop from reading from a closed connection]((https://github.com/eelcocramer/node-bluetooth-serial-port/issues/47).
* Will work on both node v0.8.x, v0.10.x and node v0.11.x on OSX and Linux (Windows to be done).
* When trying to write to closed connection the `write` function will not throw an exception anymore but will call the callback as per documentation.
* Adds a `closed` event that fires when a connection is closed either by the user or remotely.

### 1.1.4

* Fixes an compile issue on Windows.

### 1.1.3

* Fixes [segfault](https://github.com/eelcocramer/node-bluetooth-serial-port/pull/29) that occurs when a buffer is invalidated by the garbage collector.
* Adds experimental support for listing paired devices.

### 1.1.2

* Updates the documentation to reflect the changes made in version 1.1.0.

### 1.1.1

* Fixes typo in readme.

### 1.1.0

* Fixes [buffer overflow on close()](https://github.com/eelcocramer/node-bluetooth-serial-port/pull/26) in Mac OSX.
* Adds failure callback that is called when no channel can be found.
* Fixes an [issue on Mac OSX](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/23) where a write action would fail when the MTU was exceeded.
* Fixes an [issue on Mac OSX](https://github.com/eelcocramer/node-bluetooth-serial-port/issues/24) where data would not be written asynchronously.

### 1.0.5

* Updates the code example in the README.
* Adds `win32` to the supported OS'es in the `package.json`.

### 1.0.4

* Added windows support.

### 1.0.3

* Fixes an [issue on Linux](https://github.com/eelcocramer/node-bluetooth-serial-port/pull/11) where reading from a closed or reset connection would result into a SEGFAULT.

### 1.0.2

* Updates the documentation.
* Fixes an issue where memory is freed incorrectly after closing a connection on OS X.
* Improves the timeout mechanism that is used for getting the Bluetooth service records on a remote device on OS X.

### 1.0.1

* No code changes, only updates the documentation.

### 1.0.0

* Makes the write function asynchrone.
* Takes a [Buffer](http://nodejs.org/api/buffer.html) as the input for the write function in favor of a String.
* Reads data into a [Buffer](http://nodejs.org/api/buffer.html) object instead of using a String.
* Improves error handling when calling the native addon.

### 0.2.1

* Fixes issue where calling `close` on a connection would result in an `Abort trap: 6` error on OS X.

### 0.2.0

* Experimental support for OS X!
* `findSerialPortChannel` does not invoke callback anymore when no channel was found.
* `found` event now emits the Bluetooth address as the value of the name parameter `name` when the name of the device could not be determined (used to be `[undefined]`).


