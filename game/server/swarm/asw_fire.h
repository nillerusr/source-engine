//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FIRE_H
#define FIRE_H
#ifdef _WIN32
#pragma once
#endif

#include "entityoutput.h"
#include "fire_smoke.h"
#include "plasma.h"
#include "asw_shareddefs.h"

#ifdef INFESTED_DLL
// asw
//#include "asw_marine.h"
//#include "asw_marine_resource.h"
//#include "asw_alien.h"
//#include "asw_extinguisher_projectile.h"
//#include "asw_generic_emitter_entity.h"
#include "asw_dynamic_light.h"
//#include "asw_flamer_projectile.h"
//#include "iasw_spawnable_npc.h"
//#include "asw_shareddefs.h"
#endif

//Spawnflags
#define	SF_FIRE_INFINITE			0x00000001
#define	SF_FIRE_SMOKELESS			0x00000002
#define	SF_FIRE_START_ON			0x00000004
#define	SF_FIRE_START_FULL			0x00000008
#define SF_FIRE_DONT_DROP			0x00000010
#define	SF_FIRE_NO_GLOW				0x00000020
#define SF_FIRE_DIE_PERMANENT		0x00000080
//asw
#define SF_FIRE_NO_SOUND			0x00000100
#define SF_FIRE_NO_IGNITE_SOUND		0x00000200
#define SF_FIRE_NO_FUELLING_ONCE_LIT	0x00000400
#define SF_FIRE_FAST_BURN_THINK		0x00000800

//==================================================
// CFire
//==================================================

enum fireType_e
{
	FIRE_NATURAL = 0,
	FIRE_WALL_MINE,
	FIRE_PLASMA,
};

//==================================================

// NPCs and grates do not prevent fire from travelling
#define	MASK_FIRE_SOLID	 ( MASK_SOLID & (~(CONTENTS_MONSTER|CONTENTS_GRATE)) )

class CFire : public CBaseEntity
{
public:
	DECLARE_CLASS( CFire, CBaseEntity );
	DECLARE_SERVERCLASS();

	int DrawDebugTextOverlays(void);

	CFire( void );

	virtual void UpdateOnRemove( void );

	void	Precache( void );
	void	Init( const Vector &position, float scale, float attackTime, float fuel, int flags, int fireType );
	bool	GoOut();

	void	BurnThink();
#ifdef INFESTED_DLL
	void	FastBurnThink();
	void ASWFireTouch(CBaseEntity* pOther);	// asw
#endif
	void	GoOutThink();
	void	GoOutInSeconds( float seconds );

	void	SetOwner( CBaseEntity *hOwner ) { m_hOwner = hOwner; }
	CBaseEntity* GetOwner( void ) { return m_hOwner; }

	void	Scale( float end, float time );
	void	AddHeat( float heat, bool selfHeat = false );
	int		OnTakeDamage( const CTakeDamageInfo &info );

	bool	IsBurning( void ) const;

	bool	GetFireDimensions( Vector *pFireMins, Vector *pFireMaxs );

	void	Extinguish( float heat );
	void	DestroyEffect();

	virtual	void Update( float simTime );

	void	Spawn( void );
	void	Activate( void );
	void	StartFire( void );
	void	Start();
	void	SetToOutSize()
	{
		UTIL_SetSize( this, Vector(-8,-8,0), Vector(8,8,8) );
	}

	float	GetHeatLevel()	{ return m_flHeatLevel; }
#ifdef INFESTED_DLL
	void SetHeatLevel(float fHeat) { m_flHeatLevel = fHeat; }
#endif

	virtual int UpdateTransmitState();

	void DrawDebugGeometryOverlays(void) 
	{
		if (m_debugOverlays & OVERLAY_BBOX_BIT) 
		{	
			if ( m_lastDamage > gpGlobals->curtime && m_flHeatAbsorb > 0 )
			{
				NDebugOverlay::EntityBounds(this, 88, 255, 128, 0 ,0);
				char tempstr[512];
				Q_snprintf( tempstr, sizeof(tempstr), "Heat: %.1f", m_flHeatAbsorb );
				EntityText(1,tempstr, 0);
			}
			else if ( !IsBurning() )
			{
				NDebugOverlay::EntityBounds(this, 88, 88, 128, 0 ,0);
			}

			if ( IsBurning() )
			{
				Vector mins, maxs;
				if ( GetFireDimensions( &mins, &maxs ) )
				{
					NDebugOverlay::Box(GetAbsOrigin(), mins, maxs, 128, 0, 0, 10, 0);
				}
			}


		}
		BaseClass::DrawDebugGeometryOverlays();
	}

