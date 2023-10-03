#include "cbase.h"
#include "world.h"
#include "asw_barrel_explosive.h"
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_gamerules.h"
#include "particle_parse.h"
#include "asw_util_shared.h"
#include "cvisibilitymonitor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_EXPLOSIVE_BARREL_MODEL_NAME "models/swarm/Barrel/barrel.mdl"


ConVar asw_barrel_health_base( "asw_barrel_health_base", "3", FCVAR_CHEAT, "Health of barrel at level 1" );
ConVar asw_barrel_health_growth( "asw_barrel_health_growth", "0.15", FCVAR_CHEAT, "% change in health per level" );


LINK_ENTITY_TO_CLASS( asw_barrel_explosive, CASW_Barrel_Explosive );

BEGIN_DATADESC( CASW_Barrel_Explosive )	
	
END_DATADESC()


CASW_Barrel_Explosive::CASW_Barrel_Explosive()
{
	m_iExplosionDamage = 200;
}

void CASW_Barrel_Explosive::Spawn()
{
	DisableAutoFade();
	SetModelName( AllocPooledString( ASW_EXPLOSIVE_BARREL_MODEL_NAME ) );
	Precache();
	SetModel( ASW_EXPLOSIVE_BARREL_MODEL_NAME );

	AddSpawnFlags( SF_PHYSPROP_AIMTARGET );
	BaseClass::Spawn();

	InitHealth();

	VisibilityMonitor_AddEntity( this, asw_visrange_generic.GetFloat() * 0.9f, NULL, NULL );
}

void CASW_Barrel_Explosive::Precache()
{
	PrecacheParticleSystem( "explosion_barrel" );
	PrecacheScriptSound( "ASWBarrel.Explode" );
	PrecacheModel( ASW_EXPLOSIVE_BARREL_MODEL_NAME );

	BaseClass::Precache();
}

int CASW_Barrel_Explosive::OnTakeDamage( const CTakeDamageInfo &info )
{
	int saveFlags = m_takedamage;

	// don't be destroyed by buzzers
	if ( info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_BUZZER )
	{
		return 0;
	}

	// prevent barrel exploding when knocked around
	if ( info.GetDamageType() & DMG_CRUSH )
		return 0;
	
	CASW_Marine* pMarine = NULL;
	if ( info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE )
	{
		m_hAttacker = info.GetAttacker();

		pMarine = assert_cast< CASW_Marine* >( info.GetAttacker() );
		// prevent AI marines blowing up barrels as it makes the player ANGRY ANGRY
		if ( pMarine && !pMarine->IsInhabited() )
			return 0;
	}

	// don't burst open if melee'd
	if ( info.GetDamageType() & ( DMG_CLUB | DMG_SLASH ) )
	{
		m_takedamage = DAMAGE_EVENTS_ONLY;

		if( !m_bMeleeHit )
		{
			if ( pMarine )
			{
				IGameEvent * event = gameeventmanager->CreateEvent( "physics_melee" );
				if ( event )
				{
					CASW_Player *pCommander = pMarine->GetCommander();
					event->SetInt( "attacker", pCommander ? pCommander->GetUserID() : 0 );
					event->SetInt( "entindex", entindex() );
					gameeventmanager->FireEvent( event );
				}
			}
			m_bMeleeHit = true;
		}
	}

	if ( pMarine )
	{
		pMarine->HurtJunkItem(this, info);
	}

	// skip the breakable prop's complex damage handling and just hurt us
	int iResult = CBaseEntity::OnTakeDamage(info);
	m_takedamage = saveFlags;

	return iResult;
}

void CASW_Barrel_Explosive::Event_Killed( const CTakeDamageInfo &info )
{
	IPhysicsObject *pPhysics = VPhysicsGetObject();
	if ( pPhysics && !pPhysics->IsMoveable() )
	{
		pPhysics->EnableMotion( true );
		VPhysicsTakeDamage( info );
	}

	QueueForExplode( info );

	// Break( info.GetInflictor(), info );
	// DoExplosion();
}

void CASW_Barrel_Explosive::ExplodeNow( const CTakeDamageInfo &info )
{
	Break( info.GetInflictor(), info );
	DoExplosion();
}

void CASW_Barrel_Explosive::DoExplosion()
{
	// scorch the ground
	trace_t		tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, -80 ), MASK_SHOT, 
		this, COLLISION_GROUP_NONE, &tr);

	if ((tr.m_pEnt != GetWorldEntity()) || (tr.hitbox != 0))
	{
		// non-world needs smaller decals
		if( tr.m_pEnt && !tr.m_pEnt->IsNPC() )
		{
			UTIL_DecalTrace( &tr, "SmallScorch" );
		}
	}
	else
	{
		UTIL_DecalTrace( &tr, "Scorch" );
	}

	UTIL_ASW_ScreenShake( GetAbsOrigin(), 25.0, 150.0, 1.0, 750, SHAKE_START );

	// explosion effects
	Vector vecExplosionPos = GetAbsOrigin();
	CPASFilter filter( vecExplosionPos );
	UserMessageBegin( filter, "ASWBarrelExplosion" );
	WRITE_FLOAT( vecExplosionPos.x );
	WRITE_FLOAT( vecExplosionPos.y );
	WRITE_FLOAT( vecExplosionPos.z );
	WRITE_FLOAT( 160.0f );
	MessageEnd();

	EmitSound( "ASWBarrel.Explode" );

	// damage to nearby things
	CTakeDamageInfo info( this, ( m_hAttacker.Get() ? m_hAttacker.Get() : this ), m_iExplosionDamage, DMG_BLAST );
	info.SetDamageCustom( DAMAGE_FLAG_HALF_FALLOFF );
	ASWGameRules()->RadiusDamage( info, GetAbsOrigin(), 160.0f, CLASS_NONE, NULL );
}

