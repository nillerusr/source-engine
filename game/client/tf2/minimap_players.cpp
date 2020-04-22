//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "c_basetfplayer.h"
#include "minimap_trace.h"
#include "vgui_entityimagepanel.h"
#include <KeyValues.h>
#include "vgui_bitmapimage.h"
#include "ViewConeImage.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "shareddefs.h"
#include "voice_status.h"
#include "iclientvehicle.h"

//-----------------------------------------------------------------------------
// Minimap panel representing players
//-----------------------------------------------------------------------------
class CMinimapPlayerPanel : public CMinimapTracePanel
{
	DECLARE_CLASS( CMinimapPlayerPanel, CMinimapTracePanel );

public:
	CMinimapPlayerPanel( vgui::Panel *parent, const char *panelName )
		: BaseClass( parent, "CMinimapPlayerPanel" )
	{
	}

	virtual bool Init( KeyValues* pKeyValues, MinimapInitData_t* pInitData );
	virtual void Paint( );
	virtual void OnTick( );

private:
	// Parse class image data from the file
	bool InitClassImages( KeyValues* pKeyValues, MinimapInitData_t* pInitData );

	CTeamBitmapImage m_DeadImage;
	CTeamBitmapImage m_AliveImage;
	CTeamBitmapImage m_LocalAliveImage;
	CTeamBitmapImage m_ArrowImage;
	CTeamBitmapImage m_pClassAliveImages[TFCLASS_CLASS_COUNT];
	BitmapImage m_VoiceImage;
	bool			 m_bHasClassImage[TFCLASS_CLASS_COUNT];
	int m_LocalOffsetX, m_LocalOffsetY;
	int m_LocalSizeX, m_LocalSizeY;
	int m_VoiceOffsetX, m_VoiceOffsetY;
	int m_VoiceSizeX, m_VoiceSizeY;
	float m_flVoiceAlpha;
};

DECLARE_MINIMAP_FACTORY( CMinimapPlayerPanel, "minimap_player_panel" );


