/*
 * win.h - Common Win32 include with version targets and fallbacks
 *         for older MinGW headers. Include this first in every GUI file.
 */
#ifndef GUI_WIN_H
#define GUI_WIN_H

#ifndef WINVER
#define WINVER 0x0501
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#include <windows.h>
#include <commctrl.h>

#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER 0x00010000
#endif
#ifndef CLEARTYPE_QUALITY
#define CLEARTYPE_QUALITY 5
#endif
#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif
#ifndef DTM_SETFORMATA
#define DTM_SETFORMATA (DTM_FIRST + 5)
#endif
#ifndef WM_MOUSELEAVE
#define WM_MOUSELEAVE 0x02A3
#endif

#endif /* GUI_WIN_H */
