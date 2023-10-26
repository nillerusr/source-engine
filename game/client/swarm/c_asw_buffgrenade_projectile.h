#ifndef _INCLUDED_C_ASW_BUFFGREN_PROJECTILE_H
#define _INCLUDED_C_ASW_BUFFGREN_PROJECTILE_H
#pragma once


#include "c_asw_aoegrenade_projectile.h"


class C_ASW_BuffGrenade_Projectile : public C_ASW_AOEGrenade_Projectile
{
public:
	DECLARE_CLASS( C_ASW_BuffGrenade_Projectile, C_ASW_AOEGrenade_Projectile );
	DECLARE_CLIENTCLASS();

	virtual Color GetGrenadeColor( void );
	virtual const char* GetLoopSoundName( void ) { return "ASW_BuffGrenade.BuffLoop"; }
	virtual const char* GetStartSoundName( void ) { return "ASW_BuffGrenade.StartBuff"; }
	virtual const char* GetActivateSoundName( void ) { return "ASW_BuffGrenade.GrenadeActivate"; }
	virtual const char* GetPingEffectName( void ) { return "buffgrenade_pulse"; }
	virtual const char* GetArcEffectName( void ) { return "buffgrenade_attach_arc"; }
	virtual const char* GetArcAttachmentName( void ) { return "beam_attach"; }
	virtual bool ShouldAttachEffectToWeapon( void ) { return true; }
	virtual bool ShouldSpawnSphere( void ) { return true; }
	virtual float GetSphereScale( void ) { return 0.98f; }
	virtual int GetSphereSkin( void ) { return 1; }

	EHANDLE m_hSphereModel;
	//float m_flPrevRotAngle;
	float m_flTimeCreated;
};


#endif // _INCLUDED_C_ASW_BUFFGREN_PROJECTILE_H