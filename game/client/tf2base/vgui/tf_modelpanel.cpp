//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//


#include "cbase.h"
#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IScheme.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui/ISurface.h>
#include <vgui/IImage.h>
#include <vgui_controls/Label.h>

#include "materialsystem/imaterialsystem.h"

#include "c_sceneentity.h"
#include "gamestringpool.h"
#include "model_types.h"
#include "view_shared.h"
#include "view.h"
#include "ivrenderview.h"
#include "iefx.h"
#include "dlight.h"

#include "tf_modelpanel.h"

bool UseHWMorphModels();


char* ReadAndAllocStringValue( KeyValues *pSub, const char *pName, const char *pFilename = NULL );

using namespace vgui;

DECLARE_BUILD_FACTORY( CModelPanel );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CModelPanel::CModelPanel( vgui::Panel *pParent, const char *pName ) : vgui::EditablePanel( pParent, pName )
{
	m_nFOV = 54;
	m_hModel = NULL;
	m_pModelInfo = NULL;
	m_hScene = NULL;
	m_iDefaultAnimation = 0;
	m_bPanelDirty = true;

	ListenForGameEvent( "game_newmap" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CModelPanel::~CModelPanel()
{
	if ( m_pModelInfo )
	{
		delete m_pModelInfo;
		m_pModelInfo = NULL;
	}

	DeleteVCDData();
	DeleteModelData();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelPanel::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	m_nFOV = inResourceData->GetInt( "fov", 54 );

	// do we have a valid "model" section in the .res file?
	for ( KeyValues *pData = inResourceData->GetFirstSubKey() ; pData != NULL ; pData = pData->GetNextKey() )
	{
		if ( !Q_stricmp( pData->GetName(), "model" ) )
		{
			ParseModelInfo( pData );
			m_bPanelDirty = true;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelPanel::ParseModelInfo( KeyValues *inResourceData )
{
	// delete any current info
	if ( m_pModelInfo )
	{
		delete m_pModelInfo;
		m_pModelInfo = NULL;
	}

	m_pModelInfo = new CModelPanelModelInfo;

	if ( !m_pModelInfo )
		return;

	m_pModelInfo->m_pszModelName = ReadAndAllocStringValue( inResourceData, "modelname" );
	m_pModelInfo->m_pszModelName_HWM = ReadAndAllocStringValue( inResourceData, "modelname_hwm" );
	m_pModelInfo->m_nSkin = inResourceData->GetInt( "skin", -1 );
	m_pModelInfo->m_vecAbsAngles.Init( inResourceData->GetFloat( "angles_x", 0.0 ), inResourceData->GetFloat( "angles_y", 0.0 ), inResourceData->GetFloat( "angles_z", 0.0 ) );
	m_pModelInfo->m_vecOriginOffset.Init( inResourceData->GetFloat( "origin_x", 110.0 ), inResourceData->GetFloat( "origin_y", 5.0 ), inResourceData->GetFloat( "origin_z", 5.0 ) );
	m_pModelInfo->m_pszVCD = ReadAndAllocStringValue( inResourceData, "vcd" );
	m_pModelInfo->m_bUseSpotlight = ( inResourceData->GetInt( "spotlight", 0 ) == 1 );
	
	for ( KeyValues *pData = inResourceData->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
	{
		if ( !Q_stricmp( pData->GetName(), "animation" ) )
		{
			CModelPanelModelAnimation *pAnimation = new CModelPanelModelAnimation;

			if ( pAnimation )
			{
				pAnimation->m_pszName = ReadAndAllocStringValue( pData, "name" );
				pAnimation->m_pszSequence = ReadAndAllocStringValue( pData, "sequence" );
				pAnimation->m_bDefault = ( pData->GetInt( "default", 0 ) == 1 );

				for ( KeyValues *pAnimData = pData->GetFirstSubKey(); pAnimData != NULL; pAnimData = pAnimData->GetNextKey() )
				{
					if ( !Q_stricmp( pAnimData->GetName(), "pose_parameters" ) )
					{
						pAnimation->m_pPoseParameters = pAnimData->MakeCopy();
					}
				}

				m_pModelInfo->m_Animations.AddToTail( pAnimation );
				if ( pAnimation->m_bDefault )
				{
					m_iDefaultAnimation = m_pModelInfo->m_Animations.Find( pAnimation );
				}
			}
		}
		else if ( !Q_stricmp( pData->GetName(), "attached_model" ) )
		{
			CModelPanelAttachedModelInfo *pAttachedModelInfo = new CModelPanelAttachedModelInfo;

			if ( pAttachedModelInfo )
			{
				pAttachedModelInfo->m_pszModelName = ReadAndAllocStringValue( pData, "modelname" );
				pAttachedModelInfo->m_nSkin = pData->GetInt( "skin", -1 );

				m_pModelInfo->m_AttachedModelsInfo.AddToTail( pAttachedModelInfo );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelPanel::FireGameEvent( IGameEvent * event )
{	
	const char *type = event->GetName();

	if ( Q_strcmp( type, "game_newmap" ) == 0 )
	{
		// force the models to re-setup themselves
		m_bPanelDirty = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelPanel::SetDefaultAnimation( const char *pszName )
{
	if ( m_pModelInfo )
	{
		for ( int i = 0; i < m_pModelInfo->m_Animations.Count(); i++ )
		{
			if ( m_pModelInfo->m_Animations[i] && m_pModelInfo->m_Animations[i]->m_pszName )
			{
				if ( !Q_stricmp( m_pModelInfo->m_Animations[i]->m_pszName, pszName ) )
				{
					m_iDefaultAnimation = i;
					return;
				}
			}
		}
	}

	Assert( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelPanel::DeleteVCDData( void )
{
	if ( m_hScene.Get() )
	{
		m_hScene->StopClientOnlyScene();

		m_hScene->Remove();
		m_hScene = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelPanel::SetupVCD( void )
{
	if ( !m_pModelInfo )
		return;

	DeleteVCDData();

	C_SceneEntity *pEnt = new class C_SceneEntity;

	if ( !pEnt )
		return;

	if ( pEnt->InitializeAsClientEntity( "", RENDER_GROUP_OTHER ) == false )
	{
		// we failed to initialize this entity so just return gracefully
		pEnt->Remove();
		return;
	}

	// setup the handle
	m_hScene = pEnt;

	// setup the scene
	pEnt->SetupClientOnlyScene( m_pModelInfo->m_pszVCD, m_hModel, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelPanel::DeleteModelData( void )
{
	if ( m_hModel.Get() )
	{
		m_hModel->Remove();
		m_hModel = NULL;
	}

	for ( int i = 0 ; i < m_AttachedModels.Count() ; i++ )
	{
		if ( m_AttachedModels[i].Get() )
		{
			m_AttachedModels[i]->Remove();
		}
		m_AttachedModels.Remove( i );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CModelPanel::GetModelName( void )
{
	if ( !m_pModelInfo )
		return NULL;

	// check to see if we want to use a HWM model
	if ( UseHWMorphModels() )
	{
		// do we have a valid HWM model filename
		if ( m_pModelInfo->m_pszModelName_HWM && ( Q_strlen( m_pModelInfo->m_pszModelName_HWM  ) > 0 ) )
		{
			// does the file exist
			model_t *pModel = (model_t *)engine->LoadModel( m_pModelInfo->m_pszModelName_HWM );
			if ( pModel )
			{
				return m_pModelInfo->m_pszModelName_HWM;
			}
		}
	}

	return m_pModelInfo->m_pszModelName;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelPanel::SetupModel( void )
{
	if ( !m_pModelInfo )
		return; 

	MDLCACHE_CRITICAL_SECTION();

	// remove any current models we're using
	DeleteModelData();

	// create the new model
	CModelPanelModel *pEnt = new CModelPanelModel;

	if ( !pEnt )
		return;

	if ( pEnt->InitializeAsClientEntity( GetModelName(), RENDER_GROUP_OPAQUE_ENTITY ) == false )
	{
		// we failed to initialize this entity so just return gracefully
		pEnt->Remove();
		return;
	}
	
	// setup the handle
	m_hModel = pEnt;

	pEnt->DontRecordInTools();
	pEnt->AddEffects( EF_NODRAW ); // don't let the renderer draw the model normally

	if ( m_pModelInfo->m_nSkin >= 0 )
	{
		pEnt->m_nSkin = m_pModelInfo->m_nSkin;
	}

	// do we have any animation information?
	if ( m_pModelInfo->m_Animations.Count() > 0 && m_pModelInfo->m_Animations.IsValidIndex( m_iDefaultAnimation ) )
	{
		CModelPanelModelAnimation *pAnim = m_pModelInfo->m_Animations[ m_iDefaultAnimation ];
		int sequence = pEnt->LookupSequence( pAnim->m_pszSequence );
		if ( sequence != ACT_INVALID )
		{
			pEnt->ResetSequence( sequence );
			pEnt->SetCycle( 0 );

			if ( pAnim->m_pPoseParameters )
			{
				for ( KeyValues *pData = pAnim->m_pPoseParameters->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
				{
					const char *pName = pData->GetName();
					float flValue = pData->GetFloat();
		
					pEnt->SetPoseParameter( pName, flValue );
				}
			}

			pEnt->m_flAnimTime = gpGlobals->curtime;
		}
	}

	// setup any attached models
	for ( int i = 0 ; i < m_pModelInfo->m_AttachedModelsInfo.Count() ; i++ )
	{
		CModelPanelAttachedModelInfo *pInfo = m_pModelInfo->m_AttachedModelsInfo[i];
		C_BaseAnimating *pTemp = new C_BaseAnimating;

		if ( pTemp )
		{
			if ( pTemp->InitializeAsClientEntity( pInfo->m_pszModelName, RENDER_GROUP_OPAQUE_ENTITY ) == false )
			{	
				// we failed to initialize this model so just skip it
				pTemp->Remove();
				continue;
			}

			pTemp->DontRecordInTools();
			pTemp->AddEffects( EF_NODRAW ); // don't let the renderer draw the model normally
			pTemp->FollowEntity( m_hModel.Get() ); // attach to parent model

			if ( pInfo->m_nSkin >= 0 )
			{
				pTemp->m_nSkin = pInfo->m_nSkin;
			}

			pTemp->m_flAnimTime = gpGlobals->curtime;
			m_AttachedModels.AddToTail( pTemp );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelPanel::InitCubeMaps()
{
	ITexture *pCubemapTexture;

	// Deal with the default cubemap
	if ( g_pMaterialSystemHardwareConfig->GetHDREnabled() )
	{
		pCubemapTexture = materials->FindTexture( "editor/cubemap.hdr", NULL, true );
		m_DefaultHDREnvCubemap.Init( pCubemapTexture );
	}
	else
	{
		pCubemapTexture = materials->FindTexture( "editor/cubemap", NULL, true );
		m_DefaultEnvCubemap.Init( pCubemapTexture );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelPanel::Paint()
{
	BaseClass::Paint();

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

	if ( !pLocalPlayer || !m_pModelInfo )
		return;

	MDLCACHE_CRITICAL_SECTION();

	if ( m_bPanelDirty )
	{
		InitCubeMaps();

		SetupModel();

		// are we trying to play a VCD?
		if ( Q_strlen( m_pModelInfo->m_pszVCD ) > 0 )
		{
			SetupVCD();
		}

		m_bPanelDirty = false;
	}

	if ( !m_hModel.Get() )
		return;

	int i = 0;
	int x, y, w, h;

	GetBounds( x, y, w, h );
	LocalToScreen( x, y );

	if ( x < 0 )
	{
		// prevent x from being pushed off the left side of the screen
		// for modes like 1280 x 1024 (prevents model from being drawn in the panel)
		x = 0;
	}

	Vector vecExtraModelOffset( 0, 0, 0 );
	float flWidthRatio = engine->GetScreenAspectRatio() / ( 4.0f / 3.0f );

	// is this a player model?
	if ( Q_strstr( GetModelName(), "models/player/" ) )
	{
		// need to know if the ratio is not 4/3
		// HACK! HACK! to get our player models to appear the way they do in 4/3 if we're using other aspect ratios
		if ( flWidthRatio > 1.05f ) 
		{
			vecExtraModelOffset.Init( -60, 0, 0 );
		}
		else if ( flWidthRatio < 0.95f )
		{
			vecExtraModelOffset.Init( 15, 0, 0 );
		}
	}

	m_hModel->SetAbsOrigin( m_pModelInfo->m_vecOriginOffset + vecExtraModelOffset );
	m_hModel->SetAbsAngles( QAngle( m_pModelInfo->m_vecAbsAngles.x, m_pModelInfo->m_vecAbsAngles.y, m_pModelInfo->m_vecAbsAngles.z ) );

	// do we have a valid sequence?
	if ( m_hModel->GetSequence() != -1 )
	{
		m_hModel->FrameAdvance( gpGlobals->frametime );
	}

	// Now draw it.
	CViewSetup view;
	view.x = x;
	view.y = y;
	view.width = w;
	view.height = h;

	view.m_bOrtho = false;

	// scale the FOV for aspect ratios other than 4/3
	view.fov = ScaleFOVByWidthRatio( m_nFOV, flWidthRatio );

	view.origin = vec3_origin;
	view.angles.Init();
	view.zNear = VIEW_NEARZ;
	view.zFar = 1000;

	CMatRenderContextPtr pRenderContext( materials );

	// Not supported by queued material system - doesn't appear to be necessary
//	ITexture *pLocalCube = pRenderContext->GetLocalCubemap();

	if ( g_pMaterialSystemHardwareConfig->GetHDREnabled() )
	{
		pRenderContext->BindLocalCubemap( m_DefaultHDREnvCubemap );
	}
	else
	{
		pRenderContext->BindLocalCubemap( m_DefaultEnvCubemap );
	}

	pRenderContext->SetLightingOrigin( vec3_origin );
	pRenderContext->SetAmbientLight( 0.4, 0.4, 0.4 );

	static Vector white[6] = 
	{
		Vector( 0.4, 0.4, 0.4 ),
		Vector( 0.4, 0.4, 0.4 ),
		Vector( 0.4, 0.4, 0.4 ),
		Vector( 0.4, 0.4, 0.4 ),
		Vector( 0.4, 0.4, 0.4 ),
		Vector( 0.4, 0.4, 0.4 ),
	};

	g_pStudioRender->SetAmbientLightColors( white );
	g_pStudioRender->SetLocalLights( 0, NULL );

	if ( m_pModelInfo->m_bUseSpotlight )
	{
		Vector vecMins, vecMaxs;
		m_hModel->GetRenderBounds( vecMins, vecMaxs );
		LightDesc_t spotLight( vec3_origin + Vector( 0, 0, 200 ), Vector( 1, 1, 1 ), m_hModel->GetAbsOrigin() + Vector( 0, 0, ( vecMaxs.z - vecMins.z ) * 0.75 ), 0.035, 0.873 );
		g_pStudioRender->SetLocalLights( 1, &spotLight );
	}

	Frustum dummyFrustum;
	render->Push3DView( view, 0, NULL, dummyFrustum );

	modelrender->SuppressEngineLighting( true );
	float color[3] = { 1.0f, 1.0f, 1.0f };
	render->SetColorModulation( color );
	render->SetBlend( 1.0f );
	m_hModel->DrawModel( STUDIO_RENDER );

	for ( i = 0 ; i < m_AttachedModels.Count() ; i++ )
	{
		if ( m_AttachedModels[i].Get() )
		{
			m_AttachedModels[i]->DrawModel( STUDIO_RENDER );
		}
	}

	modelrender->SuppressEngineLighting( false );
	
	render->PopView( dummyFrustum );

	pRenderContext->BindLocalCubemap( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CModelPanel::SetSequence( const char *pszName )
{
	bool bRetVal = false;
	const char *pszAnim = NULL;
	int iIndex = 0;

	MDLCACHE_CRITICAL_SECTION();

	if ( m_pModelInfo )
	{
		// first try to find the sequence using pszName as the friendly name 
		for ( iIndex = 0 ; iIndex <  m_pModelInfo->m_Animations.Count() ; iIndex++ )
		{
			CModelPanelModelAnimation *pAnimation = m_pModelInfo->m_Animations[ iIndex ];
			if ( FStrEq( pAnimation->m_pszName, pszName ) )
			{
				pszAnim = pAnimation->m_pszSequence;
				break;
			}
		}

		// did we find a match?
		if ( !pszAnim )
		{
			// if not, just use the passed name as the sequence
			pszAnim = pszName;
		}

		if ( m_hModel.Get() )
		{
			int sequence = m_hModel->LookupSequence( pszAnim );
			if ( sequence != ACT_INVALID )
			{
				m_hModel->ResetSequence( sequence );
				m_hModel->SetCycle( 0 );

				bRetVal = true;
			}
		}
	}

	return bRetVal;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelPanel::OnSetAnimation( KeyValues *data )
{
	if ( data )
	{
		const char *pszAnimation = data->GetString( "animation", "" );
		SetSequence( pszAnimation );
	}
}
