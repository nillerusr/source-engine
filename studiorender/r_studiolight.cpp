//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#include "r_studiolight.h"
#include "studiorender.h"
#include "studiorendercontext.h"
#include "studio.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "mathlib/vector.h"
#include "mathlib/mathlib.h"
#include <float.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void R_WorldLightDelta( const LightDesc_t *wl, const Vector& org, Vector& delta );


//-----------------------------------------------------------------------------
// Copies lighting state
//-----------------------------------------------------------------------------
int CopyLocalLightingState( int nMaxLights, LightDesc_t *pDest, int nLightCount, const LightDesc_t *pSrc )
{
	// ensure we write within array bounds
	if ( nLightCount > nMaxLights )
	{
		nLightCount = nMaxLights;
	}

	for( int i = 0; i < nLightCount; i++ )
	{
		LightDesc_t *pLight = &pDest[i];
		memcpy( pLight, &pSrc[i], sizeof( LightDesc_t ) );
		pLight->m_Flags = 0;
		if( pLight->m_Attenuation0 != 0.0f )
		{
			pLight->m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION0;
		}
		if( pLight->m_Attenuation1 != 0.0f )
		{
			pLight->m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION1;
		}
		if( pLight->m_Attenuation2 != 0.0f )
		{
			pLight->m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2;
		}
	}

	return nLightCount;
}


//-----------------------------------------------------------------------------
// Computes the ambient term
//-----------------------------------------------------------------------------
void R_LightAmbient_4D( const Vector& normal, Vector4D* pLightBoxColor, Vector &lv )
{
	VectorScale( normal[0] > 0.f ? pLightBoxColor[0].AsVector3D() : pLightBoxColor[1].AsVector3D(), normal[0]*normal[0], lv );
	VectorMA( lv, normal[1]*normal[1], normal[1] > 0.f ? pLightBoxColor[2].AsVector3D() : pLightBoxColor[3].AsVector3D(), lv );
	VectorMA( lv, normal[2]*normal[2], normal[2] > 0.f ? pLightBoxColor[4].AsVector3D() : pLightBoxColor[5].AsVector3D(), lv );
}

