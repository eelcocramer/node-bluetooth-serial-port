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
        }]
      ]
    }

  ] # end targets
}

