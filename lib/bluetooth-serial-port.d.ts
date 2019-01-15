// Type definitions for bluetooth-serial-port v2.1.0
// Project: https://github.com/eelcocramer/node-bluetooth-serial-port
// Definitions by: Sand Angel <https://github.com/sandangel, DefinitelyTyped
// <https://github.com/DefinitelyTyped/DefinitelyTyped>
// Definitions: https://github.com/DefinitelyTyped/DefinitelyTyped

import { EventEmitter } from "events";

declare module BluetoothSerialPort {
  class BluetoothSerialPort extends EventEmitter {
    constructor();
    inquire(): void;
    inquireSync(): void;
    findSerialPortChannel(
        address: string, successCallback: (channel: number) => void,
        errorCallback?: () => void): void;
    connect(
        address: string, channel: number, successCallback: () => void,
        errorCallback?: (err?: Error) => void): void;
    write(buffer: Buffer, cb: (err?: Error) => void): void;
    close(): void;
    isOpen(): boolean;
    listPairedDevices(): void;
  }
  class BluetoothSerialPortServer {
    constructor();
    listen(
        successCallback: (clientAddress: string) => void,
        errorCallback?: (err: any) => void,
        options?: {uuid?: string; channel: number;}): void;
    on(event: string, callback: (arg1:any, arg2:any) => void): void;
    write(buffer: Buffer, callback: (err?: Error) => void): void;
    close(): void;
    disconnectClient(): void;
    isOpen(): boolean;
  }
}

export = BluetoothSerialPort;
