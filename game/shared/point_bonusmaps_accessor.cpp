//========= Copyright Valve Corporation, All rights reserved. ============//
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
#ifdef MAPBASE
#include "filesystem.h"
#include "saverestore.h"
#endif
#ifdef GAMEPADUI
#include "../gamepadui/igamepadui.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// See interface.h/.cpp for specifics:  basically this ensures that we actually Sys_UnloadModule the dll and that we don't call Sys_LoadModule 
//  over and over again.
static CDllDemandLoader g_GameUI( "GameUI" );
#if defined(GAMEPADUI) && defined(CLIENT_DLL)
extern IGamepadUI *g_pGamepadUI;
#endif

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
	if ( m_pGameUI )
		m_pGameUI->BonusMapUnlock( m_String_tFileName.ToCStr(), m_String_tMapName.ToCStr() );	
}

void CPointBonusMapsAccessor::InputComplete( inputdata_t& inputdata )
{
	if ( m_pGameUI )
	{
		m_pGameUI->BonusMapComplete( m_String_tFileName.ToCStr(), m_String_tMapName.ToCStr() );

		int iNumAdvancedComplete = m_pGameUI->BonusMapNumAdvancedCompleted();

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
	if ( m_pGameUI )
		m_pGameUI->BonusMapDatabaseSave();
}

#endif

#ifdef MAPBASE
void CustomBMSystem_UpdateChallenges( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest );

bool CustomBMSystem_OverridingInterface();
void CustomBMSystem_BonusMapChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName );
void CustomBMSystem_BonusMapChallengeObjectives( int &iBronze, int &iSilver, int &iGold );
#endif

void BonusMapChallengeUpdate( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest )
{
	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
		if ( pGameUI )
		{
			pGameUI->BonusMapChallengeUpdate( pchFileName, pchMapName, pchChallengeName, iBest );

			int piNumMedals[ 3 ];
			pGameUI->BonusMapNumMedals( piNumMedals );

			IGameEvent *event = gameeventmanager->CreateEvent( "challenge_map_complete" );
			if ( event )
			{
				event->SetInt( "numbronze", piNumMedals[ 0 ] );
				event->SetInt( "numsilver", piNumMedals[ 1 ] );
				event->SetInt( "numgold", piNumMedals[ 2 ] );
				gameeventmanager->FireEvent( event );
			}

#ifdef MAPBASE
			CustomBMSystem_UpdateChallenges( pchFileName, pchMapName, pchChallengeName, iBest );
#endif
		}	
	}
}

void BonusMapChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName )
{
#ifdef MAPBASE
	// This accounts for cases where challenges are "skipped", which causes the data from GameUI to get desynced
	if (CustomBMSystem_OverridingInterface())
	{
		CustomBMSystem_BonusMapChallengeNames( pchFileName, pchMapName, pchChallengeName );
		return;
	}
#endif

	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
		if ( pGameUI )
		{
			pGameUI->BonusMapChallengeNames( pchFileName, pchMapName, pchChallengeName );
		}	
	}
}

void BonusMapChallengeObjectives( int &iBronze, int &iSilver, int &iGold )
{
#ifdef MAPBASE
	// This accounts for cases where challenges are "skipped", which causes the data from GameUI to get desynced
	if (CustomBMSystem_OverridingInterface())
	{
		CustomBMSystem_BonusMapChallengeObjectives( iBronze, iSilver, iGold );
		return;
	}
#endif

	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
		if ( pGameUI )
		{
			pGameUI->BonusMapChallengeObjectives( iBronze, iSilver, iGold );
		}
	}
}

#ifdef MAPBASE
#ifdef CLIENT_DLL
// This is so that the client can access the bonus challenge before it's moved to the player without any complex networking.
ConVar sv_bonus_challenge( "sv_bonus_challenge", "0", FCVAR_REPLICATED | FCVAR_HIDDEN );
#else
extern ConVar sv_bonus_challenge;
#endif

