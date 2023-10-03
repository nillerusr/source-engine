//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"

#include "GameUI/IGameUI.h"
#include "fmtstr.h"
#include "igameevents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// See interface.h/.cpp for specifics:  basically this ensures that we actually Sys_UnloadModule the dll and that we don't call Sys_LoadModule 
//  over and over again.
static CDllDemandLoader g_GameUI( "gameui" );

#ifndef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointBonusMapsAccessor : public CPointEntity
{
public:
	DECLARE_CLASS( CPointBonusMapsAccessor, CPointEntity );
	DECLARE_DATADESC();

	virtual void	Activate( void );

	void InputUnlock( inputdata_t& inputdata );
	void InputComplete( inputdata_t& inputdata );
	void InputSave( inputdata_t& inputdata );

private:
	string_t	m_String_tFileName;
	string_t	m_String_tMapName;
	IGameUI		*m_pGameUI;
};

BEGIN_DATADESC( CPointBonusMapsAccessor )
	DEFINE_KEYFIELD( m_String_tFileName, FIELD_STRING, "filename" ),
	DEFINE_KEYFIELD( m_String_tMapName, FIELD_STRING, "mapname" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Unlock", InputUnlock ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Complete", InputComplete ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Save", InputSave ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( point_bonusmaps_accessor, CPointBonusMapsAccessor );

void CPointBonusMapsAccessor::Activate( void )
{
	BaseClass::Activate();

	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		m_pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
	}
}

void CPointBonusMapsAccessor::InputUnlock( inputdata_t& inputdata )
{
#if 0
	if ( m_pGameUI )
	{
		m_pGameUI->BonusMapUnlock( m_String_tFileName.ToCStr(), m_String_tMapName.ToCStr() );
	}
#endif
}

void CPointBonusMapsAccessor::InputComplete( inputdata_t& inputdata )
{
	if ( m_pGameUI )
	{
		int iNumAdvancedComplete = 0;

#if 0
		m_pGameUI->BonusMapComplete( m_String_tFileName.ToCStr(), m_String_tMapName.ToCStr() );
		iNumAdvancedComplete = m_pGameUI->BonusMapNumAdvancedCompleted();
#endif

		IGameEvent *event = gameeventmanager->CreateEvent( "advanced_map_complete" );
		if ( event )
		{
			event->SetInt( "numadvanced", iNumAdvancedComplete );
			gameeventmanager->FireEvent( event );
		}
	}
}

void CPointBonusMapsAccessor::InputSave( inputdata_t& inputdata )
{
#if 0
	if ( m_pGameUI )
		m_pGameUI->BonusMapDatabaseSave();
#endif
}

#endif

void BonusMapChallengeUpdate( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest )
{
	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
		if ( pGameUI )
		{
			int piNumMedals[ 3 ] = {0};

#if 0
			pGameUI->BonusMapChallengeUpdate( pchFileName, pchMapName, pchChallengeName, iBest );
			pGameUI->BonusMapNumMedals( piNumMedals );
#endif

			IGameEvent *event = gameeventmanager->CreateEvent( "challenge_map_complete" );
			if ( event )
			{
				event->SetInt( "numbronze", piNumMedals[ 0 ] );
				event->SetInt( "numsilver", piNumMedals[ 1 ] );
				event->SetInt( "numgold", piNumMedals[ 2 ] );
				gameeventmanager->FireEvent( event );
			}
		}	
	}
}

void BonusMapChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName )
{
#if 0
	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
		if ( pGameUI )
		{
			pGameUI->BonusMapChallengeNames( pchFileName, pchMapName, pchChallengeName );
		}	
	}
#endif
}

void BonusMapChallengeObjectives( int &iBronze, int &iSilver, int &iGold )
{
#if 0
	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
		if ( pGameUI )
		{
			pGameUI->BonusMapChallengeObjectives( iBronze, iSilver, iGold );
		}
	}
#endif
}
