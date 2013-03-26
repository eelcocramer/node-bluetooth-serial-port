{
  'targets': 
  [
    {
     # Needed declarations for the target
     'target_name': 'BTSerialPortBinding',
     'conditions': [
        [ 'OS=="freebsd" or OS=="openbsd" or OS=="solaris" or (OS=="linux")', {
          'sources': [ "src/BTSerialPortBinding.cc" ],
          'libraries': ['-lbluetooth'],
          'cflags':['-std=gnu++0x'] 
        }]
      ]
    },
    {
     # Needed declarations for the target
     'target_name': 'DeviceINQ',
     'conditions': [
        [ 'OS=="freebsd" or OS=="openbsd" or OS=="solaris" or (OS=="linux")', {
          'sources': [ "src/DeviceINQ.cc" ],
          'libraries': ['-lbluetooth'],
          'cflags':['-std=gnu++0x'] 
        }]
      ]
    }

  ] # end targets
}