//-----------------------------------------------------------------------------
// Purpose: A wrapper-like system which tries to make up for the bonus maps framework being hardcoded into GameUI.
// 			At the moment, this system mostly involves restoring bonus challenges and allowing them to be used in mods.
// 
// This aims to resolve or work around the following specific issues:
// 
// - A limitation where challenge types cannot go beyond the number of image boxes defined in the .res file.
//      - Not a complete workaround. They still have to occupy a valid image box, but different types can be used for one image.
// - A bug where "skipped" challenge types in a level will cause a desync between the GameUI and the player/cvar's value.
// - A bug where challenges with negative scores do not count as completed.
//      - Negative scores themselves are a workaround for allowing challenges which require the "most of" something rather than the "least of" something,
// 		  as the bonus maps framework normally only recognizes the latter.
//      - May not appear immediately since this fix can be overwritten by GameUI under some circumstances.
// - A bug/limitation where challenge information didn't save/restore after restarting the game.
// 
// The system also offers the following enhancements:
// 
// - Two new game events for monitoring challenge progress for maps and challenge types in particular. They're useful
// if you want to have achievements (or at least progress monitoring) for specific maps or challenge types, as the
// existing events only covered all challenges in the game.
// - VScript functions for accessing bonus map and challenge values.
// 
//-----------------------------------------------------------------------------
class CCustomBonusMapSystem : public CAutoGameSystem
{
public:
	DECLARE_DATADESC();

	CCustomBonusMapSystem() : CAutoGameSystem( "CCustomBonusMapSystem" )
	{
		m_pKV_CurrentBonusData = NULL;
		m_pKV_SaveData = NULL;
		m_bUpdatedChallenges = false;
	}

	virtual bool Init()
	{
		return true;
	}

	virtual void Shutdown()
	{
		RefreshSaveAdjustments();
	}

	virtual void LevelInitPreEntity()
	{
		CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
		if ( gameUIFactory )
		{
			m_pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
		}

#if defined(GAMEPADUI) && defined(CLIENT_DLL)
		if (g_pGamepadUI && sv_bonus_challenge.GetInt() != 0)
		{
			g_pGamepadUI->BonusMapChallengeNames( m_szChallengeFileName, m_szChallengeMapName, m_szChallengeName );
			g_pGamepadUI->BonusMapChallengeObjectives( m_iBronze, m_iSilver, m_iGold );
			LoadBonusDataKV( m_szChallengeFileName );
			m_bOverridingInterface = true;

			//VerifyChallengeValues( sv_bonus_challenge.GetInt() );

			// Get them onto the server
			engine->ClientCmd_Unrestricted( VarArgs( "_set_sv_bonus %i \"%s\" \"%s\" \"%s\" %i %i %i\n",
				sv_bonus_challenge.GetInt(), m_szChallengeFileName, m_szChallengeMapName, m_szChallengeName,
				m_iBronze, m_iSilver, m_iGold ) );
		}
		else
#endif
		// Get the GameUI's values to override its interface (covers cases where GameUI's challenge information has desynced)
		if (m_pGameUI)
		{
			m_pGameUI->BonusMapChallengeNames( m_szChallengeFileName, m_szChallengeMapName, m_szChallengeName );
			m_pGameUI->BonusMapChallengeObjectives( m_iBronze, m_iSilver, m_iGold );
			LoadBonusDataKV( m_szChallengeFileName );

//#ifndef CLIENT_DLL
			if (sv_bonus_challenge.GetInt() != 0)
			{
				VerifyChallengeValues( sv_bonus_challenge.GetInt() );
			}
//#endif
		}
	}

	virtual void LevelInitPostEntity()
	{
	}

	virtual void LevelShutdownPreEntity()
	{
	}

	virtual void LevelShutdownPostEntity()
	{
		// Destroy KV
		if (m_pKV_CurrentBonusData)
		{
			m_pKV_CurrentBonusData->deleteThis();
			m_pKV_CurrentBonusData = NULL;
		}

		if (m_pKV_SaveData)
		{
			m_pKV_SaveData->deleteThis();
			m_pKV_SaveData = NULL;
		}

		m_bUpdatedChallenges = false;
		m_bOverridingInterface = false;

		RefreshSaveAdjustments();
	}

