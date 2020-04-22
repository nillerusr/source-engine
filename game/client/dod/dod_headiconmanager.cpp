//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include <string.h>
#include <stdio.h>
#include "dod_headiconmanager.h"
#include "c_playerresource.h"
#include "cliententitylist.h"
#include "c_baseplayer.h"
#include "materialsystem/imesh.h"
#include "view.h"
#include "materialsystem/imaterial.h"
#include "tier0/dbg.h"
#include "cdll_int.h"

#include "dod_shareddefs.h"
#include "c_dod_player.h"
#include "voice_status.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static CHeadIconManager s_HeadIconMgr;

CHeadIconManager* HeadIconManager()
{
	return &s_HeadIconMgr;
}

CHeadIconManager::CHeadIconManager()
{
	m_pAlliesIconMaterial = NULL;
	m_pAxisIconMaterial = NULL;
}

CHeadIconManager::~CHeadIconManager()
{
	Shutdown();
}

bool CHeadIconManager::Init()
{
	if ( !m_pAlliesIconMaterial )
	{
		m_pAlliesIconMaterial = materials->FindMaterial( "sprites/player_icons/american", TEXTURE_GROUP_VGUI );
	}

	if ( !m_pAxisIconMaterial )
	{
		m_pAxisIconMaterial = materials->FindMaterial( "sprites/player_icons/german", TEXTURE_GROUP_VGUI );
	}

	if ( IsErrorMaterial( m_pAlliesIconMaterial ) || 
		 IsErrorMaterial( m_pAxisIconMaterial ) )
	{
		Assert(!"Can't find head icon materials");
		return false;
	}

	m_pAlliesIconMaterial->IncrementReferenceCount();
	m_pAxisIconMaterial->IncrementReferenceCount();
	m_PlayerDrawn.ClearAll();

	return true;
}

void CHeadIconManager::Shutdown()
{
	if ( m_pAlliesIconMaterial )
	{
		m_pAlliesIconMaterial->DecrementReferenceCount();
		m_pAlliesIconMaterial = NULL;	
	}

	if ( m_pAxisIconMaterial )
	{
		m_pAxisIconMaterial->DecrementReferenceCount();
		m_pAxisIconMaterial = NULL;
	}
}


//-----------------------------------------------------------------------------
// Call from player render calls to indicate a head icon should be drawn for this player this frame
//-----------------------------------------------------------------------------
void CHeadIconManager::PlayerDrawn( C_BasePlayer *pPlayer )
{
	m_PlayerDrawn.Set( pPlayer->entindex() - 1 );
}


ConVar cl_headiconoffset( "cl_headiconoffset", "24", FCVAR_CHEAT );
ConVar cl_headiconsize( "cl_headiconsize", "8", FCVAR_CHEAT );
ConVar cl_identiconmode( "cl_identiconmode", "2", FCVAR_ARCHIVE, "2 - icons over teammates' heads\n1- icons over target teammate\n0 - no head icons" );

void CHeadIconManager::DrawHeadIcons()
{
	CMatRenderContextPtr pRenderContext( materials );

	CBitVec<MAX_PLAYERS> playerDrawn = m_PlayerDrawn;
	m_PlayerDrawn.ClearAll();

	if ( cl_identiconmode.GetInt() <= 0 )
		return;

	C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();

	if ( !pLocalPlayer )
		return;

	if ( pLocalPlayer->GetTeamNumber() != TEAM_ALLIES &&
			pLocalPlayer->GetTeamNumber() != TEAM_AXIS )
	{
		return;
	}

	Vector vUp = CurrentViewUp();
	Vector vRight = CurrentViewRight();
	if ( fabs( vRight.z ) > 0.95 )	// don't draw it edge-on
		return;

	vRight.z = 0;
	VectorNormalize( vRight );

	float flSize = cl_headiconsize.GetFloat();

	for(int i=0; i < MAX_PLAYERS; i++)
	{
		if ( !playerDrawn.IsBitSet( i ) )
			continue;

		if ( cl_identiconmode.GetInt() == 1 )
		{
			// only draw if this player is our status bar target
			if ( (i+1) != pLocalPlayer->GetIDTarget() )
				continue;
		}

		IClientNetworkable *pClient = cl_entitylist->GetClientEntity( i+1 );

		// Don't show an icon if the player is not in our PVS.
		if ( !pClient || pClient->IsDormant() )
			continue;

		C_DODPlayer *pPlayer = dynamic_cast<C_DODPlayer*>(pClient);
		if( !pPlayer )
			continue;

		// Don't show an icon for dead or spectating players (ie: invisible entities).
		if( pPlayer->IsPlayerDead() )
			continue;

		if( pPlayer == pLocalPlayer )
			continue;

		if( pPlayer->GetTeamNumber() != pLocalPlayer->GetTeamNumber() )
			continue;

		if ( GetClientVoiceMgr()->IsPlayerSpeaking( i+1 ) )
			continue;

		if ( C_BasePlayer::GetLocalPlayer()->GetObserverMode() == OBS_MODE_IN_EYE &&
			 C_BasePlayer::GetLocalPlayer()->GetObserverTarget() == pPlayer )
			continue;

		IMaterial *pMaterial = pPlayer->GetHeadIconMaterial();
		if ( !pMaterial )
			continue;

		pRenderContext->Bind( pMaterial );

		Vector vOrigin; 
		QAngle vAngle;

		int iHeadAttach = pPlayer->LookupAttachment( "head" );
			
		pPlayer->GetAttachment( iHeadAttach, vOrigin, vAngle );

		vOrigin.z += cl_headiconoffset.GetFloat();

		// Align it towards the viewer

		IMesh *pMesh = pRenderContext->GetDynamicMesh();
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,0,0 );
		meshBuilder.Position3fv( (vOrigin + (vRight * -flSize) + (vUp * flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,1,0 );
		meshBuilder.Position3fv( (vOrigin + (vRight * flSize) + (vUp * flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,1,1 );
		meshBuilder.Position3fv( (vOrigin + (vRight * flSize) + (vUp * -flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,0,1 );
		meshBuilder.Position3fv( (vOrigin + (vRight * -flSize) + (vUp * -flSize)).Base() );
		meshBuilder.AdvanceVertex();
		meshBuilder.End();
		pMesh->Draw();
	}
}