//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

// main precompiled header for client rendering code

#include "const.h"
#include "utlsymbol.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"

#include "convar.h"
#include "gl_model.h"
#include "r_local.h"
#include "gl_shader.h"
#include "cmodel_engine.h"
#include "gl_model_private.h"
#include "gl_cvars.h"
#include "gl_lightmap.h"
#include "gl_matsysiface.h"
#include "gl_rmain.h"
#include "gl_rsurf.h"
#include "gl_matsysiface.h"
#include "render.h"
#include "view.h"
#include "worldsize.h"
#include "sysexternal.h"
