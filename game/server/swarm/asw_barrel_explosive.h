#ifndef _INCLUDED_ASW_BARREL_EXPLOSIVE_H
#define _INCLUDED_ASW_BARREL_EXPLOSIVE_H
#pragma once

#include "props.h"
#include "asw_prop_physics.h"
#include "asw_shareddefs.h"

// experimental tech to make explosive barrels detonate one after another, 
// rather than all in one frame.
class CDeferredExplosionQueue;

/// number of minimum seconds between explosions
#define ASW_EXPLODING_PROP_INTERVAL (1.0/6.0)

/// one thing of this type is allowed to explode per frame.
/// the rest enqueue themselves. It used to be a standalone
/// IQueuedExplodable interface from which any entity could
/// inherit, but that became problematic because if a class
/// inherits from both CBaseEntity and an interface IFoo,
/// there's no way to go from an EHANDLE to an (IFoo *)
/// We can go back that route if eventually we want to blow
/// up something that is not a prop_physics.
class CASW_Exploding_Prop : public CASW_Prop_Physics
{
protected:
	/// enqueue this object to be exploded. It may explode immediately if nothing else is queued.
	/// "explode" means it'll call the Explode() function.
	/// You can pass along the CDamageInfo that caused the explosion if you need it for ExplodeNow later
	void QueueForExplode( const CTakeDamageInfo &damageInfo = CTakeDamageInfo() );
	/// inheritors must implement this. The queue will call this when it wants you to explode this frame.
	virtual void ExplodeNow( const CTakeDamageInfo &damageInfo ) = 0;

	friend class CDeferredExplosionQueue;
};


class CASW_Barrel_Explosive : public CASW_Exploding_Prop
{
public:
	DECLARE_CLASS( CASW_Barrel_Explosive, CASW_Prop_Physics );
	DECLARE_DATADESC();

	CASW_Barrel_Explosive();
	virtual void Spawn( void );
	virtual void Precache( void );
	
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual void DoExplosion( void );

	virtual void OnDifficultyChanged( int iDifficulty );
	virtual void InitHealth();

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_EXPLOSIVE_BARREL; }

protected:
	// from CASW_Exploding_Prop:
	virtual void ExplodeNow( const CTakeDamageInfo &info );

	int m_iExplosionDamage;
	EHANDLE m_hAttacker;
};


#endif /* _INCLUDED_ASW_BARREL_EXPLOSIVE_H */