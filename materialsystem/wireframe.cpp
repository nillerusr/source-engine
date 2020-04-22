//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//===========================================================================//

#include "shaderlib/cshader.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef _WIN32
DEFINE_FALLBACK_SHADER( Wireframe, Wireframe_DX6 )
BEGIN_SHADER( Wireframe_DX6, 
			  "Help for Wireframe_DX6" )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM_OVERRIDE( BASETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shadertest/basetexture", "unused", SHADER_PARAM_NOT_EDITABLE )
		SHADER_PARAM_OVERRIDE( FRAME, SHADER_PARAM_TYPE_INTEGER, "0", "unused", SHADER_PARAM_NOT_EDITABLE )
		SHADER_PARAM_OVERRIDE( BASETEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "unused", SHADER_PARAM_NOT_EDITABLE )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS( MATERIAL_VAR_NO_DEBUG_OVERRIDE );
		SET_FLAGS( MATERIAL_VAR_NOFOG );
	}

	SHADER_INIT
	{
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->PolyMode( SHADER_POLYMODEFACE_FRONT_AND_BACK, SHADER_POLYMODE_LINE );

			SetModulationShadowState();
			SetDefaultBlendingShadowState();

			int flags = SHADER_DRAW_POSITION;
			if ( IS_FLAG_SET(MATERIAL_VAR_VERTEXCOLOR) || IS_FLAG_SET(MATERIAL_VAR_VERTEXALPHA) )
			{
				flags |= SHADER_DRAW_COLOR;
			}
			pShaderShadow->DrawFlags( flags );
		}
		DYNAMIC_STATE
		{
			SetModulationDynamicState();
		}
		Draw();
	}
END_SHADER
#endif