	virtual void OnRestore()
	{
		// No longer necessary due to datadesc and save/restore handler
		/*
#ifdef CLIENT_DLL
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
#else
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
#endif
		if (pPlayer && pPlayer->GetBonusChallenge() != 0)
		{
			if (m_pGameUI)
			{
				m_pGameUI->BonusMapChallengeNames( m_szChallengeFileName, m_szChallengeMapName, m_szChallengeName );
				m_pGameUI->BonusMapChallengeObjectives( m_iBronze, m_iSilver, m_iGold );
				LoadBonusDataKV( m_szChallengeFileName );

				VerifyChallengeValues( pPlayer->GetBonusChallenge() );
			}
		}
		*/
	}

	//------------------------------------------------------------------------------------
	
	void LoadBonusDataKV( const char *pchFileName )
	{
		// Destroy KV if we have any
		if (m_pKV_CurrentBonusData)
		{
			m_pKV_CurrentBonusData->deleteThis();
			m_pKV_CurrentBonusData = NULL;
		}

		// Reload the database
		m_pKV_CurrentBonusData = new KeyValues( "CurrentBonusMapData" );
		m_pKV_CurrentBonusData->LoadFromFile( g_pFullFileSystem, pchFileName, NULL );
	}
	
	void LoadSaveDataKV( bool bRefreshData = false )
	{
		if (bRefreshData)
		{
			// Destroy KV if we have any
			if (m_pKV_SaveData)
			{
				m_pKV_SaveData->deleteThis();
				m_pKV_SaveData = NULL;
			}

			// Save the database
			m_pGameUI->BonusMapDatabaseSave();
		}
		else if (m_pKV_SaveData)
		{
			return;
		}

		// Reload the database
		m_pKV_SaveData = new KeyValues( "SavedBonusMapData" );
		m_pKV_SaveData->LoadFromFile( g_pFullFileSystem, "save/bonus_maps_data.bmd", "MOD" );
	}

	//------------------------------------------------------------------------------------