#if defined( _WIN32 ) && !defined( _X360 )
void R_LightAmbient_4D( const FourVectors& normal, Vector4D* pLightBoxColor, FourVectors &lv )
{
//	VPROF( "R_LightAmbient" );

	// !!speed!! compute ambient color cube in sse format
	static fltx4 FourZeros={0.,0.,0.,.0};

	// find the contributions from each axis
	fltx4 NegMask=CmpLtSIMD(normal.x,FourZeros);
	fltx4 ColorSelect0=ReplicateX4(pLightBoxColor[0].AsVector3D().x);
	fltx4 ColorSelect1=ReplicateX4(pLightBoxColor[1].AsVector3D().x);
	fltx4 DirectionalColor=OrSIMD(AndSIMD(ColorSelect1,NegMask),AndNotSIMD(NegMask,ColorSelect0));
	fltx4 NormCompSquared=MulSIMD(normal.x,normal.x);
	lv.x=MulSIMD(DirectionalColor,NormCompSquared);
	ColorSelect0=ReplicateX4(pLightBoxColor[0].AsVector3D().y);
	ColorSelect1=ReplicateX4(pLightBoxColor[1].AsVector3D().y);
	DirectionalColor=OrSIMD(AndSIMD(ColorSelect1,NegMask),AndNotSIMD(NegMask,ColorSelect0));
	lv.y=MulSIMD(DirectionalColor,NormCompSquared);
	ColorSelect0=ReplicateX4(pLightBoxColor[0].AsVector3D().z);
	ColorSelect1=ReplicateX4(pLightBoxColor[1].AsVector3D().z);
	DirectionalColor=OrSIMD(AndSIMD(ColorSelect1,NegMask),AndNotSIMD(NegMask,ColorSelect0));
	lv.z=MulSIMD(DirectionalColor,NormCompSquared);

	NegMask=CmpLtSIMD(normal.y,FourZeros);
	ColorSelect0=ReplicateX4(pLightBoxColor[2].AsVector3D().x);
	ColorSelect1=ReplicateX4(pLightBoxColor[3].AsVector3D().x);
	DirectionalColor=OrSIMD(AndSIMD(ColorSelect1,NegMask),AndNotSIMD(NegMask,ColorSelect0));
	NormCompSquared=MulSIMD(normal.y,normal.y);
	lv.x=AddSIMD(lv.x,MulSIMD(DirectionalColor,NormCompSquared));
	ColorSelect0=ReplicateX4(pLightBoxColor[2].AsVector3D().y);
	ColorSelect1=ReplicateX4(pLightBoxColor[3].AsVector3D().y);
	DirectionalColor=OrSIMD(AndSIMD(ColorSelect1,NegMask),AndNotSIMD(NegMask,ColorSelect0));
	lv.y=AddSIMD(lv.y,MulSIMD(DirectionalColor,NormCompSquared));
	ColorSelect0=ReplicateX4(pLightBoxColor[2].AsVector3D().z);
	ColorSelect1=ReplicateX4(pLightBoxColor[3].AsVector3D().z);
	DirectionalColor=OrSIMD(AndSIMD(ColorSelect1,NegMask),AndNotSIMD(NegMask,ColorSelect0));
	lv.z=AddSIMD(lv.z,MulSIMD(DirectionalColor,NormCompSquared));

	NegMask=CmpLtSIMD(normal.z,FourZeros);
	ColorSelect0=ReplicateX4(pLightBoxColor[4].AsVector3D().x);
	ColorSelect1=ReplicateX4(pLightBoxColor[5].AsVector3D().x);
	DirectionalColor=OrSIMD(AndSIMD(ColorSelect1,NegMask),AndNotSIMD(NegMask,ColorSelect0));
	NormCompSquared=MulSIMD(normal.z,normal.z);
	lv.x=AddSIMD(lv.x,MulSIMD(DirectionalColor,NormCompSquared));
	ColorSelect0=ReplicateX4(pLightBoxColor[4].AsVector3D().y);
	ColorSelect1=ReplicateX4(pLightBoxColor[5].AsVector3D().y);
	DirectionalColor=OrSIMD(AndSIMD(ColorSelect1,NegMask),AndNotSIMD(NegMask,ColorSelect0));
	lv.y=AddSIMD(lv.y,MulSIMD(DirectionalColor,NormCompSquared));
	ColorSelect0=ReplicateX4(pLightBoxColor[4].AsVector3D().z);
	ColorSelect1=ReplicateX4(pLightBoxColor[5].AsVector3D().z);
	DirectionalColor=OrSIMD(AndSIMD(ColorSelect1,NegMask),AndNotSIMD(NegMask,ColorSelect0));
	lv.z=AddSIMD(lv.z,MulSIMD(DirectionalColor,NormCompSquared));
}
#endif


//-----------------------------------------------------------------------------
// Computes the ambient term, parameters are 3D Vectors for optimization
//-----------------------------------------------------------------------------
void R_LightAmbient_3D( const Vector& normal, const Vector* pLightBoxColor, Vector &lv )
{
	VectorScale( normal[0] > 0.f ? pLightBoxColor[0] : pLightBoxColor[1], normal[0]*normal[0], lv );
	VectorMA( lv, normal[1]*normal[1], normal[1] > 0.f ? pLightBoxColor[2] : pLightBoxColor[3], lv );
	VectorMA( lv, normal[2]*normal[2], normal[2] > 0.f ? pLightBoxColor[4] : pLightBoxColor[5], lv );
}


//-----------------------------------------------------------------------------
// Set up light[i].dot, light[i].falloff, and light[i].delta for all lights given 
// a vertex position "vert".
//-----------------------------------------------------------------------------
void R_LightStrengthWorld( const Vector& vert, int lightcount, LightDesc_t* pDesc, lightpos_t *light )
{
//	VPROF( "R_LightStrengthWorld" );

	// NJS: note to self, maybe switch here based on lightcount, so multiple squareroots can be done simeltaneously?
	for ( int i = 0; i < lightcount; i++)
	{
		R_WorldLightDelta( &pDesc[i], vert, light[i].delta );
		light[i].falloff = R_WorldLightDistanceFalloff( &pDesc[i], light[i].delta );

		VectorNormalizeFast( light[i].delta );
		light[i].dot = DotProduct( light[i].delta, pDesc[i].m_Direction );
	}
}


