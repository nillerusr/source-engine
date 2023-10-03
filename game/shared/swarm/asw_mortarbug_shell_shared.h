#ifndef _INCLUDED_ASW_MORTARBUG_SHELL_SHARED_H
#define _INCLUDED_ASW_MORTARBUG_SHELL_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
#include "c_basecombatcharacter.h"
#define CASW_Mortarbug_Shell C_ASW_Mortarbug_Shell
#define CBaseCombatCharacter C_BaseCombatCharacter
class Beam_t;
#else
#include "basecombatcharacter.h"
#endif

class CSprite;

class CASW_Mortarbug_Shell : public CBaseCombatCharacter
{
	DECLARE_CLASS( CASW_Mortarbug_Shell, CBaseCombatCharacter );
public:
	CASW_Mortarbug_Shell();
	DECLARE_NETWORKCLASS();

#ifdef GAME_DLL

	virtual ~CASW_Mortarbug_Shell();

	DECLARE_DATADESC();

	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual void	ShellFlyThink();
	virtual void	ShellThink();
	virtual void	VShellTouch( CBaseEntity *pOther );
	virtual void	SetFuseLength(float fSeconds);
	virtual void	Detonate();

	static CASW_Mortarbug_Shell *CASW_Mortarbug_Shell::CreateShell( const Vector &vecOrigin, const Vector &vecForward, CBaseEntity *pOwner );

	bool m_bDoScreenShake;

private:
	float	m_flDamage;
	float	m_DmgRadius;
	bool	m_bModelOpening;
	float	m_fDetonateTime;
	Vector  m_vecLastPosition;

#else
	virtual void	PostDataUpdate( DataUpdateType_t updateType );
#endif

	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_MORTAR_SHELL; }	

private:	
	CASW_Mortarbug_Shell( const CASW_Mortarbug_Shell & );
};

#endif // _INCLUDED_ASW_MORTARBUG_SHELL_SHARED_H
