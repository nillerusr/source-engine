#include "cbase.h"
#include <KeyValues.h>
#include <filesystem.h>
#include "asw_medal_store.h"
#include "asw_medals_shared.h"
#include "asw_equipment_list.h"
#include "c_asw_campaign_save.h"
#include "steam/isteamremotestorage.h"
#include "steam/steam_api.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IFileSystem *filesystem;

C_ASW_Medal_Store g_ClientMedalStore;

C_ASW_Medal_Store* GetMedalStore() { return &g_ClientMedalStore; }

unsigned char g_ucMedalStoreEncryptionKey[8] = { 17, 67, 230, 65, 174, 52, 14, 14 };

C_ASW_Medal_Store::C_ASW_Medal_Store()
{	
	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{
		m_MarineMedals[i].Purge();
		m_OfflineMarineMedals[i].Purge();
	}
	m_PlayerMedals.Purge();
	m_OfflinePlayerMedals.Purge();
	m_bLoaded = false;

	m_iMissionsCompleted = 0;
	m_iCampaignsCompleted = 0;
	m_iAliensKilled = 0;

	m_iOfflineMissionsCompleted = 0;
	m_iOfflineCampaignsCompleted = 0;
	m_iOfflineAliensKilled = 0;
	m_iXP = 0;
	m_iPromotion = 0;
	m_bFoundNewClientDat = false;
}

ConVar asw_steam_cloud( "asw_steam_cloud", "1", FCVAR_NONE, "Whether Swarm data should be stored in the Steam Cloud" );