//-----------------------------------------------------------------------------
// Calculate the delta between a light and position 
//-----------------------------------------------------------------------------
void R_WorldLightDelta( const LightDesc_t *wl, const Vector& org, Vector& delta )
{
	switch (wl->m_Type)
	{
		case MATERIAL_LIGHT_POINT:
		case MATERIAL_LIGHT_SPOT:
			VectorSubtract( wl->m_Position, org, delta );
			break;

		case MATERIAL_LIGHT_DIRECTIONAL:
			VectorMultiply( wl->m_Direction, -1, delta );
			break;

		default:
			// Bug: need to return an error
			Assert( 0 );
			break;
	}
}


//#define NO_AMBIENT_CUBE 1

// TODO: cone clipping calc's wont work for boxlight since the player asks for a single point.  Not sure what the volume is.
TEMPLATE_FUNCTION_TABLE( void, R_LightEffectsWorldFunctionTable, ( const LightDesc_t* pLightDesc, const lightpos_t *light, const Vector& normal, Vector &dest ), 256 )
{
	enum  
	{ 
		LightType1 = ( nArgument & 0xC0 ) >> 6,
		LightType2 = ( nArgument & 0x30 ) >> 4,
		LightType3 = ( nArgument & 0x0C ) >> 2,
		LightType4 = ( nArgument & 0x03 )
	};
 
	//	VPROF( "R_LightEffectsWorld" );

	#ifdef NO_AMBIENT_CUBE
		dest[0] = dest[1] = dest[2] = 0.0f;
	#endif

	// FIXME: lighting effects for normal and position are independent!
	// FIXME: these can be pre-calculated per normal
	if( (LightType_t)LightType1 != MATERIAL_LIGHT_DISABLE )
	{
		float ratio = light[0].falloff * CWorldLightAngleWrapper<LightType1>::WorldLightAngle( &pLightDesc[0], pLightDesc[0].m_Direction, normal, light[0].delta );
		if (ratio > 0)
		{
			const float* pColor = (float*)&pLightDesc[0].m_Color;
			dest[0] += pColor[0] * ratio;
			dest[1] += pColor[1] * ratio;
			dest[2] += pColor[2] * ratio;
		}
	}

	if( (LightType_t)LightType2 != MATERIAL_LIGHT_DISABLE )
	{
		float ratio = light[1].falloff * CWorldLightAngleWrapper<LightType2>::WorldLightAngle( &pLightDesc[1], pLightDesc[1].m_Direction, normal, light[1].delta );
		if (ratio > 0)
		{
			const float* pColor = (float*)&pLightDesc[1].m_Color;
			dest[0] += pColor[0] * ratio;
			dest[1] += pColor[1] * ratio;
			dest[2] += pColor[2] * ratio;
		}
	}

	if( (LightType_t)LightType3 != MATERIAL_LIGHT_DISABLE )
	{
		float ratio = light[2].falloff * CWorldLightAngleWrapper<LightType3>::WorldLightAngle( &pLightDesc[2], pLightDesc[2].m_Direction, normal, light[2].delta );
		if (ratio > 0)
		{
			const float* pColor = (float*)&pLightDesc[2].m_Color;
			dest[0] += pColor[0] * ratio;
			dest[1] += pColor[1] * ratio;
			dest[2] += pColor[2] * ratio;
		}
	}

	if( (LightType_t)LightType4 != MATERIAL_LIGHT_DISABLE )
	{
		float ratio = light[3].falloff * CWorldLightAngleWrapper<LightType4>::WorldLightAngle( &pLightDesc[3], pLightDesc[3].m_Direction, normal, light[3].delta );
		if (ratio > 0)
		{
			const float* pColor = (float*)&pLightDesc[3].m_Color;
			dest[0] += pColor[0] * ratio;
			dest[1] += pColor[1] * ratio;
			dest[2] += pColor[2] * ratio;
		}
	}
}

