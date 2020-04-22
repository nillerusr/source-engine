//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Bomb Target Area ent
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "triggers.h"

class CBombTarget : public CBaseTrigger
{
public:
	DECLARE_CLASS( CBombTarget, CBaseTrigger );
	DECLARE_DATADESC();

	CBombTarget();

	void Spawn();
	void EXPORT BombTargetTouch( CBaseEntity* pOther );
	void EXPORT BombTargetUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void OnBombExplode( inputdata_t &inputdata );
	void OnBombPlanted( inputdata_t &inputdata );
	void OnBombDefused( inputdata_t &inputdata );

	bool	IsHeistBombTarget( void ) { return m_bIsHeistBombTarget; }
	const char *GetBombMountTarget( void ){ return STRING( m_szMountTarget ); }

private:
	COutputEvent m_OnBombExplode;	//Fired when the bomb explodes
	COutputEvent m_OnBombPlanted;	//Fired when the bomb is planted
	COutputEvent m_OnBombDefused;	//Fired when the bomb is defused

	bool		m_bIsHeistBombTarget;
	string_t	m_szMountTarget;
};