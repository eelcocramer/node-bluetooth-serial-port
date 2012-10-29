module.exports = function (IOBluetoothDeviceInquiryDelegate) {
  // AppDelegate.addMethod('applicationDidFinishLaunching:', 'v@:@', function (self, _cmd, notif) {});
  // AppDelegate.addMethod('applicationWillTerminate:', 'v@:@', function (self, _cmd, notif) {});
  
  IOBluetoothDeviceInquiryDelegate.addMethod('deviceInquiryDeviceFound:', 'v@:@', function(self, _device, device) {
      console.log('blaat');
  });

  IOBluetoothDeviceInquiryDelegate.addMethod('deviceInquiryComplete:', 'v@:@', function(self, _error, error, _aborted, aborted)  {
      console.log('blaat');
  });

  IOBluetoothDeviceInquiryDelegate.addMethod('deviceInquiryStarted:', 'v@:@', function(self)  {
      console.log('blaat123');
  });

  
  IOBluetoothDeviceInquiryDelegate.register();
  return IOBluetoothDeviceInquiryDelegate('alloc')('init');
};

