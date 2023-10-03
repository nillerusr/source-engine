#ifndef _DEFINED_ASW_SIMPLE_GRUB_H
#define _DEFINED_ASW_SIMPLE_GRUB_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_simple_alien.h"

class CASW_Simple_Grub : public CASW_Simple_Alien
{
public:
	DECLARE_CLASS( CASW_Simple_Grub, CASW_Simple_Alien  );	
	//DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	CASW_Simple_Grub();
	virtual ~CASW_Simple_Grub();

	virtual void Spawn();
	virtual void Precache();
	virtual void AlienThink();
	virtual Class_T Classify( void ) { return (Class_T) CLASS_ASW_GRUB; }	

	// anims
	virtual void PlayRunningAnimation();
	virtual void PlayAttackingAnimation();
	virtual void PlayFallingAnimation();

	// movement
	virtual float GetIdealSpeed() const;
	virtual float GetPitchSpeed() const;
	virtual float GetIdealPitch();
	virtual void UpdatePitch(float delta);
	virtual float GetZigZagChaseDistance() const;
	virtual float GetFaceEnemyDistance() const;
	virtual bool TryMove(const Vector &vecSrc, Vector &vecTarget, float deltatime, bool bStepMove = false);
	virtual bool ApplyGravity(Vector &vecTarget, float deltatime);
	bool m_bSkipGravity;

	void GrubTouch( CBaseEntity *pOther );

	// sounds
	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void AlertSound();
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual void AttackSound();

	// health related
	bool ShouldGib( const CTakeDamageInfo &info );
	bool CorpseGib( const CTakeDamageInfo &info );
	void Event_Killed( const CTakeDamageInfo &info );
};


#endif // _DEFINED_ASW_SIMPLE_GRUB_H