	void UpdateChallenges( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest )
	{
		if ( !m_pGameUI || m_bUpdatedChallenges )
			return;

		LoadSaveDataKV( true );

		//----------------------------------------

		Msg( "Updating challenges\n" );

		KeyValues *pBonusFiles = m_pKV_SaveData->FindKey( "bonusfiles" );
		if (!pBonusFiles)
		{
			Warning( "Can't find bonus files\n" );
			return;
		}

		// Can't use FindKey for file paths
		KeyValues *pFile = NULL;
		for (KeyValues *key = pBonusFiles->GetFirstSubKey(); key != NULL; key = key->GetNextKey())
		{
			if ( !Q_stricmp( key->GetName(), pchFileName ) )
			{
				pFile = key;
				break;
			}
		}

		// Identify when all bonus challenges on this map are complete
		if (pFile)
		{
			// Since we have saved data, get a list of this map's challenges
			LoadBonusDataKV( pchFileName );

			CUtlVector<int> iAllScores;

			for (KeyValues *pBDMap = m_pKV_CurrentBonusData; pBDMap != NULL; pBDMap = pBDMap->GetNextKey())
			{
				KeyValues *pChallenges = pBDMap->FindKey( "challenges" );
				if (pChallenges)
				{
					KeyValues *pSaveMap = pFile->FindKey( pBDMap->GetName() );
					if (!pSaveMap)
						continue;

					KeyValues *pThisChallenge = pChallenges->FindKey( pchChallengeName );
					if (pThisChallenge)
					{
						KeyValues *pSavedChallenge = pSaveMap->FindKey( pchChallengeName );
						if (!pSavedChallenge)
						{
							iAllScores.AddToTail( 0 );
							continue;
						}

						// Add to progress vector
						if (pSavedChallenge->GetInt() < pThisChallenge->GetInt( "gold" ))
							iAllScores.AddToTail( 3 );
						else if (pSavedChallenge->GetInt() < pThisChallenge->GetInt( "silver" ))
							iAllScores.AddToTail( 2 );
						else if (pSavedChallenge->GetInt() < pThisChallenge->GetInt( "bronze" ))
							iAllScores.AddToTail( 1 );
						else
							iAllScores.AddToTail( 0 );
					}

					// Only do the rest if it's the map we just completed
					if (!Q_stricmp( pBDMap->GetName(), pchMapName ))
					{
						CUtlDict<int> iScores;

						// Compare all challenges to the saved data
						for (KeyValues *challenge = pChallenges->GetFirstSubKey(); challenge != NULL; challenge = challenge->GetNextKey())
						{
							KeyValues *pSavedChallenge = pSaveMap->FindKey( challenge->GetName() );
							if (!pSavedChallenge)
							{
								iScores.Insert( challenge->GetName(), 0 );
								continue;
							}

							// Add to progress vector
							if (pSavedChallenge->GetInt() < challenge->GetInt("gold"))
								iScores.Insert( challenge->GetName(), 3 );
							else if (pSavedChallenge->GetInt() < challenge->GetInt("silver"))
								iScores.Insert( challenge->GetName(), 2 );
							else if (pSavedChallenge->GetInt() < challenge->GetInt("bronze"))
								iScores.Insert( challenge->GetName(), 1 );
							else
								iScores.Insert( challenge->GetName(), 0 );
						}

						int iNumMedals[3] = { 0, 0, 0 };
						for (unsigned int i = 0; i < iScores.Count(); i++)
						{
							switch (iScores[i])
							{
								// These are supposed to fall through into each other
								case 3:		iNumMedals[2]++; // Gold
								case 2:		iNumMedals[1]++; // Silver
								case 1:		iNumMedals[0]++; // Bronze
							}
						}

						// Fire an expanded map update event with more map-specific values
						// 
						// 	"challenge_map_update"
						// 	{
						// 		"challenge"	"short"
						// 		"challengescore"	"short"
						// 		"challengebest"	"short"
						// 		"numbronze"	"short"
						// 		"numsilver"	"short"
						// 		"numgold"	"short"
						// 	}
						// 
						IGameEvent *event = gameeventmanager->CreateEvent( "challenge_map_update", true );
						if ( event )
						{

							Msg("CHALLENGE MAP UPDATE:\n	Map: %s, Challenge: %s\nChallenge Score: %i\n	Ratio Bronze: %f, Ratio Silver: %f, Ratio Gold: %f\n",
								pchMapName, pchChallengeName, iScores.Find( pchChallengeName ),
								(float)iNumMedals[0] / (float)iScores.Count(), (float)iNumMedals[1] / (float)iScores.Count(), (float)iNumMedals[2] / (float)iScores.Count() );

#ifdef CLIENT_DLL
							C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
#else
							CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
#endif

							event->SetInt( "challenge", pPlayer ? pPlayer->GetBonusChallenge() : -1 );
							event->SetInt( "challengescore", iBest );
							event->SetInt( "challengebest", iScores.Find( pchChallengeName ) );
							event->SetInt( "numbronze", iNumMedals[0] );
							event->SetInt( "numsilver", iNumMedals[1] );
							event->SetInt( "numgold", iNumMedals[2] );
#ifdef CLIENT_DLL
							gameeventmanager->FireEventClientSide( event );
#else
							gameeventmanager->FireEvent( event );
#endif
						}
					}
				}


				//if (pSaveMap->FindKey( "complete", false ))
			}

			// Fire an event for the challenge itself
			// 	
			// 	"challenge_update"
			// 	{
			// 		"challenge"	"short"
			// 		"numbronze"	"short"
			// 		"numsilver"	"short"
			// 		"numgold"	"short"
			// 	}
			// 
			IGameEvent *event = gameeventmanager->CreateEvent( "challenge_update", true );
			if ( event )
			{
				int iNumMedals[3] = { 0, 0, 0 };
				for (int i = 0; i < iAllScores.Count(); i++)
				{
					switch (iAllScores[i])
					{
						// These are supposed to fall through into each other
						case 3:		iNumMedals[2]++; // Gold
						case 2:		iNumMedals[1]++; // Silver
						case 1:		iNumMedals[0]++; // Bronze
					}
				}

				Msg("CHALLENGE UPDATE:\n	Ratio Bronze: %f, Ratio Silver: %f, Ratio Gold: %f\n",
					(float)iNumMedals[0] / (float)iAllScores.Count(), (float)iNumMedals[1] / (float)iAllScores.Count(), (float)iNumMedals[2] / (float)iAllScores.Count() );

#ifdef CLIENT_DLL
				C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
#else
				CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
#endif

				event->SetInt( "challenge", pPlayer ? pPlayer->GetBonusChallenge() : -1 );
				event->SetInt( "numbronze", iNumMedals[0] );
				event->SetInt( "numsilver", iNumMedals[1] );
				event->SetInt( "numgold", iNumMedals[2] );
#ifdef CLIENT_DLL
				gameeventmanager->FireEventClientSide( event );
#else
				gameeventmanager->FireEvent( event );
#endif
			}
		}

		m_bUpdatedChallenges = true;
	}

