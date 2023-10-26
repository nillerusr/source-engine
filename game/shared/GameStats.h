//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef GAMESTATS_H
#define GAMESTATS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utldict.h"
#include "tier1/utlbuffer.h"
#include "igamesystem.h"

const int GAMESTATS_VERSION = 1;

enum StatSendType_t
{
	STATSEND_LEVELSHUTDOWN,
	STATSEND_APPSHUTDOWN
};

struct StatsBufferRecord_t
{
	float m_flFrameRate;									// fps
	float m_flServerPing;									// client ping to server

};

#define STATS_WINDOW_SIZE ( 60 * 10 )						// # of records to hold
#define STATS_RECORD_INTERVAL 1								// # of seconds between data grabs. 2 * 300 = every 10 minutes

class CGameStats;

void UpdatePerfStats( void );
void SetGameStatsHandler( CGameStats *pGameStats );

class CBasePlayer;
class CPropVehicleDriveable;
class CTakeDamageInfo;

#ifdef GAME_DLL

#define GAMESTATS_STANDARD_NOT_SAVED 0xFEEDBEEF

enum GameStatsVersions_t
{
	GAMESTATS_FILE_VERSION_OLD = 001,
	GAMESTATS_FILE_VERSION_OLD2,
	GAMESTATS_FILE_VERSION_OLD3,
	GAMESTATS_FILE_VERSION_OLD4,
	GAMESTATS_FILE_VERSION_OLD5,
	GAMESTATS_FILE_VERSION
};

struct BasicGameStatsRecord_t
{
};

struct BasicGameStats_t
{
};
#endif // GAME_DLL

class CBaseGameStats 
{
public:
	CBaseGameStats() { }

	// override this to declare what format you want to send.  New products should use new format.
	virtual bool UseOldFormat() 
	{ 
#ifdef GAME_DLL
		return true;		// servers by default send old format for backward compat
#else
		return false;		// clients never used old format so no backward compat issues, they use new format by default
#endif
	}

	// Implement this if you support new format gamestats.
	// Return true if you added data to KeyValues, false if you have no data to report
	virtual bool AddDataForSend( KeyValues *pKV, StatSendType_t sendType ) { return false; }

	// These methods used for new format gamestats only and control when data gets sent.
	virtual bool ShouldSendDataOnLevelShutdown()
	{
		// by default, servers send data at every level change and clients don't
#ifdef GAME_DLL
		return true;
#else
		return false;
#endif
	}
	virtual bool ShouldSendDataOnAppShutdown()
	{
		// by default, clients send data at app shutdown and servers don't
#ifdef GAME_DLL
		return false;
#else
		return true;
#endif
	}

	virtual void Event_Init( void ) { }
	virtual void Event_Shutdown( void ) { }
	virtual void Event_MapChange( const char *szOldMapName, const char *szNewMapName ) { }
	virtual void Event_LevelInit( void ) { }
	virtual void Event_LevelShutdown( float flElapsed ) { }
	virtual void Event_SaveGame( void ) { }
	virtual void Event_LoadGame( void ) { }

	void StatsLog( char const *fmt, ... ) { }

	// This is the first call made, so that we can "subclass" the CBaseGameStats based on gamedir as needed (e.g., ep2 vs. episodic)
	virtual CBaseGameStats *OnInit( CBaseGameStats *pCurrentGameStats, char const *gamedir ) { return pCurrentGameStats; }

	// Frees up data from gamestats and resets it to a clean state.
	virtual void Clear( void ) { }

	virtual bool StatTrackingEnabledForMod( void ) { return false; } //Override this to turn on the system. Stat tracking is disabled by default and will always be disabled at the user's request
	static bool StatTrackingAllowed( void ) { } //query whether stat tracking is possible and warranted by the user
	virtual bool HaveValidData( void ) { return true; } // whether we currently have an interesting enough data set to upload.  Called at upload time { } if false, data is not uploaded.

	virtual bool ShouldTrackStandardStats( void ) { return true; } //exactly what was tracked for EP1 release

	//Get mod specific strings used for tracking, defaults should work fine for most cases
	virtual const char *GetStatSaveFileName( void ) { return ""; }
	virtual const char *GetStatUploadRegistryKeyName( void ) { return ""; }
	const char *GetUserPseudoUniqueID( void ) { }

