//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#ifndef PCH_MATERIALSYSTEM_H
#define PCH_MATERIALSYSTEM_H

#if defined( _WIN32 )
#pragma once
#endif

#if defined( _WIN32 ) && !defined( _X360 )
#define WIN32_LEAN_AND_MEAN 1
#include "windows.h"
#endif

#include <malloc.h>
#include <string.h>
#include "crtmemdebug.h"

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier0/fasttimer.h"
#include "tier0/vprof.h"

#include "tier1/tier1.h"
#include "tier1/utlstack.h"
#include "tier1/generichash.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlrbtree.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "mathlib/vmatrix.h"
#include "icvar.h"
#include "KeyValues.h"
#include "convar.h"

#include "tier2/tier2.h"
#include "bitmap/imageformat.h"
#include "bitmap/tgawriter.h"
#include "bitmap/tgaloader.h"
#include "datacache/idatacache.h"
#include "filesystem.h"
#include "pixelwriter.h"

#include "materialsystem_global.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "materialsystem/IColorCorrection.h"

#include "imaterialinternal.h"
#include "imaterialsysteminternal.h"

#endif // PCH_MATERIALSYSTEM_H
