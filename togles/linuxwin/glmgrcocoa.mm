//========= Copyright  1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: provide some call-out glue to ObjC from the C++ GLMgr code
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//


#include <Cocoa/Cocoa.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

#undef MIN
#undef MAX
#define DONT_DEFINE_BOOL	// Don't define BOOL!
#include "tier0/threadtools.h"
#include "tier1/interface.h"
#include "tier1/strtools.h"
#include "tier1/utllinkedlist.h"
#include "togl/rendermechanism.h"



// ------------------------------------------------------------------------------------ //
// some glue to let GLMgr call into NS/ObjC classes.
// ------------------------------------------------------------------------------------ //

CGLContextObj GetCGLContextFromNSGL( PseudoNSGLContextPtr nsglCtx )
{
	return (CGLContextObj)[ (NSOpenGLContext*)nsglCtx CGLContextObj];
}