//-----------------------------------------------------------------------------
// Parse class image data from the file
//-----------------------------------------------------------------------------
bool CMinimapPlayerPanel::InitClassImages( KeyValues* pKeyValues, MinimapInitData_t* pInitData )
{
	memset( m_bHasClassImage, 0, TFCLASS_CLASS_COUNT * sizeof(bool) );

	for (int i = 1; i < TFCLASS_CLASS_COUNT; ++i)
	{
		KeyValues *pClassSection = pKeyValues->FindKey( GetTFClassInfo(i)->m_pClassName );
		if (!pClassSection)
			continue;

		m_bHasClassImage[i] = true;
		if (!InitializeTeamImage( pKeyValues, GetTFClassInfo( i )->m_pClassName, this, pInitData->m_pEntity, &m_pClassAliveImages[i] ))
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Parse data from the file
//-----------------------------------------------------------------------------
bool CMinimapPlayerPanel::Init( KeyValues* pKeyValues, MinimapInitData_t* pInitData )
{
	// Must be applied to players
	Assert( dynamic_cast< C_BaseTFPlayer * >( pInitData->m_pEntity ) );

	if (!BaseClass::Init( pKeyValues, pInitData ))
		return false;

	if (!InitializeTeamImage( pKeyValues, "localaliveimage", this, pInitData->m_pEntity, &m_LocalAliveImage))
		return false;

	if (!InitializeTeamImage( pKeyValues, "aliveimage", this, pInitData->m_pEntity, &m_AliveImage ))
		return false;

	if (!InitializeTeamImage( pKeyValues, "arrowimage", this, pInitData->m_pEntity, &m_ArrowImage ))
		return false;

	if (!InitializeTeamImage( pKeyValues, "deadimage", this, pInitData->m_pEntity, &m_DeadImage ))
		return false;

	if (!InitializeImage( pKeyValues, "voiceimage", this, &m_VoiceImage ))
		return false;

	// Load class-specific images
	if (!InitClassImages( pKeyValues, pInitData ) )
		return false;

	// Modify the size if this is the local player
	if (!ParseCoord(pKeyValues, "localplayeroffset", m_LocalOffsetX, m_LocalOffsetY))
		return false;

	if (!ParseCoord(pKeyValues, "localplayersize", m_LocalSizeX, m_LocalSizeY ))
		return false;

	if (!ParseCoord(pKeyValues, "voiceoffset", m_VoiceOffsetX, m_VoiceOffsetY ))
		return false;

	if (!ParseCoord(pKeyValues, "voicesize", m_VoiceSizeX, m_VoiceSizeY ))
		return false;

	m_flVoiceAlpha = 0.0f;
	SetCursor( NULL );

	return true;
}


//-----------------------------------------------------------------------------
// Render
//-----------------------------------------------------------------------------
void CMinimapPlayerPanel::OnTick( )
{
	// Modify position and size if we're the local player
	// It's too bad, but we can't do this during Init() because 
	// C_BaseTFPlayer::GetLocalPlayer() doesn't return the correct value at that time
	if ( C_BaseTFPlayer::GetLocalPlayer() == GetEntity() )
	{
		m_OffsetX = m_LocalOffsetX;
		m_OffsetY = m_LocalOffsetY;

		m_SizeW = m_LocalSizeX;
		m_SizeH = m_LocalSizeY;

		// Make sure the local player draws on top
		SetZPos( MINIMAP_LOCAL_PLAYER );

		SetSize( m_LocalSizeX, m_LocalSizeY );
	}

	BaseClass::OnTick();
}


//-----------------------------------------------------------------------------
// Render
//-----------------------------------------------------------------------------
void CMinimapPlayerPanel::Paint( )
{
	if ( gHUD.IsHidden( HIDEHUD_MISCSTATUS ) )
		return;

	C_BaseEntity* pEntity = GetEntity();
	Assert( pEntity );

	// Don't render players if they aren't on a team
	if( pEntity->GetTeamNumber() == 0 )
		return;

	C_BaseTFPlayer *local = C_BaseTFPlayer::GetLocalPlayer();
	C_BaseTFPlayer *pPlayer = static_cast< C_BaseTFPlayer * >( pEntity );
	Assert( pPlayer );

	if (!m_bClipToMap)
		g_pMatSystemSurface->DisableClipping( true );

	// Set the fade amount (only for alive players)...
//	m_AliveImage.SetAlpha( pPlayer->GetOverlayAlpha() );
//	m_LocalAliveImage.SetAlpha( pPlayer->GetOverlayAlpha() );

	bool isLocalPlayer = ( pEntity == local );
	
	bool isAlive = pPlayer->GetHealth() > 0 ? true : false;
	if ( isAlive )
	{
		if ( pPlayer->IsPlayerDead() )
		{
			isAlive = false;
		}
	}

	float yaw = pPlayer->EyeAngles().y;
	if ( pPlayer->InLocalTeam() && GetClientVoiceMgr()->IsPlayerSpeaking( pPlayer->entindex() ) &&
			GetClientVoiceMgr()->IsPlayerAudible( pPlayer->entindex() ) )
	{
		m_flVoiceAlpha = 1.0f;
	}
	else if (m_flVoiceAlpha != 0.0f)
	{
		m_flVoiceAlpha *= 0.8f;
		if (m_flVoiceAlpha < 0.005)
			m_flVoiceAlpha = 0.0f;
	}

	if ( m_flVoiceAlpha != 0.0f )
	{
		int x = (int)(m_VoiceOffsetX - m_OffsetX + 0.5f);
		int y = (int)(m_VoiceOffsetY - m_OffsetY + 0.5f);

		g_pMatSystemSurface->DisableClipping( true );
		m_VoiceImage.DoPaint( x, y, m_VoiceSizeX, m_VoiceSizeY, 0, m_flVoiceAlpha );
		if (m_bClipToMap)
			g_pMatSystemSurface->DisableClipping( false );
	}
				 
	//  Draw dead guys with a different icon
	if ( isAlive )
	{
		if (isLocalPlayer)
		{
			m_LocalAliveImage.Paint( );
			m_ArrowImage.Paint( yaw );
		}
		else
		{
			int nPlayerClass = pPlayer->GetClass();
			if (pPlayer->InLocalTeam() && m_bHasClassImage[nPlayerClass])
			{
				m_pClassAliveImages[nPlayerClass].Paint( );
				m_ArrowImage.Paint( yaw );
			}
			else
			{
				m_AliveImage.Paint( );
				m_ArrowImage.Paint( yaw );
			}
		}
	}
	else
	{
		m_DeadImage.Paint();
	}

	g_pMatSystemSurface->DisableClipping( false );
}




