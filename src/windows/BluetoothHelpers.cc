#include "BluetoothHelpers.h"

bool BluetoothHelpers::Initialize() {
    WSADATA data;
    int startupError = WSAStartup(MAKEWORD(2, 2), &data);
    bool initializationSuccess = startupError == 0;
    if (initializationSuccess) {
        initializationSuccess = LOBYTE(data.wVersion) == 2 && HIBYTE(data.wVersion) == 2;
        if (!initializationSuccess) {
            BluetoothHelpers::Finalize();
        }
    }

    return initializationSuccess;
}

void BluetoothHelpers::Finalize() {
    WSACleanup();
}