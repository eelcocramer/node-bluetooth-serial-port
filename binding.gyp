{
  'targets': 
  [
    {
     # Needed declarations for the target
     'target_name': 'rfcomm-binding',
     'conditions': [
        [ 'OS=="freebsd" or OS=="openbsd" or OS=="solaris" or (OS=="linux")', {
          'sources': [ "src/rfcomm-binding.cc" ],
          'libraries': ['-lbluetooth'],
          'cflags':['-std=gnu++0x'] 
        }]
      ]
    }

  ] # end targets
}