	void RefreshSaveAdjustments()
	{
		if ( !m_pGameUI )
			return;

		LoadSaveDataKV( true );

		//----------------------------------------

		KeyValues *pBonusFiles = m_pKV_SaveData->FindKey( "bonusfiles" );
		if (!pBonusFiles)
		{
			Warning( "Can't find bonus files\n" );
			return;
		}

		bool bChanged = true;

		// Go through all maps we have save data for
		for (KeyValues *pFile = pBonusFiles->GetFirstSubKey(); pFile != NULL; pFile = pFile->GetNextKey())
		{
			// Get a list of this map's challenges
			LoadBonusDataKV( pFile->GetName() );

			CUtlVector<int> iAllScores;

			for (KeyValues *pBDMap = m_pKV_CurrentBonusData; pBDMap != NULL; pBDMap = pBDMap->GetNextKey())
			{
				KeyValues *pChallenges = pBDMap->FindKey( "challenges" );
				if (pChallenges)
				{
					KeyValues *pSaveMap = pFile->FindKey( pBDMap->GetName() );
					if (!pSaveMap)
						continue;

					CUtlDict<int> iScores;

					// Compare all challenges to the saved data
					for (KeyValues *challenge = pChallenges->GetFirstSubKey(); challenge != NULL; challenge = challenge->GetNextKey())
					{
						KeyValues *pSavedChallenge = pSaveMap->FindKey( challenge->GetName() );
						if (!pSavedChallenge)
						{
							iScores.Insert( challenge->GetName(), 0 );
							continue;
						}

						// Add to progress vector
						if (pSavedChallenge->GetInt() < challenge->GetInt("gold"))
							iScores.Insert( challenge->GetName(), 3 );
						else if (pSavedChallenge->GetInt() < challenge->GetInt("silver"))
							iScores.Insert( challenge->GetName(), 2 );
						else if (pSavedChallenge->GetInt() < challenge->GetInt("bronze"))
							iScores.Insert( challenge->GetName(), 1 );
						else
							iScores.Insert( challenge->GetName(), 0 );
					}

					int iNumMedals[3] = { 0, 0, 0 };
					for (unsigned int i = 0; i < iScores.Count(); i++)
					{
						switch (iScores[i])
						{
							// These are supposed to fall through into each other
							case 3:		iNumMedals[2]++; // Gold
							case 2:		iNumMedals[1]++; // Silver
							case 1:		iNumMedals[0]++; // Bronze
						}
					}

					// Is the map completed?
					if (iNumMedals[2] >= (int)iScores.Count())
					{
						// Make sure the completion key is there. If it's not, there was a problem with the database code
						// (This typically happens when the scores are negative)
						if (pSaveMap && pSaveMap->FindKey( "complete" ) == NULL)
						{
							// Create the completion key and save
							pSaveMap->SetBool( "complete", true );
							bChanged = true;
						}
					}
				}
			}
		}

		if (bChanged)
			m_pKV_SaveData->SaveToFile( g_pFullFileSystem, "save/bonus_maps_data.bmd", "MOD" );
	}

	//------------------------------------------------------------------------------------

	bool OverridingInterface() { return m_bOverridingInterface; }

