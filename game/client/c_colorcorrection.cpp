//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Color correction entity with simple radial falloff
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"

#include "c_colorcorrection.h"
#include "filesystem.h"
#include "cdll_client_int.h"
#include "colorcorrectionmgr.h"
#include "materialsystem/materialsystemutil.h"
#include "iclientmode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static ConVar mat_colcorrection_disableentities( "mat_colcorrection_disableentities", "0", FCVAR_NONE, "Disable map color-correction entities" );

static ConVar mat_colcorrection_forceentitiesclientside( "mat_colcorrection_forceentitiesclientside", "0", FCVAR_CHEAT, "Forces color correction entities to be updated on the client" );

#ifdef CColorCorrection
#undef CColorCorrection
#endif

IMPLEMENT_CLIENTCLASS_DT(C_ColorCorrection, DT_ColorCorrection, CColorCorrection)
	RecvPropVector( RECVINFO(m_vecOrigin) ),
	RecvPropFloat(  RECVINFO(m_minFalloff) ),
	RecvPropFloat(  RECVINFO(m_maxFalloff) ),
	RecvPropFloat(  RECVINFO(m_flCurWeight) ),
	RecvPropFloat(  RECVINFO(m_flMaxWeight) ),
	RecvPropFloat(  RECVINFO(m_flFadeInDuration) ),
	RecvPropFloat(  RECVINFO(m_flFadeOutDuration) ),
	RecvPropString( RECVINFO(m_netLookupFilename) ),
	RecvPropBool(   RECVINFO(m_bEnabled) ),
	RecvPropBool(   RECVINFO(m_bMaster) ),
	RecvPropBool(   RECVINFO(m_bClientSide) ),
	RecvPropBool(	RECVINFO(m_bExclusive) )
END_RECV_TABLE()


//------------------------------------------------------------------------------
// Constructor, destructor
//------------------------------------------------------------------------------
C_ColorCorrection::C_ColorCorrection()
{
	m_minFalloff = -1.0f;
	m_maxFalloff = -1.0f;
	m_flFadeInDuration = 0.0f;
	m_flFadeOutDuration = 0.0f;
	m_flCurWeight = 0.0f;
	m_flMaxWeight = 1.0f;
	m_netLookupFilename[0] = '\0';
	m_bEnabled = false;
	m_bMaster = false;
	m_bExclusive = false;
	m_CCHandle = INVALID_CLIENT_CCHANDLE;

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; i++ )
	{
		m_bEnabledOnClient[i] = false;
		m_flCurWeightOnClient[i] = 0.0f;
		m_bFadingIn[i] = false;
		m_flFadeStartWeight[i] = 0.0f;
		m_flFadeStartTime[i] = 0.0f;
		m_flFadeDuration[i] = 0.0f;
	}
}

C_ColorCorrection::~C_ColorCorrection()
{
	g_pColorCorrectionMgr->RemoveColorCorrectionEntity( this, m_CCHandle );
}

bool C_ColorCorrection::IsClientSide() const
{
	return m_bClientSide || mat_colcorrection_forceentitiesclientside.GetBool();
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void C_ColorCorrection::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		if ( m_CCHandle == INVALID_CLIENT_CCHANDLE )
		{
			// forming a unique name without extension
			char cleanName[MAX_PATH];
			V_StripExtension( m_netLookupFilename, cleanName, sizeof( cleanName ) );
			char name[MAX_PATH];
			Q_snprintf( name, MAX_PATH, "%s_%d", cleanName, entindex() );

			m_CCHandle = g_pColorCorrectionMgr->AddColorCorrectionEntity( this, name, m_netLookupFilename );
		}
	}
}

//------------------------------------------------------------------------------
// We don't draw...
//------------------------------------------------------------------------------
bool C_ColorCorrection::ShouldDraw()
{
	return false;
}

