#pragma once
/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016 D Levin
 * Benchmark source code is dual-licensed under MIT and GPL 2 or later
 * See LICENSE.txt for details
 */

#pragma once

#ifdef __MINGW32__

#include <windows.h>

extern "C" {
// int __security_cookie;
}

extern "C" void __security_check_cookie(uintptr_t i)
{
    (void)i;
    // do nothing
}
extern "C" void __fastcall __GSHandlerCheck() {}

extern "C" void __chkstk()
{
    // do nothing
}
extern "C" LONGLONG _allmul(LONGLONG x, LONGLONG y) { return x * y; }
#endif //__MINGW32__