	void VerifyChallengeValues( int iTrueChallenge, bool bOverride = false )
	{
		// Use sv_bonus_challenge to find this map and challenge in the file
		for (KeyValues *pBDMap = m_pKV_CurrentBonusData; pBDMap != NULL; pBDMap = pBDMap->GetNextKey())
		{
			if (Q_stricmp( pBDMap->GetName(), m_szChallengeMapName ) != 0)
				continue;
			
			KeyValues *pChallenges = pBDMap->FindKey( "challenges" );
			if (pChallenges)
			{
				KeyValues *pGameUIChallenge = pChallenges->FindKey( m_szChallengeName );
				KeyValues *pConVarChallenge = NULL;

				for (KeyValues *challenge = pChallenges->GetFirstSubKey(); challenge != NULL; challenge = challenge->GetNextKey())
				{
					if (challenge->GetInt( "type", -1 )+1 == iTrueChallenge)
					{
						pConVarChallenge = challenge;
						break;
					}
				}

				// Begin overriding GameUI
				m_bOverridingInterface = true;

				if ((pGameUIChallenge != pConVarChallenge || bOverride) && pConVarChallenge)
				{
					// This indicates there was a desync within GameUI

					//Msg( "****************** OVERRIDING GAME UI BONUS MAP INFO ******************\n%i -> %i --- %s -> %s\n******************************************************\n",
					//	pGameUIChallenge->GetInt( "type" ), iTrueChallenge,
					//	m_szChallengeName, pConVarChallenge->GetName() );

					Msg( "Overriding GameUI bonus map info!!! (%i -> %i) (%s -> %s)\n", pGameUIChallenge->GetInt( "type" ), iTrueChallenge, m_szChallengeName, pConVarChallenge->GetName() );

					Q_strncpy( m_szChallengeName, pConVarChallenge->GetName(), sizeof( m_szChallengeName ) );

					m_iBronze = pConVarChallenge->GetInt( "bronze", 0 );
					m_iSilver = pConVarChallenge->GetInt( "silver", 0 );
					m_iGold = pConVarChallenge->GetInt( "gold", 0 );
				}
			}

			break;
		}
	}

	void SetCustomBonusMapChallengeNames( const char *pchFileName, const char *pchMapName, const char *pchChallengeName )
	{
		Q_strncpy( m_szChallengeFileName, pchFileName, sizeof( m_szChallengeFileName ) );
		Q_strncpy( m_szChallengeMapName, pchMapName, sizeof( m_szChallengeMapName ) );
		Q_strncpy( m_szChallengeName, pchChallengeName, sizeof( m_szChallengeName ) );
		m_bOverridingInterface = true;
	}

	void CustomBonusMapChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName )
	{
		Q_strncpy( pchFileName, m_szChallengeFileName, sizeof( m_szChallengeFileName ) );
		Q_strncpy( pchMapName, m_szChallengeMapName, sizeof( m_szChallengeMapName ) );
		Q_strncpy( pchChallengeName, m_szChallengeName, sizeof( m_szChallengeName ) );
	}

	void SetCustomBonusMapChallengeObjectives( int iBronze, int iSilver, int iGold )
	{
		m_iBronze = iBronze; m_iSilver = iSilver; m_iGold = iGold;
		m_bOverridingInterface = true;
	}

	void CustomBonusMapChallengeObjectives( int &iBronze, int &iSilver, int &iGold )
	{
		iBronze = m_iBronze; iSilver = m_iSilver; iGold = m_iGold;
	}

#ifdef GAME_DLL
	// Allows a challenge to turn into another type in-game, allowing multiple challenges of the same "type" and
	// getting around issues connected to too many challenge types in the dialog
	void ResolveCustomBonusChallenge( CBasePlayer *pPlayer )
	{
		for (KeyValues *pBDMap = m_pKV_CurrentBonusData; pBDMap != NULL; pBDMap = pBDMap->GetNextKey())
		{
			//Msg( "Bonus map data has map %s\n", pBDMap->GetName() );

			// Only do the rest if it's the map we just completed
			if (!Q_stricmp( pBDMap->GetName(), m_szChallengeMapName ))
			{
				KeyValues *pChallenges = pBDMap->FindKey( "challenges" );
				if (pChallenges)
				{
					KeyValues *pThisChallenge = pChallenges->FindKey( m_szChallengeName );
					if (pThisChallenge)
					{
						// Get the hacky "type_game"
						int iTypeGame = pThisChallenge->GetInt( "type_game", 0 );
						if (iTypeGame != 0)
							pPlayer->SetBonusChallenge( iTypeGame );
					}
				}
			}
		}
	}