	void Disable();
	EHANDLE m_hCreatorWeapon;

	//Inputs
	void	InputStartFire( inputdata_t &inputdata );
	void	InputExtinguish( inputdata_t &inputdata );
	void	InputExtinguishTemporary( inputdata_t &inputdata );
	void	InputEnable( inputdata_t &inputdata );
	void	InputDisable( inputdata_t &inputdata );

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_FIRE; }
protected:

	void	Spread( void );
	void	SpawnEffect( fireType_e type, float scale );

	CHandle<CBaseFire>	m_hEffect;
#ifdef INFESTED_DLL
	//CHandle<CASW_Emitter>  m_hEmitter;
	CHandle<CASW_Dynamic_Light> m_hFireLight;
	bool m_bFirefighterCounted;	// has this fire been counted towards the firefighter stats yet?  (so we don't count more than once)
#endif
	EHANDLE		m_hOwner;

	CNetworkVar( int, m_nFireType );

	float	m_flFuel;
	float	m_flDamageTime;
	float	m_lastDamage;
	CNetworkVar( float, m_flFireSize ); // size of the fire in world units
	CNetworkVar( float, m_flHeatLevel ); // Used as a "health" for the fire.  > 0 means the fire is burning
	float	m_flHeatAbsorb;	// This much heat must be "absorbed" before it gets transferred to the flame size
	float	m_flDamageScale;

	CNetworkVar( float, m_flMaxHeat );
	float	m_flLastHeatLevel;

	//NOTENOTE: Lifetime is an expression of the sum total of these amounts plus the global time when started
	float	m_flAttackTime;	//Amount of time to scale up

	CNetworkVar( bool,	m_bEnabled );
	bool	m_bStartDisabled;
	bool	m_bDidActivate;

#ifdef INFESTED_DLL
	// asw
	CSoundPatch *m_pLoopSound;
	string_t m_LoopSoundName;
	string_t m_StartSoundName;
	color32 m_LightColor;
	float m_fLightRadiusScale;
	int m_iLightBrightness;
	bool m_bNoFuelingOnceLit;
#endif

	COutputEvent	m_OnIgnited;
	COutputEvent	m_OnExtinguished;

	DECLARE_DATADESC();
};

class CFireSphere : public IPartitionEnumerator
{
public:
	CFireSphere( CFire **pList, int listMax, bool onlyActiveFires, const Vector &origin, float radius );
	// This gets called	by the enumeration methods with each element
	// that passes the test.
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );

	int GetCount() { return m_count; }
	bool AddToList( CFire *pEntity );

private:
	Vector			m_origin;
	float			m_radiusSqr;
	CFire			**m_pList;
	int				m_listMax;
	int				m_count;
	bool			m_onlyActiveFires;
};

//==================================================
// FireSystem
//==================================================
bool FireSystem_StartFire( const Vector &position, float fireHeight, float attack, float fuel, int flags, CBaseEntity *owner, fireType_e type = FIRE_NATURAL, float flRotation = 0, CBaseEntity *pCreatorWeapon = NULL );
void FireSystem_ExtinguishInRadius( const Vector &origin, float radius, float rate );
void FireSystem_AddHeatInRadius( const Vector &origin, float radius, float heat );

bool FireSystem_GetFireDamageDimensions( CBaseEntity *pFire, Vector *pFireMins, Vector *pFireMaxs );

#ifdef INFESTED_DLL
bool FireSystem_StartDormantFire( const Vector &position, float fireHeight, float attack, float fuel, int flags, CBaseEntity *owner, fireType_e type = FIRE_NATURAL);
bool FireSystem_ExtinguishFire(CBaseEntity *pEnt);
#endif
#endif // FIRE_H
