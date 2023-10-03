#ifndef _INCLUDED_ASW_ROCKET_H
#define _INCLUDED_ASW_ROCKET_H

#include "basehlcombatweapon.h"

class RocketTrail;

/// This is a class that can handle cases where you have, say, ten projectiles
/// and three enemies, and you'd like each enemy to get just the number of projectiles
/// necessary to kill it. It stores a dictionary of targeted enemies, the total damage
/// to be done so far upon each of them given the allocated projectiles, and a linked
/// list of projectiles (or spell effects or what have you) allocated to each one. 
class CASW_DamageAllocationMgr
{
public:
		// typedefs
	// relevant data about a given projectile: its handle, and the damage it causes
	struct ProjectileInfo_t 
	{	
		EHANDLE m_hHandle;
		float m_flDamage;

		ProjectileInfo_t() : m_flDamage(0) {}
		ProjectileInfo_t( CBaseEntity *pProj, float damage ) : m_hHandle(pProj),
			m_flDamage(damage) {}
	};

	// we store the chain of projectiles allocated to each target 
	// as a linked list. This is actually a MULTILIST, which means
	// that many linked lists actually share the pool of spaces herein
	typedef CUtlLinkedList< ProjectileInfo_t, unsigned short, true, unsigned int > ProjectilePool_t;

	struct tuple_t 
	{
		EHANDLE m_hTargeted;
		float   m_flAccumulatedDamage;
		ProjectilePool_t::IndexType_t m_nProjectiles; ///< head of linked list of projectiles targeted at this object

		tuple_t() : m_flAccumulatedDamage(0), m_nProjectiles( ProjectilePool_t::InvalidIndex() ) {};
		tuple_t( const EHANDLE &handle ) : m_hTargeted(handle), 
			m_flAccumulatedDamage(0), m_nProjectiles( ProjectilePool_t::InvalidIndex() ) {};
	};

	/// an index into the targets contained herein. 
	// typedeffed just in case I need to change it from being a pointer.
	typedef tuple_t* IndexType_t;

		// functions
	// get the element for a given IndexType_t
	inline tuple_t &operator[]( const IndexType_t &I ) { return Elem(I); }
	inline tuple_t &Elem( const IndexType_t &I );
	inline bool IsValid( const IndexType_t &I ) const ;
	static inline IndexType_t InvalidIndex();

	// if the object hasn't got anything allocated, returns NULL
	IndexType_t Find( const EHANDLE &handle ) ; 

	// add this object to the damage allocator, with nothing allocated
	// to it yet. returns the allocated tuple_t. returns the existing one
	// if the object is already in the manager.
	IndexType_t Insert( CBaseEntity* pTarget );

	void Remove( const EHANDLE &handle );

	// allocate a projectile to a target. returns total damage being done to target so far.
	inline float Allocate(  CBaseEntity *pTarget, CBaseEntity *pProjectile, float flDamage );
	float Allocate( IndexType_t iTarget, CBaseEntity *pProjectile, float flDamage );

	// remove a projectile from a target. returns total damage being done to target so far.
	inline float Deallocate(  CBaseEntity *pTarget, CBaseEntity *pProjectile );
	float Deallocate( IndexType_t iTarget, CBaseEntity *pProjectile );

	// is the given projectile allocated to the given target?
	inline bool IsProjectileForTarget( IndexType_t iTarget, CBaseEntity *pProjectile );


	// constructor
	CASW_DamageAllocationMgr( int baseSize = 0 ) : m_Targets( 0, baseSize ), m_ProjectileLists( 0, baseSize ) {};

protected:

	// look through the projectile pool for a given target and return the index
	// of the ProjectilePool_t corresponding to the given projectile. Return 
	// ProjectilePool_t::InvalidIndex if not found.
	ProjectilePool_t::IndexType_t GetProjectileIndexInTarget( IndexType_t iTarget, CBaseEntity *pProjectile );

	// rebuild consistency
	void Rebuild( IndexType_t iTarget );

	CUtlVectorConservative< tuple_t > m_Targets;
	ProjectilePool_t m_ProjectileLists;
};

inline CASW_DamageAllocationMgr::tuple_t & CASW_DamageAllocationMgr::Elem( const IndexType_t &I )
{
	return *I;
}


inline bool CASW_DamageAllocationMgr::IsProjectileForTarget( IndexType_t iTarget, CBaseEntity *pProjectile )
{
	return GetProjectileIndexInTarget( iTarget, pProjectile ) != ProjectilePool_t::InvalidIndex();
}