#endif

	//------------------------------------------------------------------------------------

#ifdef MAPBASE_VSCRIPT
	virtual void RegisterVScript()
	{
		g_pScriptVM->RegisterInstance( this, "BonusMaps" );
	}
#endif

	//------------------------------------------------------------------------------------

	const char *GetChallengeFileName()
	{
		return m_szChallengeFileName;
	}

	const char *GetChallengeMapName()
	{
		return m_szChallengeMapName;
	}

	const char *GetChallengeName()
	{
		return m_szChallengeName;
	}

	int GetBronze()
	{
		return m_iBronze;
	}

	int GetSilver()
	{
		return m_iSilver;
	}

	int GetGold()
	{
		return m_iGold;
	}

	//------------------------------------------------------------------------------------

	inline bool BonusChallengesActive()
	{
#ifdef CLIENT_DLL
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
#else
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
#endif
		if (pPlayer && pPlayer->GetBonusChallenge() != 0)
		{
			return true;
		}
		else
		{
			return sv_bonus_challenge.GetInt() != 0;
		}
	}

private:

	bool	m_bUpdatedChallenges;

	//-------------------------------------------

	bool	m_bOverridingInterface;

	char	m_szChallengeFileName[MAX_PATH];
	char	m_szChallengeMapName[48];
	char	m_szChallengeName[48];

	int		m_iBronze, m_iSilver, m_iGold;

	//-------------------------------------------

	KeyValues	*m_pKV_CurrentBonusData;
	KeyValues	*m_pKV_SaveData;

	IGameUI *m_pGameUI;
#ifdef GAMEPADUI
	IGamepadUI *m_pGamepadUI;
#endif
};

CCustomBonusMapSystem	g_CustomBonusMapSystem;

BEGIN_DATADESC_NO_BASE( CCustomBonusMapSystem )
	DEFINE_FIELD( m_bOverridingInterface, FIELD_BOOLEAN ),

	DEFINE_AUTO_ARRAY( m_szChallengeFileName, FIELD_CHARACTER ),
	DEFINE_AUTO_ARRAY( m_szChallengeMapName, FIELD_CHARACTER ),
	DEFINE_AUTO_ARRAY( m_szChallengeName, FIELD_CHARACTER ),

	DEFINE_FIELD( m_iBronze, FIELD_INTEGER ),
	DEFINE_FIELD( m_iSilver, FIELD_INTEGER ),
	DEFINE_FIELD( m_iGold, FIELD_INTEGER ),
END_DATADESC()

#ifdef MAPBASE_VSCRIPT
BEGIN_SCRIPTDESC_ROOT( CCustomBonusMapSystem, SCRIPT_SINGLETON "A custom bonus map tracking system." )
	DEFINE_SCRIPTFUNC( OverridingInterface, "Returns true if the custom bonus map system is overriding the GameUI interface." )

	DEFINE_SCRIPTFUNC( GetChallengeFileName, "Gets the .bns file this bonus map originates from." )
	DEFINE_SCRIPTFUNC( GetChallengeMapName, "Gets the name of this bonus map from the .bns file. This is the literal name of the bonus map as it appears in the menu, which may be a user-friendly title and/or a localization token." )
	DEFINE_SCRIPTFUNC( GetChallengeName, "If a challenge is active, this gets its name. This is the literal name of the challenge as it appears in the menu, which may be a user-friendly title and/or a localization token." )
	
	DEFINE_SCRIPTFUNC( GetBronze, "If a challenge is active, this gets its Bronze goal value." )
	DEFINE_SCRIPTFUNC( GetSilver, "If a challenge is active, this gets its Silver goal value." )
	DEFINE_SCRIPTFUNC( GetGold, "If a challenge is active, this gets its Gold goal value." )
END_SCRIPTDESC();
#endif

//------------------------------------------------------------------------------------

inline void CustomBMSystem_UpdateChallenges( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest )
{
	g_CustomBonusMapSystem.UpdateChallenges( pchFileName, pchMapName, pchChallengeName, iBest );
}

