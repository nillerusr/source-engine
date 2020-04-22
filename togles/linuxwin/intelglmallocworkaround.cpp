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
#include "intelglmallocworkaround.h"
#include "mach_override.h"

// memdbgon -must- be the last include file in a .cpp file.
#include "tier0/memdbgon.h"

IntelGLMallocWorkaround* IntelGLMallocWorkaround::s_pWorkaround = NULL;

void *IntelGLMallocWorkaround::ZeroingAlloc(size_t size)
{
	// We call into this pointer that resumes the original malloc.
	void *memory = s_pWorkaround->m_pfnMallocReentry(size);
	if (size < 96)
	{
		// Since the Intel driver has an issue with a small allocation 
		// that's left uninitialized, we use memset to ensure it's zero-initialized.
		memset(memory, 0, size);
	}

	return memory;
}

IntelGLMallocWorkaround* IntelGLMallocWorkaround::Get()
{
	if (!s_pWorkaround)
	{
		s_pWorkaround = new IntelGLMallocWorkaround();
	}

	return s_pWorkaround;
}

bool IntelGLMallocWorkaround::Enable()
{
	if ( m_pfnMallocReentry != NULL )
	{
		return true;
	}

	mach_error_t error = mach_override_ptr( (void*)&malloc, (const void*)&ZeroingAlloc, (void**)&m_pfnMallocReentry );
	if ( error == err_cannot_override )
	{
		m_pfnMallocReentry = NULL;
		return false;
	}

	return true;
}