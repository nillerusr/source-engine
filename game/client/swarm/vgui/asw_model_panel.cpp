#include "cbase.h"
#include "asw_model_panel.h"
#include "renderparm.h"
#include "animation.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ConVar asw_key_falloff( "asw_key_falloff" , ".1" , FCVAR_NONE );
ConVar asw_rim_falloff( "asw_rim_falloff" , ".1" , FCVAR_NONE );
ConVar asw_key_atten( "asw_key_atten" , "1.0" , FCVAR_NONE );
ConVar asw_rim_atten( "asw_rim_atten" , ".25" , FCVAR_NONE );
ConVar asw_key_innerCone( "asw_key_innerCone" , "20.0" , FCVAR_NONE );
ConVar asw_key_outerCone( "asw_key_outerCone" , "30.0" , FCVAR_NONE );
ConVar asw_rim_innerCone( "asw_rim_innerCone" , "20.0" , FCVAR_NONE );
ConVar asw_rim_outerCone( "asw_rim_outerCone" , "30.0" , FCVAR_NONE );
ConVar asw_key_range( "asw_key_range" , "0" , FCVAR_NONE );
ConVar asw_rim_range( "asw_rim_range" , "0" , FCVAR_NONE );

CASW_Model_Panel::CASW_Model_Panel( vgui::Panel *parent, const char *name ) : CBaseModelPanel( parent, name )
{
	m_bShouldPaint = true;
	m_pStudioHdr = NULL;
}

CASW_Model_Panel::~CASW_Model_Panel()
{

}


void CASW_Model_Panel::Paint()
{
	CMatRenderContextPtr pRenderContext( materials );

	// Turn off depth-write to dest alpha so that we get white there instead.  The code that uses
	// the render target needs a mask of where stuff was rendered.
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_WRITE_DEPTH_TO_DESTALPHA, false );

	// Disable flashlights when drawing our model
	pRenderContext->SetFlashlightMode(false);

	if ( m_bShouldPaint )
	{
		SetupLights();

		BaseClass::Paint();
	}
}

void CASW_Model_Panel::SetupCustomLights( Color cAmbient, Color cKey, float fKeyBoost, Color cRim, float fRimBoost )
{
	memset( &m_LightingState, 0, sizeof(MaterialLightingState_t) );
	
	for ( int i = 0; i < 6; ++i )
	{
		m_LightingState.m_vecAmbientCube[0].Init( cAmbient[0]/255.0f  , cAmbient[1]/255.0f , cAmbient[2]/255.0f );
	}
	
	// set up the lighting
	//QAngle angDir = vec3_angle;
	//Vector vecPos = vec3_origin;
	matrix3x4_t lightMatrix;
	matrix3x4_t rimLightMatrix;

	m_LightingState.m_nLocalLightCount = 2;
	// key light settings
	GetAttachment( "attach_light", lightMatrix );
	m_LightingState.m_pLocalLightDesc[0].m_Type = MATERIAL_LIGHT_SPOT;
	m_LightingState.m_pLocalLightDesc[0].m_Color.Init( ( cKey[0]/255.0f )* fKeyBoost  , ( cKey[1]/255.0f )* fKeyBoost  , ( cKey[2]/255.0f ) * fKeyBoost );
	m_LightToWorld[0] = lightMatrix;
	m_LightingState.m_pLocalLightDesc[0].m_Falloff = asw_key_falloff.GetFloat();
	m_LightingState.m_pLocalLightDesc[0].m_Attenuation0 = asw_key_atten.GetFloat();
	m_LightingState.m_pLocalLightDesc[0].m_Attenuation1 = 0;
	m_LightingState.m_pLocalLightDesc[0].m_Attenuation2 = 0;
	m_LightingState.m_pLocalLightDesc[0].m_Theta = asw_key_innerCone.GetFloat();
	m_LightingState.m_pLocalLightDesc[0].m_Phi = asw_key_outerCone.GetFloat();
	m_LightingState.m_pLocalLightDesc[0].m_Range = asw_key_range.GetFloat();
	m_LightingState.m_pLocalLightDesc[0].RecalculateDerivedValues();


	// rim light settings
	GetAttachment( "attach_light_rim", rimLightMatrix );
	m_LightingState.m_pLocalLightDesc[1].m_Type = MATERIAL_LIGHT_SPOT;
	m_LightingState.m_pLocalLightDesc[1].m_Color.Init( ( cRim[0]/255.0f ) * fRimBoost , ( cRim[1]/255.0f ) * fRimBoost, ( cRim[2]/255.0f )* fRimBoost);
	m_LightToWorld[1] = rimLightMatrix;
	m_LightingState.m_pLocalLightDesc[1].m_Falloff = asw_rim_falloff.GetFloat();
	m_LightingState.m_pLocalLightDesc[1].m_Attenuation0 = asw_rim_atten.GetFloat();
	m_LightingState.m_pLocalLightDesc[1].m_Attenuation1 = 0;
	m_LightingState.m_pLocalLightDesc[1].m_Attenuation2 = 0;
	m_LightingState.m_pLocalLightDesc[1].m_Theta = asw_rim_innerCone.GetFloat();
	m_LightingState.m_pLocalLightDesc[1].m_Phi = asw_rim_outerCone.GetFloat();
	m_LightingState.m_pLocalLightDesc[1].m_Range = asw_rim_range.GetFloat();
	m_LightingState.m_pLocalLightDesc[1].RecalculateDerivedValues();
}

