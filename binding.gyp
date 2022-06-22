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