void CASW_Barrel_Explosive::OnDifficultyChanged( int iDifficulty )
{
	InitHealth();
}

void CASW_Barrel_Explosive::InitHealth()
{
	m_iMinHealthDmg = 0;
	m_iHealth = asw_barrel_health_base.GetInt();
	const float flHealthGrowth = asw_barrel_health_growth.GetFloat();
	m_iHealth = m_iHealth + m_iHealth * flHealthGrowth * ( ASWGameRules()->GetMissionDifficulty() - 1 );
	SetMaxHealth( m_iHealth );

	m_iExplosionDamage = 200.0f;
	/*
	int iExplosiveDamage = 120.0f;
	m_iExplosionDamage = iExplosiveDamage + iExplosiveDamage * flHealthGrowth * ( ASWGameRules()->GetMissionDifficulty() - 1 );
	//SetExplosiveRadius( 160.0f );
	*/
}


// the queue of explodable objects
class CDeferredExplosionQueue : public CAutoGameSystemPerFrame
{
public:
	CDeferredExplosionQueue() : CAutoGameSystemPerFrame("CDeferredExplosionQueue"), m_fLastExplosionTime(0) {};

	/// enqueue an object for explosion. may call ExplodeNow() immediately
	/// if nothing has or is slated to detonate this frame.
	void Enqueue( CASW_Exploding_Prop *pBoom, const CTakeDamageInfo &damageInfo, bool bImmediateExplosionAllowed = true );

	// game system interface
	virtual void FrameUpdatePreEntityThink( );
	virtual void LevelInitPreEntity();
	virtual void LevelShutdownPostEntity();

	float m_fLastExplosionTime; //< the gpglobals->tickcount when the last explosion happened
protected:
	// tell an enqued object to detonate. 
	inline void Detonate( CASW_Exploding_Prop *pBoom, const CTakeDamageInfo &damageInfo );

	class QueueCell_t
	{
	public:
		QueueCell_t(){};
		QueueCell_t( const CHandle<CASW_Exploding_Prop> &handle, const CTakeDamageInfo &damageInfo ) : m_hHandle(handle), m_damageInfo(damageInfo)
		{};

		CHandle<CASW_Exploding_Prop> m_hHandle;
		CTakeDamageInfo m_damageInfo;
	};

	CUtlQueue< QueueCell_t > m_Explodables;
};
CDeferredExplosionQueue g_ASWDeferredExplosionQueue;


void CDeferredExplosionQueue::Enqueue( CASW_Exploding_Prop *pBoom, const CTakeDamageInfo &damageInfo, bool bImmediateExplosionAllowed )
{
	// can it detonate immediately?
	if ( bImmediateExplosionAllowed && 
		 m_fLastExplosionTime <= gpGlobals->curtime - ASW_EXPLODING_PROP_INTERVAL && 
		 m_Explodables.Count() == 0 )
	{
		Detonate( pBoom, damageInfo );
	}
	else
	{
		// must be queued for later detonation. (unless it's already queued)
		const int count = m_Explodables.Count();
		for ( int i = 0 ; i < count ; ++i )
		{
			if ( m_Explodables[i].m_hHandle == pBoom )
				return; // bail out
		}
		m_Explodables.Insert( QueueCell_t(pBoom, damageInfo) );
	}
}

inline void CDeferredExplosionQueue::Detonate( CASW_Exploding_Prop *pBoom, const CTakeDamageInfo &damageInfo )
{
	m_fLastExplosionTime = gpGlobals->curtime;
	pBoom->ExplodeNow( damageInfo );
}

void CDeferredExplosionQueue::FrameUpdatePreEntityThink( )
{
	// if nothing has exploded this frame and we've got something in the queue, blow it up
	if ( m_fLastExplosionTime <= gpGlobals->curtime - ASW_EXPLODING_PROP_INTERVAL && 
		 m_Explodables.Count() > 0 )
	{
		CASW_Exploding_Prop *pBoom = m_Explodables.Head().m_hHandle.Get();
		if ( pBoom ) 
		{
			Detonate( pBoom, m_Explodables.Head().m_damageInfo );
		}
		m_Explodables.RemoveAtHead();
	}
}

void CDeferredExplosionQueue::LevelInitPreEntity()
{
	m_fLastExplosionTime = 0;
}

void CDeferredExplosionQueue::LevelShutdownPostEntity()
{
	// empty out everything in the queue
	m_Explodables.Purge();
}

void CASW_Exploding_Prop::QueueForExplode( const CTakeDamageInfo &damageInfo )
{
	g_ASWDeferredExplosionQueue.Enqueue( this, damageInfo );
}