void C_ColorCorrection::Update( C_BasePlayer *pPlayer, float ccScale )
{
	Assert( m_CCHandle != INVALID_CLIENT_CCHANDLE );

	if ( mat_colcorrection_disableentities.GetInt() )
	{
		// Allow the colorcorrectionui panel (or user) to turn off color-correction entities
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCHandle, 0.0f, m_bExclusive );
		return;
	}

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	bool bEnabled = IsClientSide() ? m_bEnabledOnClient[nSlot] : m_bEnabled;

	// fade weight on client
	if ( IsClientSide() )
	{
		m_flCurWeightOnClient[nSlot] = Lerp( GetFadeRatio( nSlot ), m_flFadeStartWeight[nSlot], m_bFadingIn[nSlot] ? m_flMaxWeight : 0.0f );
	}

	float flCurWeight = IsClientSide() ? m_flCurWeightOnClient[nSlot] : m_flCurWeight;

	if( !bEnabled && flCurWeight == 0.0f )
	{
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCHandle, 0.0f, m_bExclusive );
		return;
	}

	Vector playerOrigin = pPlayer->GetAbsOrigin();

	float weight = 0;
	if ( ( m_minFalloff != -1 ) && ( m_maxFalloff != -1 ) && m_minFalloff != m_maxFalloff )
	{
		float dist = (playerOrigin - m_vecOrigin).Length();
		weight = (dist-m_minFalloff) / (m_maxFalloff-m_minFalloff);
		if ( weight<0.0f ) weight = 0.0f;	
		if ( weight>1.0f ) weight = 1.0f;	
	}

	g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCHandle, flCurWeight * ( 1.0 - weight ) * ccScale, m_bExclusive );
}

void C_ColorCorrection::EnableOnClient( bool bEnable, bool bSkipFade )
{
	if ( !IsClientSide() )
	{
		return;
	}

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	if( m_bEnabledOnClient[nSlot] == bEnable )
	{
		return;
	}

	m_bFadingIn[nSlot] = bEnable;
	m_bEnabledOnClient[nSlot] = bEnable;

	// initialize countdown timer
	m_flFadeStartWeight[nSlot] = m_flCurWeightOnClient[nSlot];
	float flFadeTimeScale = 1.0f;
	if ( m_flMaxWeight != 0.0f )
	{
		flFadeTimeScale = m_flCurWeightOnClient[nSlot] / m_flMaxWeight;
	}

	if ( m_bFadingIn[nSlot] )
	{
		flFadeTimeScale = 1.0f - flFadeTimeScale;
	}

	if ( bSkipFade )
	{
		flFadeTimeScale = 0.0f;
	}

	StartFade( nSlot, flFadeTimeScale * ( m_bFadingIn[nSlot] ? m_flFadeInDuration : m_flFadeOutDuration ) );

	// update the clientside weight once here, in case the fade duration is 0
	m_flCurWeightOnClient[nSlot] = Lerp( GetFadeRatio( nSlot ), m_flFadeStartWeight[nSlot], m_bFadingIn[nSlot] ? m_flMaxWeight : 0.0f );
}

Vector C_ColorCorrection::GetOrigin()
{
	return m_vecOrigin;
}

float C_ColorCorrection::GetMinFalloff()
{
	return m_minFalloff;
}

float C_ColorCorrection::GetMaxFalloff()
{
	return m_maxFalloff;
}

void C_ColorCorrection::SetWeight( float fWeight )
{
	g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCHandle, fWeight, false );
}

void C_ColorCorrection::StartFade( int nSplitScreenSlot, float flDuration )
{
	m_flFadeStartTime[nSplitScreenSlot] = gpGlobals->curtime;
	m_flFadeDuration[nSplitScreenSlot] = MAX( flDuration, 0.0f );
}

float C_ColorCorrection::GetFadeRatio( int nSplitScreenSlot ) const
{
	float flRatio = 1.0f;
	
	if ( m_flFadeDuration[nSplitScreenSlot] != 0.0f )
	{
		flRatio = ( gpGlobals->curtime - m_flFadeStartTime[nSplitScreenSlot] ) / m_flFadeDuration[nSplitScreenSlot];
		flRatio = clamp( flRatio, 0.0f, 1.0f );
	}
	return flRatio;
}

bool C_ColorCorrection::IsFadeTimeElapsed( int nSplitScreenSlot ) const
{
	return	( ( gpGlobals->curtime - m_flFadeStartTime[nSplitScreenSlot] ) > m_flFadeDuration[nSplitScreenSlot] ) ||
			( ( gpGlobals->curtime - m_flFadeStartTime[nSplitScreenSlot] ) < 0.0f );
}

void UpdateColorCorrectionEntities( C_BasePlayer *pPlayer, float ccScale, C_ColorCorrection **pList, int listCount )
{
	for ( int i = 0; i < listCount; i++ )
	{
		pList[i]->Update(pPlayer, ccScale);
	}
}