TEMPLATE_FUNCTION_TABLE( void, R_LightEffectsWorldFunctionTableConstDirectional, ( const LightDesc_t* pLightDesc, const lightpos_t *light, const Vector& normal, Vector &dest, float flDirectionalConstant ), 256 )
{
	enum  
	{ 
		LightType1 = ( nArgument & 0xC0 ) >> 6,
		LightType2 = ( nArgument & 0x30 ) >> 4,
		LightType3 = ( nArgument & 0x0C ) >> 2,
		LightType4 = ( nArgument & 0x03 )
	};

	//	VPROF( "R_LightEffectsWorld" );

#ifdef NO_AMBIENT_CUBE
	dest[0] = dest[1] = dest[2] = 0.0f;
#endif

	// FIXME: lighting effects for normal and position are independent!
	// FIXME: these can be pre-calculated per normal
	if( (LightType_t)LightType1 != MATERIAL_LIGHT_DISABLE )
	{
		float ratio = light[0].falloff *
			CWorldLightAngleWrapperConstDirectional<LightType1>::WorldLightAngle( &pLightDesc[0],
				pLightDesc[0].m_Direction, normal, light[0].delta, flDirectionalConstant );
		if (ratio > 0)
		{
			const float* pColor = (float*)&pLightDesc[0].m_Color;
			dest[0] += pColor[0] * ratio;
			dest[1] += pColor[1] * ratio;
			dest[2] += pColor[2] * ratio;
		}
	}

	if( (LightType_t)LightType2 != MATERIAL_LIGHT_DISABLE )
	{
		float ratio = light[1].falloff *
			CWorldLightAngleWrapperConstDirectional<LightType2>::WorldLightAngle( &pLightDesc[1],
				pLightDesc[1].m_Direction, normal, light[1].delta, flDirectionalConstant );

		if (ratio > 0)
		{
			const float* pColor = (float*)&pLightDesc[1].m_Color;
			dest[0] += pColor[0] * ratio;
			dest[1] += pColor[1] * ratio;
			dest[2] += pColor[2] * ratio;
		}
	}

	if( (LightType_t)LightType3 != MATERIAL_LIGHT_DISABLE )
	{
		float ratio = light[2].falloff *
			CWorldLightAngleWrapperConstDirectional<LightType3>::WorldLightAngle( &pLightDesc[2],
				pLightDesc[2].m_Direction, normal, light[2].delta, flDirectionalConstant );

		if (ratio > 0)
		{
			const float* pColor = (float*)&pLightDesc[2].m_Color;
			dest[0] += pColor[0] * ratio;
			dest[1] += pColor[1] * ratio;
			dest[2] += pColor[2] * ratio;
		}
	}

	if( (LightType_t)LightType4 != MATERIAL_LIGHT_DISABLE )
	{
		float ratio = light[3].falloff *
			CWorldLightAngleWrapperConstDirectional<LightType4>::WorldLightAngle( &pLightDesc[3],
			pLightDesc[3].m_Direction, normal, light[3].delta, flDirectionalConstant );

		if (ratio > 0)
		{
			const float* pColor = (float*)&pLightDesc[3].m_Color;
			dest[0] += pColor[0] * ratio;
			dest[1] += pColor[1] * ratio;
			dest[2] += pColor[2] * ratio;
		}
	}
}


//-----------------------------------------------------------------------------
// Get the function table index
//-----------------------------------------------------------------------------
static int s_pLightMask[ 5 ] =
{			
	0,			// No lights
	0xC0,		// 1 light
	0xF0,		// 2 lights
	0xFC,		// 3 lights
	0xFF,		// 4 lights  
};