inline float CASW_DamageAllocationMgr::Allocate(  CBaseEntity *pTarget, CBaseEntity *pProjectile, float flDamage )
{
	IndexType_t i = Find(pTarget);
	if ( !IsValid(i) )
	{
		AssertMsg2( false, "Tried to allocate %s as a damage projectile to %s but it isn't managed\n",
			pProjectile->GetDebugName(), pTarget->GetDebugName() );
		i = Insert( pTarget );
	}
	return Allocate( i, pProjectile, flDamage );
}

inline float CASW_DamageAllocationMgr::Deallocate(  CBaseEntity *pTarget, CBaseEntity *pProjectile )
{
	IndexType_t i = Find(pTarget);
	if ( !IsValid(i) )
	{
		AssertMsg2( false, "Tried to remove %s as a damage projectile to unmanaged %s\n",
			pProjectile->GetDebugName(), pTarget->GetDebugName() );
		return 0;
	}
	else
	{
		return Deallocate( i, pProjectile );
	}
}

inline CASW_DamageAllocationMgr::IndexType_t CASW_DamageAllocationMgr::InvalidIndex()
{
	return NULL;
}

inline bool CASW_DamageAllocationMgr::IsValid( const IndexType_t &I ) const
{
	return I != InvalidIndex();
}

class CASW_Rocket : public CBaseCombatCharacter
{
	DECLARE_CLASS( CASW_Rocket, CBaseCombatCharacter );
	DECLARE_SERVERCLASS();
public:
	static const int EXPLOSION_RADIUS = 200;

	CASW_Rocket();
	virtual ~CASW_Rocket();

	Class_T Classify( void ) { return CLASS_MISSILE; }
	
	virtual void Spawn( void );
	virtual void Activate( void );
	virtual void StopLoopingSounds( void );
	virtual void Precache( void );

	void	MissileTouch( CBaseEntity *pOther );
	void	Explode( void );
	void	AccelerateThink( void );
	void	IgniteThink( void );
	void	SeekThink( void );
	void	DumbFire( void );
	void	SetGracePeriod( float flGracePeriod );

	int		OnTakeDamage_Alive( const CTakeDamageInfo &info );
	void	Event_Killed( const CTakeDamageInfo &info );
	
	virtual float	GetDamage() { return m_flDamage; }
	virtual void	SetDamage(float flDamage) { m_flDamage = flDamage; }

	unsigned int PhysicsSolidMaskForEntity( void ) const;	

	static CASW_Rocket *Create( float fDamage, const Vector &vecOrigin, const QAngle &vecAngles, CBaseCombatCharacter *pentOwner = NULL , CBaseEntity * pCreatorWeapon = NULL, const char *className = "asw_rocket" );

	void CreateDangerSounds( bool bState ){ m_bCreateDangerSounds = bState; }

	void FindHomingPosition( Vector *pTarget );
	CBaseEntity *FindPotentialTarget( void ) const ; /// find a good enemy for us to target. The enemy isn't actually targeted until the result is written back to m_hHomingTarget
	void SetTarget( CBaseEntity *pTarget );
	inline CBaseEntity *GetTarget( void )  { return  m_hHomingTarget.Get(); }

	virtual void DrawDebugGeometryOverlays();

	int		m_iAttributeEffects;

	EHANDLE m_hCreatorWeapon;
	Class_T m_CreatorWeaponClass;

protected:
	virtual void DoExplosion(bool bHitwall);
	Vector		 IntegrateRocketThrust( const Vector &vTargetDir, float flDist ) const ;
	void		 ComputeWallDodge( const Vector &vCurVel ); /// if we're headed for a wall, adjust the wobble angles so we steer away

	float		 GetLifeFraction() const;

	bool IsWallDodging() const;

	EHANDLE m_hHomingTarget;

	float m_flDamage;
	float m_fSpawnTime;
	float m_fDieTime;

	const char *			m_szFlightSound;
	const char *			m_szDetonationSound;

	static CASW_DamageAllocationMgr m_RocketAssigner;

private:
	float					m_flGracePeriodEndsAt;
	float					m_flNextWobbleTime;

	QAngle					m_vWobbleAngles; ///< will approach this direction while wobbling

	bool					m_bCreateDangerSounds;
	bool					m_bFlyingWild; // has no target, is wobbling crazily

	DECLARE_DATADESC();
};

#endif // _INCLUDED_ASW_ROCKET_H