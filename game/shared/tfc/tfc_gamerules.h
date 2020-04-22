//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The TF Game rules object
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef TFC_GAMERULES_H
#define TFC_GAMERULES_H

#ifdef _WIN32
#pragma once
#endif


#include "teamplay_gamerules.h"
#include "convar.h"
#include "gamevars_shared.h"

#ifdef CLIENT_DLL
	#include "c_baseplayer.h"
#else
	#include "player.h"
#endif


#ifdef CLIENT_DLL
	
	#define CTFCGameRules C_TFCGameRules
	#define CTFCGameRulesProxy C_TFCGameRulesProxy

#else

	extern BOOL no_cease_fire_text;
	extern BOOL cease_fire;

#endif


class CTFCGameRulesProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CTFCGameRulesProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();
};


class CTFCGameRules : public CTeamplayRules
{
public:
	DECLARE_CLASS( CTFCGameRules, CTeamplayRules );

	CTFCGameRules();

	virtual bool ShouldCollide( int collisionGroup0, int collisionGroup1 );
	void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, bool bIgnoreWorld );

	#ifdef CLIENT_DLL

		DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.

	#else

		DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.
		
		virtual ~CTFCGameRules();

		virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );
		virtual void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore );
		virtual void Think();

		virtual const char *GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer );

		bool IsInPreMatch() const;
		float GetPreMatchEndTime() const;	// Returns the time at which the prematch will be over.
		void TFCGoToIntermission();

	private:

#endif


public:
	
	bool CTF_Map;

};

//-----------------------------------------------------------------------------
// Gets us at the team fortress game rules
//-----------------------------------------------------------------------------

inline CTFCGameRules* TFCGameRules()
{
	return static_cast<CTFCGameRules*>(g_pGameRules);
}


extern ConVar mp_fadetoblack;


#endif // TFC_GAMERULES_H
