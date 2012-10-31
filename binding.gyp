{
  'targets': 
  [
    {
     # Needed declarations for the target
     'target_name': 'bt-serial-port-binding',
     'conditions': [
        [ 'OS=="freebsd" or OS=="openbsd" or OS=="solaris" or (OS=="linux")', {
          'sources': [ "src/bt-serial-port-binding.cc" ],
          'libraries': ['-lbluetooth'],
          'cflags':['-std=gnu++0x'] 
        }]
      ]
    },
    {
     # Needed declarations for the target
     'target_name': 'device-inquiry-binding',
     'conditions': [
        [ 'OS=="freebsd" or OS=="openbsd" or OS=="solaris" or (OS=="linux")', {
          'sources': [ "src/device-inquiry-binding.cc" ],
          'libraries': ['-lbluetooth'],
          'cflags':['-std=gnu++0x'] 
        }]
      ]
    }

  ] # end targets
}

