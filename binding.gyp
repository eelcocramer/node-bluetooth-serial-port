{
  'targets':
  [
    {
     # Needed declarations for the target
     'target_name': 'BluetoothSerialPort',
     'conditions': [
        [ 'OS=="freebsd" or OS=="openbsd" or OS=="solaris" or (OS=="linux")', {
          'sources': [ 'src/linux/BluetoothSerialPort.cc', 'src/linux/DeviceINQ.cc', 'src/linux/BTSerialPortBinding.cc' ],
          'include_dirs' : [ "<!(node -e \"require('nan')\")", 'src' ],
          'libraries': ['-lbluetooth'],
          'cflags':['-std=c++11']
        }],
        [ 'OS=="mac"', {
          'sources': ['src/macos/DeviceINQ.mm', 'src/macos/BluetoothWorker.mm', 'src/macos/pipe.c', 'src/macos/BluetoothDeviceResources.mm', 'src/macos/BluetoothSerialPort.mm', 'src/macos/BTSerialPortBinding.mm'],
          'include_dirs' : [ "<!(node -e \"require('nan')\")", 'src', 'src/macos' ],
          'libraries':['-framework Foundation', '-framework IOBluetooth', '-fobjc-arc'],
          'cflags!': [ '-fno-exceptions' ],
          'cflags_cc!': [ '-fno-exceptions' ],
          'xcode_settings': { 'GCC_ENABLE_CPP_EXCEPTIONS': 'YES', 'OTHER_CFLAGS': [ '-Wno-unused-function' ] }
        }],
        [ 'OS=="win"', {
          'sources': [ 'src/windows/BluetoothSerialPort.cc', 'src/windows/DeviceINQ.cc', 'src/windows/BTSerialPortBinding.cc', 'src/windows/BluetoothHelpers.cc' ],
          'include_dirs' : [ "<!(node -e \"require('nan')\")", 'src', 'src/windows' ],
          'libraries': [ '-lkernel32.lib', '-luser32.lib', '-lWs2_32.lib' ]
        }],
      ]
    },
    {
     # Needed declarations for the target
     'target_name': 'BluetoothSerialPortServer',
     'conditions': [
        [ 'OS=="freebsd" or OS=="openbsd" or OS=="solaris" or (OS=="linux")', {
          'sources': [ 'src/linux/BluetoothSerialPortServer.cc', 'src/linux/BTSerialPortBindingServer.cc'],
          'include_dirs' : [ "<!(node -e \"require('nan')\")", 'src' ],
          'libraries': ['-lbluetooth'],
          'cflags':['-std=gnu++0x']
        }],
      ]
    }
  ] # end targets
}