void C_ASW_Medal_Store::LoadMedalStore()
{
#if defined(NO_STEAM)
	AssertMsg( false, "SteamCloud not available." );
#else
	ISteamRemoteStorage *pRemoteStorage = SteamClient() ? ( ISteamRemoteStorage * )SteamClient()->GetISteamGenericInterface(
		SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMREMOTESTORAGE_INTERFACE_VERSION ) : NULL;
	ISteamUser *pSteamUser = steamapicontext ? steamapicontext->SteamUser() : NULL;
	if ( !pSteamUser )
		return;

	char szMedalFile[ 256 ];
	Q_snprintf( szMedalFile, sizeof( szMedalFile ), "cfg/clientc_%I64u.dat", pSteamUser->GetSteamID().ConvertToUint64() );
	int len = Q_strlen( szMedalFile );
	for ( int i = 0; i < len; i++ )
	{
		if ( szMedalFile[ i ] == ':' )
			szMedalFile[i] = '_';
	}

	if ( asw_steam_cloud.GetBool() && pRemoteStorage )
	{
		if ( !GetFileFromRemoteStorage( pRemoteStorage, "PersistentMarines.dat", szMedalFile ) )
		{
#ifdef _DEBUG
			Warning( "Failed to get client.dat from Steam Cloud.\n" );
#endif
		}
	}
#endif

	// clear out the currently loaded medals, if any
	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{
		m_MarineMedals[i].Purge();
		m_OfflineMarineMedals[i].Purge();
	}
	m_PlayerMedals.Purge();
	m_OfflinePlayerMedals.Purge();

	m_bLoaded = true;

	FileHandle_t f = filesystem->Open( szMedalFile, "rb", "MOD" );
	if ( !f )
		return;		// if we get here, it means the player has no clientc.dat file and therefore no medals

	int fileSize = filesystem->Size(f);
	char *file_buffer = (char*)MemAllocScratch(fileSize + 1);
	Assert(file_buffer);
	filesystem->Read(file_buffer, fileSize, f); // read into local buffer
	file_buffer[fileSize] = 0; // null terminate file as EOF
	filesystem->Close( f );	// close file after reading

	UTIL_DecodeICE( (unsigned char*)file_buffer, fileSize, g_ucMedalStoreEncryptionKey );

	KeyValues *kv = new KeyValues( "CLIENTDAT" );
	if ( !kv->LoadFromBuffer( "CLIENTDAT", file_buffer, filesystem ) )
	{
		return;
	}

	MemFreeScratch();

	m_bFoundNewClientDat = true;

	// pull out missions/campaigns/kills
	m_iMissionsCompleted = kv->GetInt("MC");
	m_iCampaignsCompleted = kv->GetInt("CC");
	m_iAliensKilled = kv->GetInt("AK");

	m_iOfflineMissionsCompleted = kv->GetInt("OMC");
	m_iOfflineCampaignsCompleted = kv->GetInt("OCC");
	m_iOfflineAliensKilled = kv->GetInt("OAK");

	m_iXP = kv->GetInt( "LPL" );
	m_iPromotion = kv->GetInt( "LPP" );

	// new equip
	m_NewEquipment.Purge();
	KeyValues *pkvEquip = kv->FindKey("NEWEQUIP");
	char buffer[64];
	if ( pkvEquip )	
	{
		for ( KeyValues *pKey = pkvEquip->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
		{
			m_NewEquipment.AddToTail( pKey->GetInt() );
		}
	}
	
	// first subsection is player medals
	//KeyValues *pkvPlayerMedals = kv->GetFirstSubKey();
	KeyValues *pkvPlayerMedals = kv->FindKey("LP");
	int iMedalNum = 0;
	if (pkvPlayerMedals)	
	{
		int iMedal = 0;
		while (iMedal != -1)
		{
			Q_snprintf(buffer, sizeof(buffer), "M%d", iMedalNum);			
			iMedal = pkvPlayerMedals->GetInt(buffer, -1);
			if (iMedal != -1 && IsPlayerMedal(iMedal))
			{
				m_PlayerMedals.AddToTail(iMedal);
			}
			iMedalNum++;
		}
	}

	// now go through each marine
	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{
		Q_snprintf(buffer, sizeof(buffer), "LA%d", i);
		KeyValues *pkvMarineMedals = kv->FindKey(buffer);
		if (pkvMarineMedals)
		{
			iMedalNum = 0;
			int iMedal = 0;
			while (iMedal != -1)
			{
				Q_snprintf(buffer, sizeof(buffer), "M%d", iMedalNum);			
				iMedal = pkvMarineMedals->GetInt(buffer, -1);
				if (iMedal != -1 && !IsPlayerMedal(iMedal))
				{
					m_MarineMedals[i].AddToTail(iMedal);
				}
				iMedalNum++;
			}
		}
	}

	// offline medal store
	pkvPlayerMedals = kv->FindKey("FP");
	iMedalNum = 0;
	if (pkvPlayerMedals)	
	{
		int iMedal = 0;
		while (iMedal != -1)
		{
			Q_snprintf(buffer, sizeof(buffer), "M%d", iMedalNum);			
			iMedal = pkvPlayerMedals->GetInt(buffer, -1);
			if (iMedal != -1 && IsPlayerMedal(iMedal))
			{
				m_OfflinePlayerMedals.AddToTail(iMedal);
			}
			iMedalNum++;
		}
	}

	// now go through each marine
	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{
		Q_snprintf(buffer, sizeof(buffer), "FA%d", i);
		KeyValues *pkvMarineMedals = kv->FindKey(buffer);
		if (pkvMarineMedals)
		{
			iMedalNum = 0;
			int iMedal = 0;
			while (iMedal != -1)
			{
				Q_snprintf(buffer, sizeof(buffer), "M%d", iMedalNum);			
				iMedal = pkvMarineMedals->GetInt(buffer, -1);
				if (iMedal != -1 && !IsPlayerMedal(iMedal))
				{
					m_OfflineMarineMedals[i].AddToTail(iMedal);
				}
				iMedalNum++;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: encrypts an 8-byte sequence
//-----------------------------------------------------------------------------


bool C_ASW_Medal_Store::SaveMedalStore()
{
	if ( !m_bLoaded )
		return false;

	KeyValues *kv = new KeyValues( "CLIENTDAT" );
	
	// output missions/campaigns/kills
	kv->SetInt("MC", m_iMissionsCompleted);
	kv->SetInt("CC", m_iCampaignsCompleted);
	kv->SetInt("AK", m_iAliensKilled);

	kv->SetInt("OMC", m_iOfflineMissionsCompleted);
	kv->SetInt("OCC", m_iOfflineCampaignsCompleted);
	kv->SetInt("OAK", m_iOfflineAliensKilled);

	kv->SetInt( "LPL", m_iXP );
	kv->SetInt( "LPP", m_iPromotion );
	
	KeyValues *pSubSection = new KeyValues("NEWEQUIP");
	char buffer[64];
	if (pSubSection)
	{
		for (int i=0;i<m_NewEquipment.Count();i++)
		{			
			pSubSection->SetInt( "EQUIP", m_NewEquipment[i]);
		}
		kv->AddSubKey(pSubSection);
	}	

	// output player medals
	pSubSection = new KeyValues("LP");
	if (pSubSection)
	{
		for (int i=0;i<m_PlayerMedals.Count();i++)
		{			
			Q_snprintf(buffer, sizeof(buffer), "M%d", i);
			pSubSection->SetInt(buffer, m_PlayerMedals[i]);
		}
		kv->AddSubKey(pSubSection);
	}	

	for (int k=0;k<ASW_NUM_MARINE_PROFILES;k++)
	{
		Q_snprintf(buffer, sizeof(buffer), "LA%d", k);
		pSubSection = new KeyValues(buffer);
		if (pSubSection)
		{			
			for (int i=0;i<m_MarineMedals[k].Count();i++)
			{
				Q_snprintf(buffer, sizeof(buffer), "M%d", i);
				pSubSection->SetInt(buffer, m_MarineMedals[k][i]);				
			}
			kv->AddSubKey(pSubSection);
		}		
	}

	// offline medal store
	pSubSection = new KeyValues("FP");
	if (pSubSection)
	{
		for (int i=0;i<m_OfflinePlayerMedals.Count();i++)
		{			
			Q_snprintf(buffer, sizeof(buffer), "M%d", i);
			pSubSection->SetInt(buffer, m_OfflinePlayerMedals[i]);
		}
		kv->AddSubKey(pSubSection);
	}	

	for (int k=0;k<ASW_NUM_MARINE_PROFILES;k++)
	{
		Q_snprintf(buffer, sizeof(buffer), "FA%d", k);
		pSubSection = new KeyValues(buffer);
		if (pSubSection)
		{			
			for (int i=0;i<m_OfflineMarineMedals[k].Count();i++)
			{
				Q_snprintf(buffer, sizeof(buffer), "M%d", i);
				pSubSection->SetInt(buffer, m_OfflineMarineMedals[k][i]);				
			}
			kv->AddSubKey(pSubSection);
		}		
	}

	CUtlBuffer buf; //( 0, 0, CUtlBuffer::TEXT_BUFFER );
	kv->RecursiveSaveToFile( buf, 0 );

	// pad buffer with zeroes to make a multiple of 8
	int nExtra = buf.TellPut() % 8;
	while ( nExtra != 0 && nExtra < 8 )
	{
		buf.PutChar( 0 );
		nExtra++;
	}
	UTIL_EncodeICE( (unsigned char*) buf.Base(), buf.TellPut(), g_ucMedalStoreEncryptionKey );

	ISteamUser *pSteamUser = steamapicontext ? steamapicontext->SteamUser() : NULL;
	if ( !pSteamUser )
		return false;

	char szMedalFile[ 256 ];
	Q_snprintf( szMedalFile, sizeof( szMedalFile ), "cfg/clientc_%I64u.dat", pSteamUser->GetSteamID().ConvertToUint64() );
	int len = Q_strlen( szMedalFile );
	for ( int i = 0; i < len; i++ )
	{
		if ( szMedalFile[ i ] == ':' )
			szMedalFile[i] = '_';
	}

	bool bResult = filesystem->WriteFile( szMedalFile, "MOD", buf );
	if ( bResult )
	{
	#if defined(NO_STEAM)
		AssertMsg( false, "SteamCloud not available." );
	#else
		ISteamRemoteStorage *pRemoteStorage = SteamClient() ? ( ISteamRemoteStorage * )SteamClient()->GetISteamGenericInterface(
			SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMREMOTESTORAGE_INTERFACE_VERSION ) : NULL;

		if ( asw_steam_cloud.GetBool() && pRemoteStorage )
		{
			WriteFileToRemoteStorage( pRemoteStorage, "PersistentMarines.dat", szMedalFile );
		}
	#endif
	}

	return bResult;
}

bool C_ASW_Medal_Store::IsPlayerMedal(int i)
{
	return (i == MEDAL_IAF_TRAINING || i == MEDAL_IAF_COMBAT_HONORS || i == MEDAL_IAF_BATTLE_HONORS
		|| i == MEDAL_IAF_CAMPAIGN_HONORS || i == MEDAL_IAF_WARTIME_SERVICE || i == MEDAL_PROFESSIONAL ||
		i == MEDAL_NEMESIS || i == MEDAL_RETRIBUTION || i == MEDAL_IAF_HERO);
}

// output all the medals in the store
void C_ASW_Medal_Store::DebugInfo()
{
	Msg("Outputting online client medal store:\n");
	Msg("Missions: %d  Campaigns: %d  Kills: %d\n", m_iMissionsCompleted, m_iCampaignsCompleted, m_iAliensKilled);
	Msg("Player Medals:  (%d)\n", m_PlayerMedals.Count());
	for (int i=0;i<m_PlayerMedals.Count();i++)
	{
		Msg("%d ", m_PlayerMedals[i]);
	}
	Msg("\n");

	for (int k=0;k<ASW_NUM_MARINE_PROFILES;k++)
	{
		Msg("Marine %d Medals:  (%d)\n", k, m_MarineMedals[k].Count());
		for (int i=0;i<m_MarineMedals[k].Count();i++)
		{
			Msg("%d ", m_MarineMedals[k][i]);
		}
		Msg("\n");
	}

	Msg("Outputting offline client medal store:\n");
	Msg("Missions: %d  Campaigns: %d  Kills: %d\n", m_iOfflineMissionsCompleted, m_iOfflineCampaignsCompleted, m_iOfflineAliensKilled);
	Msg("Player Medals:  (%d)\n", m_OfflinePlayerMedals.Count());
	for (int i=0;i<m_OfflinePlayerMedals.Count();i++)
	{
		Msg("%d ", m_OfflinePlayerMedals[i]);
	}
	Msg("\n");

	for (int k=0;k<ASW_NUM_MARINE_PROFILES;k++)
	{
		Msg("Marine %d Medals:  (%d)\n", k, m_OfflineMarineMedals[k].Count());
		for (int i=0;i<m_OfflineMarineMedals[k].Count();i++)
		{
			Msg("%d ", m_OfflineMarineMedals[k][i]);
		}
		Msg("\n");
	}
}

// a marine has just been awarded some medals - add to the client store
//    HUMMM - what if the level is cancelled and retried?
// in single mission, this doesn't count, so medals should be awarded there fine.
// in campaign - only matters for outstanding execution and speed run really - but let's only award those if all marines make it there alive
bool C_ASW_Medal_Store::OnAwardedMedals(const char *pszMedalsAwarded, int iProfileIndex, bool bMultiplayer)
{
	if (!m_bLoaded)
	{
		LoadMedalStore();
	}
	
	// break up the medal string into medal numbers
	const char	*p = pszMedalsAwarded;
	char		token[128];
	bool bAddedMedal = false;
	
	p = nexttoken( token, p, ' ' );	
	while ( Q_strlen( token ) > 0 )  
	{
		int iMedalIndex = atoi(token);
		bAddedMedal |= AddMarineMedal(iProfileIndex, iMedalIndex, bMultiplayer);
		if (p)
			p = nexttoken( token, p, ' ' );
		else
			token[0] = '\0';
	}

	if (bAddedMedal)
		SaveMedalStore();

	return bAddedMedal;
}

// the player has been awarded medal(s), save it into our store
bool C_ASW_Medal_Store::OnAwardedPlayerMedals(int iPlayerIndex, const char *pszPlayerMedals, bool bMultiplayer)
{
	if (!m_bLoaded)
	{
		LoadMedalStore();
	}

	if (iPlayerIndex < 0 || iPlayerIndex >= ASW_MAX_READY_PLAYERS)
		return false;

	// NOTE: should only save if the playerindex matches the local player, but we can't check that here?
	//  we'll assume our caller has checked this
	
	// break up the medal string into medal numbers
	const char	*p = pszPlayerMedals;
	char		token[128];
	bool bAddedMedal = false;
	
	p = nexttoken( token, p, ' ' );	
	while ( Q_strlen( token ) > 0 )  
	{
		int iMedalIndex = atoi(token);
		bAddedMedal |= AddPlayerMedal(iMedalIndex, bMultiplayer);
		if (p)
			p = nexttoken( token, p, ' ' );
		else
			token[0] = '\0';		
	}

	if (bAddedMedal)
		SaveMedalStore();

	return bAddedMedal;
}

bool C_ASW_Medal_Store::AddMarineMedal(int iProfileIndex, int iMedal, bool bMultiplayer)
{
	if (iProfileIndex < 0 || iProfileIndex >= ASW_NUM_MARINE_PROFILES)
		return false;

	if (IsPlayerMedal(iMedal))
		return false;

	if (!bMultiplayer)
	{
		// marine already has the medal?
		if (m_OfflineMarineMedals[iProfileIndex].Find(iMedal) != m_OfflineMarineMedals[iProfileIndex].InvalidIndex())
			return false;	

		m_OfflineMarineMedals[iProfileIndex].AddToTail(iMedal);
	}
	else
	{
		// marine already has the medal?
		if (m_MarineMedals[iProfileIndex].Find(iMedal) != m_MarineMedals[iProfileIndex].InvalidIndex())
			return false;	

		m_MarineMedals[iProfileIndex].AddToTail(iMedal);
	}

	return true;
}

bool C_ASW_Medal_Store::AddPlayerMedal(int iMedal, bool bMultiplayer)
{	
	if (!bMultiplayer)
	{
		// player already has the medal?
		if (m_OfflinePlayerMedals.Find(iMedal) != m_OfflinePlayerMedals.InvalidIndex())
			return false;

		if (!IsPlayerMedal(iMedal))
			return false;

		m_OfflinePlayerMedals.AddToTail(iMedal);

		if (iMedal == MEDAL_IAF_TRAINING)	// add to multiplayer listing too, just so the multi collection is completable
		{
			m_PlayerMedals.AddToTail(iMedal);
		}
	}
	else
	{
		// player already has the medal?
		if (m_PlayerMedals.Find(iMedal) != m_PlayerMedals.InvalidIndex())
			return false;

		if (!IsPlayerMedal(iMedal))
			return false;

		m_PlayerMedals.AddToTail(iMedal);
	}

	return true;
}

// clears all the medals/xp this player has found!
void C_ASW_Medal_Store::ClearMedalStore()
{
	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{
		m_MarineMedals[i].Purge();
		m_OfflineMarineMedals[i].Purge();
	}
	m_PlayerMedals.Purge();
	m_OfflinePlayerMedals.Purge();
	m_iXP = 0;
	m_iPromotion = 0;
	m_NewEquipment.Purge();
	SaveMedalStore();
}

bool C_ASW_Medal_Store::HasMedal(int iMedalIndex, bool bOffline, int iMarine)
{
	if (!m_bLoaded)
	{
		LoadMedalStore();
	}

	if (!bOffline)
	{
		if (IsPlayerMedal(iMedalIndex))
		{
			if (iMarine != -1)
				return false;
			int i = m_PlayerMedals.Find(iMedalIndex);
			if (i == m_PlayerMedals.InvalidIndex())
				return false;
			return true;
		}
		for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
		{
			if (i == iMarine || iMarine == -1)
			{
				int h = m_MarineMedals[i].Find(iMedalIndex);
				if (h != m_MarineMedals[i].InvalidIndex())
					return true;
			}
		}
	}
	else
	{
		if (IsPlayerMedal(iMedalIndex))
		{
			if (iMarine != -1)
				return false;
			int i = m_OfflinePlayerMedals.Find(iMedalIndex);
			if (i == m_OfflinePlayerMedals.InvalidIndex())
				return false;
			return true;
		}
		for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
		{
			if (i == iMarine || iMarine == -1)
			{
				int h = m_OfflineMarineMedals[i].Find(iMedalIndex);
				if (h != m_OfflineMarineMedals[i].InvalidIndex())
					return true;
			}
		}
	}
	return false;
}

void reset_xp_f()
{
	if ( GetMedalStore() )
	{
		GetMedalStore()->ClearMedalStore();
	}
}

static ConCommand reset_xp( "reset_xp", reset_xp_f, "Clears your experience", FCVAR_DEVELOPMENTONLY );


void C_ASW_Medal_Store::OnIncreaseCounts(int iAddMission, int iAddCampaign, int iAddKills, bool bOffline)
{
	if (!m_bLoaded)
	{
		LoadMedalStore();
	}

	if (bOffline)
	{
		m_iOfflineMissionsCompleted += iAddMission;
		m_iOfflineCampaignsCompleted += iAddCampaign;
		m_iOfflineAliensKilled += iAddKills;
	}
	else
	{
		m_iMissionsCompleted += iAddMission;
		m_iCampaignsCompleted += iAddCampaign;
		m_iAliensKilled += iAddKills;
	}
	SaveMedalStore();
}

void C_ASW_Medal_Store::GetCounts(int &iMissions, int &iCampaigns, int &iKills, bool bOffline)
{
	if (!m_bLoaded)
	{
		LoadMedalStore();
	}

	if (bOffline)
	{
		iMissions = m_iOfflineMissionsCompleted;
		iCampaigns = m_iOfflineCampaignsCompleted;
		iKills = m_iOfflineAliensKilled;
	}
	else
	{
		iMissions = m_iMissionsCompleted;
		iCampaigns = m_iCampaignsCompleted;
		iKills = m_iAliensKilled;
	}
}

void C_ASW_Medal_Store::OnUnlockedEquipment( const char *pszWeaponUnlockClass )
{
	if (!m_bLoaded)
	{
		LoadMedalStore();
	}
	if ( !ASWEquipmentList() )
		return;

	int nEquipmentListIndex = ASWEquipmentList()->GetRegularIndex( pszWeaponUnlockClass );
	bool bExtraItem = false;
	if ( nEquipmentListIndex == -1 )
	{
		bExtraItem = true;
		nEquipmentListIndex = ASWEquipmentList()->GetExtraIndex( pszWeaponUnlockClass );
		if ( nEquipmentListIndex == -1 )
			return;
	}

	int nIndexHash = nEquipmentListIndex + ( bExtraItem ? 100 : 0 );
	if ( m_NewEquipment.Find( nIndexHash ) == m_NewEquipment.InvalidIndex() )
	{
		m_NewEquipment.AddToTail( nIndexHash );
		SaveMedalStore();
	}
}

void C_ASW_Medal_Store::OnSelectedEquipment( bool bExtraItem, int nEquipmentListIndex )
{
	if (!m_bLoaded)
	{
		LoadMedalStore();
	}
	int nIndexHash = nEquipmentListIndex + ( bExtraItem ? 100 : 0 );
	if ( m_NewEquipment.Find( nIndexHash ) != m_NewEquipment.InvalidIndex() )
	{
		m_NewEquipment.FindAndRemove( nIndexHash );
		SaveMedalStore();
	}
}

bool C_ASW_Medal_Store::IsWeaponNew( bool bExtraItem, int nEquipmentListIndex )
{
	if (!m_bLoaded)
	{
		LoadMedalStore();
	}
	int nIndexHash = nEquipmentListIndex + ( bExtraItem ? 100 : 0 );
	//Msg( "C_ASW_Medal_Store::IsWeaponNew bextra=%d index=%d hash=%d found=%d m_NewEquipmentcount=%d\n", bExtraItem, nEquipmentListIndex, nIndexHash, ( m_NewEquipment.Find( nIndexHash ) != m_NewEquipment.InvalidIndex() ), m_NewEquipment.Count() );
	return ( m_NewEquipment.Find( nIndexHash ) != m_NewEquipment.InvalidIndex() );
}

void C_ASW_Medal_Store::ClearNewWeapons()
{
	if (!m_bLoaded)
	{
		LoadMedalStore();
	}
	m_NewEquipment.RemoveAll();
}

void C_ASW_Medal_Store::SetExperience( int nXP )
{
	if (!m_bLoaded)
	{
		LoadMedalStore();
	}
	m_iXP = nXP;
}

int C_ASW_Medal_Store::GetExperience()
{
	if (!m_bLoaded)
	{
		LoadMedalStore();
	}
	return m_iXP;
}

void C_ASW_Medal_Store::SetPromotion( int nPromotion )
{
	if (!m_bLoaded)
	{
		LoadMedalStore();
	}
	m_iPromotion = nPromotion;
}

int C_ASW_Medal_Store::GetPromotion()
{
	if (!m_bLoaded)
	{
		LoadMedalStore();
	}
	return m_iPromotion;
}