inline bool CustomBMSystem_OverridingInterface()
{
	return g_CustomBonusMapSystem.OverridingInterface();
}

inline void CustomBMSystem_BonusMapChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName )
{
	g_CustomBonusMapSystem.CustomBonusMapChallengeNames( pchFileName, pchMapName, pchChallengeName );
}

void CustomBMSystem_BonusMapChallengeObjectives( int &iBronze, int &iSilver, int &iGold )
{
	g_CustomBonusMapSystem.CustomBonusMapChallengeObjectives( iBronze, iSilver, iGold );
}

#ifdef GAME_DLL
void ResolveCustomBonusChallenge( CBasePlayer *pPlayer )
{
	g_CustomBonusMapSystem.ResolveCustomBonusChallenge( pPlayer );
}

//------------------------------------------------------------------------------------

void CC_SetSVBonus( const CCommand &args )
{
	sv_bonus_challenge.SetValue( args.Arg( 1 ) );

	// m_szChallengeFileName, m_szChallengeMapName, m_szChallengeName
	g_CustomBonusMapSystem.SetCustomBonusMapChallengeNames( args.Arg( 2 ), args.Arg( 3 ), args.Arg( 4 ) );
	g_CustomBonusMapSystem.SetCustomBonusMapChallengeObjectives( atoi( args.Arg( 5 ) ), atoi( args.Arg( 6 ) ), atoi( args.Arg( 7 ) ) );
	g_CustomBonusMapSystem.LoadBonusDataKV( args.Arg( 2 ) );

	Msg( "Challenge: %s, file name: %s, map name: %s, challenge name: %s\n", args.Arg( 1 ), args.Arg( 2 ), args.Arg( 3 ), args.Arg( 4 ) );

	//g_CustomBonusMapSystem.VerifyChallengeValues( sv_bonus_challenge.GetInt(), true );
}
static ConCommand _set_sv_bonus("_set_sv_bonus", CC_SetSVBonus, "", FCVAR_HIDDEN);

#endif

//-----------------------------------------------------------------------------
// Custom Bonus System Save/Restore
//-----------------------------------------------------------------------------
static short CUSTOM_BONUS_SAVE_RESTORE_VERSION = 1;

class CCustomBonusSaveRestoreBlockHandler : public CDefSaveRestoreBlockHandler
{
public:
	const char *GetBlockName()
	{
		return "CustomBonusMapSystem";
	}

	//---------------------------------

	void Save( ISave *pSave )
	{
		// We only use this to save challenge info at the moment
		bool bDoSave = g_CustomBonusMapSystem.BonusChallengesActive();

		pSave->WriteBool( &bDoSave );
		if ( bDoSave )
		{
			pSave->WriteAll( &g_CustomBonusMapSystem, g_CustomBonusMapSystem.GetDataDescMap() );
		}
	}

	//---------------------------------

	void WriteSaveHeaders( ISave *pSave )
	{
		pSave->WriteShort( &CUSTOM_BONUS_SAVE_RESTORE_VERSION );
	}

	//---------------------------------

	void ReadRestoreHeaders( IRestore *pRestore )
	{
		// No reason why any future version shouldn't try to retain backward compatability. The default here is to not do so.
		short version;
		pRestore->ReadShort( &version );
		m_fDoLoad = ( version == CUSTOM_BONUS_SAVE_RESTORE_VERSION );
	}

	//---------------------------------

	void Restore( IRestore *pRestore, bool createPlayers )
	{
		if ( m_fDoLoad )
		{
			bool bDoRestore = false;

			pRestore->ReadBool( &bDoRestore );
			if ( bDoRestore )
			{
				pRestore->ReadAll( &g_CustomBonusMapSystem, g_CustomBonusMapSystem.GetDataDescMap() );
			}
		}
	}

private:
	bool m_fDoLoad;
};

//-----------------------------------------------------------------------------

CCustomBonusSaveRestoreBlockHandler g_CustomBonusSaveRestoreBlockHandler;

//-------------------------------------

ISaveRestoreBlockHandler *GetCustomBonusSaveRestoreBlockHandler()
{
	return &g_CustomBonusSaveRestoreBlockHandler;
}
#endif
