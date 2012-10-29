var $ = require('NodObjC'),
    AppDelegate = require('./AppDelegate.js');

var a = new Date();
$.import('Cocoa');
console.log(new Date() - a); // 3075

var b = new Date();
var pool = $.NSAutoreleasePool('alloc')('init'),
    app  = $.NSApplication('sharedApplication');
console.log(new Date() - b); // 69

var c = new Date();
app('setActivationPolicy', $.NSApplicationActivationPolicyRegular);
console.log(new Date() - c); // 2

var d = new Date();
var menuBar = $.NSMenu('alloc')('init'),
    appMenuItem = $.NSMenuItem('alloc')('init');
console.log(new Date() - d); // 1

var e = new Date();
menuBar('addItem', appMenuItem);
app('setMainMenu', menuBar);
console.log(new Date() - e); // 2

var f = new Date();
var appMenu = $.NSMenu('alloc')('init'),
    appName = $('Hello NodeJS!'),
    quitTitle = $('Quit "' + appName + '"'),
    quitMenuItem = $.NSMenuItem('alloc')('initWithTitle', quitTitle,
                                                'action', 'terminate:',
                                         'keyEquivalent', $('q'));
console.log(new Date() - f); // 2

var g = new Date();
appMenu('addItem', quitMenuItem);
appMenuItem('setSubmenu', appMenu);
console.log(new Date() - g); // 1

var h = new Date();
var styleMask = $.NSTitledWindowMask | $.NSResizableWindowMask | $.NSClosableWindowMask;
var window = $.NSWindow('alloc')('initWithContentRect', $.NSMakeRect(0,0,200,200),
                                           'styleMask', styleMask,
                                             'backing', $.NSBackingStoreBuffered, 
                                               'defer', false);
console.log(new Date() - h); // 12


var i = new Date();
window('cascadeTopLeftFromPoint', $.NSMakePoint(20,20));
window('setTitle', appName);
window('makeKeyAndOrderFront', window);
window('center');
console.log(new Date() - i); // 70

var j = new Date();
AppDelegate = AppDelegate($.NSObject.extend('AppDelegate'));
app('setDelegate', AppDelegate);
app('activateIgnoringOtherApps', true);
console.log(new Date() - j); // 3
app('run');

