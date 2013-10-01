#ifndef NODE_BTSP_SRC_BLUETOOTH_HELPERS_H
#define NODE_BTSP_SRC_BLUETOOTH_HELPERS_H

#include <winsock2.h>
#include <ws2bth.h>

class BluetoothHelpers {
    public:
        static bool Initialize();
        static void Finalize();
};

#endif
