{
  'targets': 
  [
    {
     # Needed declarations for the target
     'target_name': 'BluetoothSerialPort',
     'conditions': [
        [ 'OS=="freebsd" or OS=="openbsd" or OS=="solaris" or (OS=="linux")', {
          'sources': [ 'src/linux/BluetoothSerialPort.cc', 'src/linux/DeviceINQ.cc', 'src/linux/BTSerialPortBinding.cc' ],
          'include_dirs' : [ 'src' ],
          'libraries': ['-lbluetooth'],
          'cflags':['-std=gnu++0x'] 
        }],
        [ 'OS=="mac"', {
          'sources': ['src/osx/DeviceINQ.mm', 'src/osx/BluetoothWorker.mm', 'src/osx/pipe.c', 'src/osx/BluetoothDeviceResources.mm', 'src/osx/BluetoothSerialPort.mm', 'src/osx/BTSerialPortBinding.mm'],
          'include_dirs' : [ 'src', 'src/osx' ],
          'libraries':['-framework Foundation', '-framework IOBluetooth', '-fobjc-arc'],
          'cflags!': [ '-fno-exceptions' ],
          'cflags_cc!': [ '-fno-exceptions' ],
          'xcode_settings': { 'GCC_ENABLE_CPP_EXCEPTIONS': 'YES' }
        }],
        [ 'OS=="win"', {
          'sources': [ 'src/windows/BluetoothSerialPort.cc', 'src/windows/DeviceINQ.cc', 'src/windows/BTSerialPortBinding.cc', 'src/windows/BluetoothHelpers.cc' ],
          'include_dirs' : [ 'src', 'src/windows' ],
          'libraries': [ '-lkernel32.lib', '-luser32.lib', '-lWs2_32.lib' ]
        }],
      ]
    }

  ] # end targets
}

