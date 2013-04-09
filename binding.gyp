{
  'targets': 
  [
    {
     # Needed declarations for the target
     'target_name': 'BluetoothSerialPort',
     'conditions': [
        [ 'OS=="freebsd" or OS=="openbsd" or OS=="solaris" or (OS=="linux")', {
          'sources': [ "src/BluetoothSerialPort.cc", "src/DeviceINQ.cc", "src/BTSerialPortBinding.cc" ],
          'libraries': ['-lbluetooth'],
          'cflags':['-std=gnu++0x'] 
        }]
      ]
    }

  ] # end targets
}

