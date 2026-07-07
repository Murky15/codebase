// This module defines the Window_Type used by Windows/X11/etc.
// You only need to include it if you want to define Window_Type and
// you are not using one of the Windows-type modules directly.

// NOTE: Custom module to add WASM support and circumvent a poor include of Window_Type in GetRect

#if OS == .WINDOWS {
  #scope_module;
  Windows :: #import "Windows";
  #scope_export;
  Window_Type :: Windows.HWND;
  INVALID_WINDOW : Window_Type : null;
} else #if OS == .LINUX {
  #scope_module;
  X11 :: #import "X11";
  #scope_export;
  Window_Type : : X11.Window;
  INVALID_WINDOW : Window_Type : 0; // @Incomplete: Check if this is a valid window ID on linux. (It might be…) If it is, we need to change this default.
} else #if OS == .MACOS {
  #scope_module;
  ObjC :: #import "Objective_C/AppKit";
  #scope_export;
  Window_Type :: *ObjC.NSWindow;
  INVALID_WINDOW : Window_Type : null;
} else #if OS == .PS5 {
  #scope_export;
  Window_Type :: *void;
  INVALID_WINDOW : Window_Type : null;
} else #if OS == .NN_SWITCH2 {
  #scope_export;
  Window_Type :: *void;
  INVALID_WINDOW : Window_Type : null;
} else #if OS == .ANDROID {
  #scope_module;
  Android :: #import "Android";
  #scope_export;
  Window_Type :: *Android.ANativeWindow;
  INVALID_WINDOW : Window_Type : null;
} else #if OS == .WASM {
  Window_Type :: bool;
  INVALID_WINDOW : Window_Type : false;
}