inline int R_LightEffectsWorldIndex(const LightDesc_t* pLightDesc, int nNumLights)
{
	if ( nNumLights > 4 )
	{
		nNumLights = 4;
	}

	int nIndex = ((pLightDesc[0].m_Type & 0x3) << 6) | ((pLightDesc[1].m_Type & 0x3) << 4) | ( (pLightDesc[2].m_Type & 0x3) << 2) | (pLightDesc[3].m_Type & 0x3);
	nIndex &= s_pLightMask[ nNumLights ]; 

	Assert( nIndex >= 0 && nIndex < R_LightEffectsWorldFunctionTable::count );
	return nIndex;
}


/*
  light_direction (light_pos - vertex_pos)
*/
// TODO: move cone calcs to position
// TODO: cone clipping calc's wont work for boxlight since the player asks for a single point.  Not sure what the volume is.
TEMPLATE_FUNCTION_TABLE( float, R_WorldLightDistanceFalloffFunctionTable, ( const LightDesc_t *wl, const Vector& delta ), 8)
{
	Assert( nArgument != 0 );

	float dist2 = DotProduct( delta, delta );

	// Cull out light beyond this radius
	if (wl->m_Range != 0.f)
	{
		if (dist2 > wl->m_Range * wl->m_Range)
			return 0.0f;
	}

	// The general purpose equation:
	float fTotal = FLT_EPSILON;

	if( nArgument & LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION0 )
	{
		fTotal = wl->m_Attenuation0;
	}

	if( nArgument & LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION1 )
	{
		fTotal += wl->m_Attenuation1 * FastSqrt( dist2 );
	}

	if( nArgument & LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2 )
	{
		fTotal += wl->m_Attenuation2 * dist2;
	}

	return 1.0f / fTotal;
}

//-----------------------------------------------------------------------------
// Calculate the falloff from the world lights 
//-----------------------------------------------------------------------------
float FASTCALL R_WorldLightDistanceFalloff( const LightDesc_t *wl, const Vector& delta )
{
	// Ensure no invalid flags are set
	Assert( ! ( wl->m_Flags & ~(LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION0|LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION1|LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2|LIGHTTYPE_OPTIMIZATIONFLAGS_DERIVED_VALUES_CALCED) ) );

	// calculate falloff
	int flags = wl->m_Flags & (LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION0|LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION1|LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2);
	return R_WorldLightDistanceFalloffFunctionTable::functions[flags](wl, delta);
}

#if defined( _WIN32 ) && !defined( _X360 )
fltx4 FASTCALL R_WorldLightDistanceFalloff( const LightDesc_t *wl, const FourVectors &delta )
{
	// !!speed!!: lights could store m_Attenuation2,m_Attenuation1, and m_Range^2 copies in replicated SSE format.

	// Ensure no invalid flags are set
	Assert( ! ( wl->m_Flags & ~(LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION0|LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION1|LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2|LIGHTTYPE_OPTIMIZATIONFLAGS_DERIVED_VALUES_CALCED) ) );

	fltx4 dist2 = delta*delta;

	fltx4 fTotal;

	if( wl->m_Flags & LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION0 )
	{
		fTotal = ReplicateX4(wl->m_Attenuation0);
	}
	else
		fTotal= ReplicateX4(FLT_EPSILON);					// !!speed!! replicate

	if( wl->m_Flags & LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION1 )
	{
		fTotal=AddSIMD(fTotal,MulSIMD(ReplicateX4(wl->m_Attenuation1),SqrtEstSIMD(dist2)));
	}

	if( wl->m_Flags & LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2 )
	{
		fTotal=AddSIMD(fTotal,MulSIMD(ReplicateX4(wl->m_Attenuation2),dist2));
	}

	fTotal=ReciprocalEstSIMD(fTotal);
	// Cull out light beyond this radius
	// now, zero out elements for which dist2 was > range^2. !!speed!! lights should store dist^2 in sse format
	if (wl->m_Range != 0.f)
	{
		fltx4 RangeSquared = ReplicateX4(wl->m_Range*wl->m_Range); // !!speed!!
		fTotal=AndSIMD(fTotal,CmpLtSIMD(dist2,RangeSquared));
	}
	return fTotal;
}
#endif


