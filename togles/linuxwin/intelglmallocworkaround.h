//========= Copyright Valve Corporation, All rights reserved. ============//
//                       TOGL CODE LICENSE
//
//  Copyright 2011-2014 Valve Corporation
//  All Rights Reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//
// intelglmallocworkaround.h
//  class responsible for setting up a malloc override that zeroes allocated 
//  memory of less than 96 bytes. this is to work around a bug
//  in the Intel GLSL compiler on Mac OS X 10.8 due to uninitialized memory.
//  
//  96 was chosen due to this quote from Apple: 
//    "I verified that the size of the structure is exactly 64 bytes on 10.8.3, 10.8.4 and will be on 10.8.5."
//
//  certain GLSL shaders would (intermittently) cause a crash the first time they
//  were drawn, and the bug has supposedly been fixed in 10.9, but is unlikely to
//  ever make it to 10.8.
//
//===============================================================================

#ifndef INTELGLMALLOCWORKAROUND_H
#define	INTELGLMALLOCWORKAROUND_H

#include <stdlib.h>

class IntelGLMallocWorkaround
{
public:
	static IntelGLMallocWorkaround *Get();
	bool Enable();

protected:
	IntelGLMallocWorkaround() :m_pfnMallocReentry(NULL) {}
	~IntelGLMallocWorkaround() {}

	static IntelGLMallocWorkaround *s_pWorkaround;
	static void* ZeroingAlloc(size_t);

	typedef void* (*pfnMalloc_t)(size_t);
	pfnMalloc_t m_pfnMallocReentry;
};

#endif // INTELGLMALLOCWORKAROUND_H