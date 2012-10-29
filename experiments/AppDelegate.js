module.exports = function (AppDelegate) {
  AppDelegate.addMethod('applicationDidFinishLaunching:', 'v@:@', function (self, _cmd, notif) {});
  AppDelegate.addMethod('applicationWillTerminate:', 'v@:@', function (self, _cmd, notif) {});
  AppDelegate.register();
  return AppDelegate('alloc')('init');
};

