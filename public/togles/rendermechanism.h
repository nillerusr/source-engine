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
#ifndef RENDERMECHANISM_H
#define RENDERMECHANISM_H

#if defined(DX_TO_GL_ABSTRACTION)

#undef PROTECTED_THINGS_ENABLE

#include <GL/gl.h>
#include <GL/glext.h>

#include "tier0/basetypes.h"
#include "tier0/platform.h"

#include "togles/linuxwin/glmdebug.h"
#include "togles/linuxwin/glbase.h"
#include "togles/linuxwin/glentrypoints.h"
#include "togles/linuxwin/glmdisplay.h"
#include "togles/linuxwin/glmdisplaydb.h"
#include "togles/linuxwin/glmgrbasics.h"
#include "togles/linuxwin/glmgrext.h"
#include "togles/linuxwin/cglmbuffer.h"
#include "togles/linuxwin/cglmtex.h"
#include "togles/linuxwin/cglmfbo.h"
#include "togles/linuxwin/cglmprogram.h"
#include "togles/linuxwin/cglmquery.h"
#include "togles/linuxwin/glmgr.h"
#include "togles/linuxwin/dxabstract_types.h"
#include "togles/linuxwin/dxabstract.h"

#else
	//USE_ACTUAL_DX
	#ifdef WIN32
		#ifdef _X360
			#include "d3d9.h"
			#include "d3dx9.h"
		#else
			#include <windows.h>
			#include "../../dx9sdk/include/d3d9.h"
			#include "../../dx9sdk/include/d3dx9.h"
		#endif
		typedef HWND VD3DHWND;
	#endif

	#define	GLMPRINTF(args)	
	#define	GLMPRINTSTR(args)
	#define	GLMPRINTTEXT(args)
#endif // defined(DX_TO_GL_ABSTRACTION)

#endif // RENDERMECHANISM_H
