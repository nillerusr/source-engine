#ifndef _INCLUDED_ASW_WEAPON_JUMP_JET_H
#define _INCLUDED_ASW_WEAPON_JUMP_JET_H
#pragma once

#include "asw_weapon_blink.h"

#ifdef CLIENT_DLL
#define CASW_Weapon_Jump_Jet C_ASW_Weapon_Jump_Jet
#else
#endif

class CASW_Weapon_Jump_Jet : public CASW_Weapon_Blink
{
public:
	DECLARE_CLASS( CASW_Weapon_Jump_Jet, CASW_Weapon_Blink );
	DECLARE_NETWORKCLASS();

#ifdef CLIENT_DLL
	DECLARE_PREDICTABLE();
#endif

	CASW_Weapon_Jump_Jet();
	virtual ~CASW_Weapon_Jump_Jet();

	virtual void Spawn();
	virtual void PrimaryAttack();
	
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_JUMP_JET; }
	virtual int GetChargesForHUD() { return Clip1(); }

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();
	#else
	#endif

	void DoJumpJet();
};


#endif /* _INCLUDED_ASW_WEAPON_JUMP_JET_H */