void CASW_Model_Panel::SetBodygroup( int iGroup, int iValue )
{
	studiohdr_t *pStudioHdr = m_RootMDL.m_MDL.GetStudioHdr();
	if ( !pStudioHdr )
		return;

	CStudioHdr studioHdr( pStudioHdr, g_pMDLCache );

	::SetBodygroup( &studioHdr, m_RootMDL.m_MDL.m_nBody, iGroup, iValue );
}


int CASW_Model_Panel::FindBodygroupByName( const char *name )
{
	studiohdr_t *pStudioHdr = m_RootMDL.m_MDL.GetStudioHdr();
	if ( !pStudioHdr )
		return -1;

	CStudioHdr studioHdr( pStudioHdr, g_pMDLCache );

	return ::FindBodygroupByName( &studioHdr, name );
}

int CASW_Model_Panel::GetNumBodyGroups( void )
{
	studiohdr_t *pStudioHdr = m_RootMDL.m_MDL.GetStudioHdr();
	if ( !pStudioHdr )
		return 0;

	CStudioHdr studioHdr( pStudioHdr, g_pMDLCache );

	return ::GetNumBodyGroups( &studioHdr );
}

/*
void SetBodygroup( CStudioHdr *pstudiohdr, int& body, int iGroup, int iValue )

// Build list of submodels
BodyPartInfo_t *pBodyPartInfo = (BodyPartInfo_t*)stackalloc( m_pStudioHdr->numbodyparts * sizeof(BodyPartInfo_t) );
for ( int i=0 ; i < m_pStudioHdr->numbodyparts; ++i ) 
{
	pBodyPartInfo[i].m_nSubModelIndex = R_StudioSetupModel( i, body, &pBodyPartInfo[i].m_pSubModel, m_pStudioHdr );
}
R_StudioRenderFinal( pRenderContext, skin, m_pStudioHdr->numbodyparts, pBodyPartInfo, 
					pEntity, ppMaterials, pMaterialFlags, boneMask, lod, pColorMeshes )

					m_VertexCache.SetBodyPart( i );
m_VertexCache.SetModel( pBodyPartInfo[i].m_nSubModelIndex );

numFacesRendered += R_StudioDrawPoints( pRenderContext, skin, pClientEntity, 
									   ppMaterials, pMaterialFlags, boneMask, lod, pColorMeshes );

									   */