	virtual bool UserPlayedAllTheMaps( void ) { return false; } //be sure to override this to determine user completion time

#ifdef GAME_DLL
	virtual void Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info ) { }	
	virtual void Event_PlayerConnected( CBasePlayer *pBasePlayer ) { }
	virtual void Event_PlayerDisconnected( CBasePlayer *pBasePlayer ) { }
	virtual void Event_PlayerDamage( CBasePlayer *pBasePlayer, const CTakeDamageInfo &info ) { }
	virtual void Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info ) { }
	virtual void Event_Credits() { }
	virtual void Event_Commentary() { }
	virtual void Event_CrateSmashed() { }
	virtual void Event_Punted( CBaseEntity *pObject ) { }
	virtual void Event_PlayerTraveled( CBasePlayer *pBasePlayer, float distanceInInches, bool bInVehicle, bool bSprinting ) { }
	virtual void Event_WeaponFired( CBasePlayer *pShooter, bool bPrimary, char const *pchWeaponName ) { }
	virtual void Event_WeaponHit( CBasePlayer *pShooter, bool bPrimary, char const *pchWeaponName, const CTakeDamageInfo &info ) { }
	virtual void Event_FlippedVehicle( CBasePlayer *pDriver, CPropVehicleDriveable *pVehicle ) { }
	virtual void Event_PreSaveGameLoaded( char const *pSaveName, bool bInGame ) { }
	virtual void Event_PlayerEnteredGodMode( CBasePlayer *pBasePlayer ) { }
	virtual void Event_PlayerEnteredNoClip( CBasePlayer *pBasePlayer ) { }
	virtual void Event_DecrementPlayerEnteredNoClip( CBasePlayer *pBasePlayer ) { }
	virtual void Event_IncrementCountedStatistic( const Vector& vecAbsOrigin, char const *pchStatisticName, float flIncrementAmount ) { }	
	//custom data to tack onto existing stats if you're not doing a complete overhaul
	virtual void AppendCustomDataToSaveBuffer( CUtlBuffer &SaveBuffer ) { } //custom data you want thrown into the default save and upload path
	virtual void LoadCustomDataFromBuffer( CUtlBuffer &LoadBuffer ) { } //when loading the saved stats file, this will point to where you started saving data to the save buffer

	virtual void LoadingEvent_PlayerIDDifferentThanLoadedStats( void ) { } //Only called if you use the base SaveToFileNOW() and LoadFromFile() functions. Used in case you want to keep/invalidate data that was just loaded. 

	virtual bool LoadFromFile( void ) { return false; } //called just before Event_Init()
	virtual bool SaveToFileNOW( bool bForceSyncWrite = false ) { return false; } //saves buffers to their respective files now, returns success or failure
	virtual bool UploadStatsFileNOW( void ) { return false; } //uploads data to the CSER now, returns success or failure

	static bool AppendLump( int nMaxLumpCount, CUtlBuffer &SaveBuffer, unsigned short iLump, unsigned short iLumpCount, size_t nSize, void *pData ) { return false; }
	static bool GetLumpHeader( int nMaxLumpCount, CUtlBuffer &LoadBuffer, unsigned short &iLump, unsigned short &iLumpCount, bool bPermissive = false ) { return false; }
	static void LoadLump( CUtlBuffer &LoadBuffer, unsigned short iLumpCount, size_t nSize, void *pData ) { }

	//default save behavior is to save on level shutdown, and game shutdown
	virtual bool AutoSave_OnInit( void ) { return false; }
	virtual bool AutoSave_OnShutdown( void ) { return true; }
	virtual bool AutoSave_OnMapChange( void ) { return false; }
	virtual bool AutoSave_OnLevelInit( void ) { return false; }
	virtual bool AutoSave_OnLevelShutdown( void ) { return true; }

	//default upload behavior is to upload on game shutdown
	virtual bool AutoUpload_OnInit( void ) { return false; }
	virtual bool AutoUpload_OnShutdown( void ) { return true; }
	virtual bool AutoUpload_OnMapChange( void ) { return false; }
	virtual bool AutoUpload_OnLevelInit( void ) { return false; }
	virtual bool AutoUpload_OnLevelShutdown( void ) { return false; }

	// Helper for builtin stuff
	void SetSteamStatistic( bool bUsingSteam ) { }
	void SetCyberCafeStatistic( bool bIsCyberCafeUser ) { }
	void SetHDRStatistic( bool bHDREnabled ) { }
	void SetCaptionsStatistic( bool bClosedCaptionsEnabled ) { }
	void SetSkillStatistic( int iSkillSetting ) { }
	void SetDXLevelStatistic( int iDXLevel ) { }
#endif // GAMEDLL
public:
#ifdef GAME_DLL
	BasicGameStats_t m_BasicStats; //exposed in case you do a complete overhaul and still want to save it
#endif
	bool			m_bLogging : 1;
	bool			m_bLoggingToFile : 1;
};

extern CBaseGameStats *gamestats; //starts out pointing at a singleton of the class above, overriding this in any constructor should work for replacing it

#endif // GAMESTATS_H
