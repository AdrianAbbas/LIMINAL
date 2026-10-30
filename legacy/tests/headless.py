#!/usr/bin/env python3
"""Run a JUCE GUI smoke test on an existing Xvfb DISPLAY, without a window manager.

JUCE 8.0.6 expects ICCCM window protocol atoms to exist. Real window managers
provide these; bare Xvfb does not. Keep this X connection open for the child.
"""
import ctypes
import ctypes.util
import subprocess
import sys

x11 = ctypes.CDLL(ctypes.util.find_library("X11"))
x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
x11.XInternAtom.restype = ctypes.c_ulong
x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
display = x11.XOpenDisplay(None)
if not display:
    sys.exit("No X display. Run under xvfb-run or start Xvfb first.")
try:
    for name in (b"WM_PROTOCOLS", b"WM_TAKE_FOCUS", b"WM_DELETE_WINDOW", b"_NET_WM_PING", b"WM_STATE", b"WM_CHANGE_STATE", b"_NET_WM_WINDOW_TYPE", b"_NET_WM_STATE", b"_NET_WM_STATE_HIDDEN"):
        x11.XInternAtom(display, name, 0)
    x11.XSync(display, 0)
    sys.exit(subprocess.call(sys.argv[1:]))
finally:
    x11.XCloseDisplay(display)