int CStudioRender::R_LightGlintPosition( int index, const Vector& org, Vector& delta, Vector& intensity )
{
	if (index >= m_pRC->m_NumLocalLights)
		return false;

	R_WorldLightDelta( &m_pRC->m_LocalLights[index], org, delta );
	float falloff = R_WorldLightDistanceFalloff( &m_pRC->m_LocalLights[index], delta );

	VectorMultiply( m_pRC->m_LocalLights[index].m_Color, falloff, intensity );
	return true;
}


//-----------------------------------------------------------------------------
// Setup up the function table 
//-----------------------------------------------------------------------------
void CStudioRender::R_InitLightEffectsWorld3()
{
	// set the function pointer
	int index = R_LightEffectsWorldIndex( m_pRC->m_LocalLights, m_pRC->m_NumLocalLights );
	R_LightEffectsWorld3 = R_LightEffectsWorldFunctionTable::functions[index];
}


//-----------------------------------------------------------------------------
// Performs lighting functions common to the ComputeLighting and ComputeLightingConstantDirectional
// returns the index of the LightEffectsWorldFunction to use 
//-----------------------------------------------------------------------------
static int ComputeLightingCommon( const Vector* pAmbient, int lightCount,
	LightDesc_t* pLights, const Vector& pt, const Vector& normal, lightpos_t *pLightPos, Vector& lighting )
{
	// Set up lightpos[i].dot, lightpos[i].falloff, and lightpos[i].delta for all lights
	R_LightStrengthWorld( pt, lightCount, pLights, pLightPos );

	// calculate ambient values from the ambient cube given a normal.
	R_LightAmbient_3D( normal, pAmbient, lighting );

	return R_LightEffectsWorldIndex( pLights, lightCount );
}


//-----------------------------------------------------------------------------
// Compute the lighting at a point and normal
// Final Lighting is in linear space
//-----------------------------------------------------------------------------
void CStudioRenderContext::ComputeLighting( const Vector* pAmbient, int lightCount,
		LightDesc_t* pLights, const Vector& pt, const Vector& normal, Vector& lighting )
{
	if ( m_RC.m_Config.fullbright )
	{
		lighting.Init( 1.0f, 1.0f, 1.0f );
		return;
	}

	if ( lightCount > ARRAYSIZE( m_pLightPos ) )
	{
		AssertMsg( 0, "Light count out of range in ComputeLighting\n" );
		lightCount = ARRAYSIZE( m_pLightPos );
	}

	// Calculate color given lightpos_t lightpos, a normal, and the ambient
	// color from the ambient cube calculated in ComputeLightingCommon
	int index = ComputeLightingCommon( pAmbient, lightCount, pLights, pt, normal, m_pLightPos, lighting );
	R_LightEffectsWorldFunctionTable::functions[index]( pLights, m_pLightPos, normal, lighting );
}


//-----------------------------------------------------------------------------
// Compute the lighting at a point and normal
// Final Lighting is in linear space
// Uses flDirectionalAmount instead of directional components of lights
//-----------------------------------------------------------------------------
void CStudioRenderContext::ComputeLightingConstDirectional( const Vector* pAmbient, int lightCount,
		LightDesc_t* pLights, const Vector& pt, const Vector& normal, Vector& lighting, float flDirectionalAmount )
{
	if ( m_RC.m_Config.fullbright )
	{
		lighting.Init( 1.0f, 1.0f, 1.0f );
		return;
	}

	if ( lightCount > ARRAYSIZE( m_pLightPos ) )
	{
		AssertMsg( 0, "Light count out of range in ComputeLighting\n" );
		lightCount = ARRAYSIZE( m_pLightPos );
	}

	// Calculate color given lightpos_t lightpos, a normal, and the ambient
	// color from the ambient cube calculated in ComputeLightingCommon
	int index = ComputeLightingCommon( pAmbient, lightCount, pLights, pt, normal, m_pLightPos, lighting );
	R_LightEffectsWorldFunctionTableConstDirectional::functions[index]( pLights, m_pLightPos, normal, lighting, flDirectionalAmount );
}