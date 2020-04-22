//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "Sprite.h"
#include "EnvLaser.h"
#include "basecombatweapon.h"
#include "explode.h"
#include "eventqueue.h"
#include "gamerules.h"
#include "ammodef.h"
#include "in_buttons.h"
#include "soundent.h"
#include "ndebugoverlay.h"
#include "grenade_beam.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "triggers.h"
#include "physics_cannister.h"

#include "player.h"
#include "entitylist.h"


#define SF_TANK_ACTIVE			0x0001
#define SF_TANK_PLAYER			0x0002
#define SF_TANK_HUMANS			0x0004
#define SF_TANK_ALIENS			0x0008
#define SF_TANK_LINEOFSIGHT		0x0010
#define SF_TANK_CANCONTROL		0x0020
#define	SF_TANK_DAMAGE_KICK		0x0040	// Kick when take damage
#define	SF_TANK_AIM_AT_POS		0x0080	// Aim at a particular position
#define SF_TANK_SOUNDON			0x8000


enum TANKBULLET
{
	TANK_BULLET_NONE	= 0,
	TANK_BULLET_SMALL	= 1,
	TANK_BULLET_MEDIUM	= 2,
	TANK_BULLET_LARGE	= 3,
};

#define FUNCTANK_FIREVOLUME	1000


//			Custom damage
//			env_laser (duration is 0.5 rate of fire)
//			rockets
//			explosion?

class CFuncTank : public CBaseEntity
{
	DECLARE_CLASS( CFuncTank, CBaseEntity );
public:
	~CFuncTank( void );
	void	Spawn( void );
	void	Activate( void );
	void	Precache( void );
	bool	CreateVPhysics( void );
	bool	KeyValue( const char *szKeyName, const char *szValue );

	int		ObjectCaps( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	Think( void );
	void	TrackTarget( void );
	int		DrawDebugTextOverlays(void);

	virtual void FiringSequence( const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker );
	virtual void Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker );

	void	StartRotSound( void );
	void	StopRotSound( void );

	inline bool IsActive( void ) { return (m_spawnflags & SF_TANK_ACTIVE)?TRUE:FALSE; }

	// Input handlers.
	void InputActivate( inputdata_t &inputdata );
	void InputDeactivate( inputdata_t &inputdata );
	void InputSetFireRate( inputdata_t &inputdata );
	void InputSetTargetPosition( inputdata_t &inputdata );
	void InputSetTargetEntityName( inputdata_t &inputdata );
	void InputSetTargetEntity( inputdata_t &inputdata );

	void TankActivate(void);
	void TankDeactivate(void);

	inline bool CanFire( void );
	bool		InRange( float range );

	void		TankTrace( const Vector &vecStart, const Vector &vecForward, const Vector &vecSpread, trace_t &tr );
	void		TraceAttack( CBaseEntity *pAttacker, float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType);

	Vector		WorldBarrelPosition( void )
	{
		EntityMatrix tmp;
		tmp.InitFromEntity( this );
		return tmp.LocalToWorld( m_barrelPos );
	}

	void		UpdateMatrix( void )
	{
		m_parentMatrix.InitFromEntity( GetParent() ? GetParent() : NULL );
	}
	QAngle		AimBarrelAt( const Vector &parentTarget );

	bool	ShouldSavePhysics()	{ return false; }
	
	DECLARE_DATADESC();

	bool OnControls( CBaseEntity *pTest );
	bool StartControl( CBasePlayer *pController );
	void StopControl( void );
	void ControllerPostFrame( void );

	CBaseEntity *FindTarget( string_t targetName, CBaseEntity *pActivator );

protected:
	CBasePlayer*	m_pController;
	float			m_flNextAttack;
	Vector			m_vecControllerUsePos;
	
	float			m_yawCenter;	// "Center" yaw
	float			m_yawRate;		// Max turn rate to track targets
	float			m_yawRange;		// Range of turning motion (one-sided: 30 is +/- 30 degress from center)
									// Zero is full rotation
	float			m_yawTolerance;	// Tolerance angle

	float			m_pitchCenter;	// "Center" pitch
	float			m_pitchRate;	// Max turn rate on pitch
	float			m_pitchRange;	// Range of pitch motion as above
	float			m_pitchTolerance;	// Tolerance angle

	float			m_fireLast;		// Last time I fired
	float			m_fireRate;		// How many rounds/second
	float			m_lastSightTime;// Last time I saw target
	float			m_persist;		// Persistence of firing (how long do I shoot when I can't see)
	float			m_persist2;		// Secondary persistence of firing (randomly shooting when I can't see)
	float			m_persist2burst;// How long secondary persistence burst lasts
	float			m_minRange;		// Minimum range to aim/track
	float			m_maxRange;		// Max range to aim/track

	Vector			m_barrelPos;	// Length of the freakin barrel
	float			m_spriteScale;	// Scale of any sprites we shoot
	string_t		m_iszSpriteSmoke;
	string_t		m_iszSpriteFlash;
	TANKBULLET		m_bulletType;	// Bullet type
	int				m_iBulletDamage; // 0 means use Bullet type's default damage
	
	Vector			m_sightOrigin;	// Last sight of target
	int				m_spread;		// firing spread
	string_t		m_iszMaster;	// Master entity (game_team_master or multisource)

	int				m_iSmallAmmoType;
	int				m_iMediumAmmoType;
	int				m_iLargeAmmoType;

	string_t		m_soundStartRotate;
	string_t		m_soundStopRotate;
	string_t		m_soundLoopRotate;

	string_t		m_targetEntityName;
	EHANDLE			m_hTarget;
	Vector			m_vTargetPosition;
	EntityMatrix	m_parentMatrix;

	COutputEvent	m_OnFire;
	COutputEvent	m_OnLoseTarget;
	COutputEvent	m_OnAquireTarget;

	CHandle<CBaseTrigger>	m_hControlVolume;
	string_t				m_iszControlVolume;
};


BEGIN_DATADESC( CFuncTank )

	DEFINE_KEYFIELD( m_yawRate, FIELD_FLOAT, "yawrate" ),
	DEFINE_KEYFIELD( m_yawRange, FIELD_FLOAT, "yawrange" ),
	DEFINE_KEYFIELD( m_yawTolerance, FIELD_FLOAT, "yawtolerance" ),
	DEFINE_KEYFIELD( m_pitchRate, FIELD_FLOAT, "pitchrate" ),
	DEFINE_KEYFIELD( m_pitchRange, FIELD_FLOAT, "pitchrange" ),
	DEFINE_KEYFIELD( m_pitchTolerance, FIELD_FLOAT, "pitchtolerance" ),
	DEFINE_KEYFIELD( m_fireRate, FIELD_FLOAT, "firerate" ),
	DEFINE_KEYFIELD( m_persist, FIELD_FLOAT, "persistence" ),
	DEFINE_KEYFIELD( m_persist2, FIELD_FLOAT, "persistence2" ),
	DEFINE_KEYFIELD( m_minRange, FIELD_FLOAT, "minRange" ),
	DEFINE_KEYFIELD( m_maxRange, FIELD_FLOAT, "maxRange" ),
	DEFINE_KEYFIELD( m_spriteScale, FIELD_FLOAT, "spritescale" ),
	DEFINE_KEYFIELD( m_iszSpriteSmoke, FIELD_STRING, "spritesmoke" ),
	DEFINE_KEYFIELD( m_iszSpriteFlash, FIELD_STRING, "spriteflash" ),
	DEFINE_KEYFIELD( m_bulletType, FIELD_INTEGER, "bullet" ),
	DEFINE_KEYFIELD( m_spread, FIELD_INTEGER, "firespread" ),
	DEFINE_KEYFIELD( m_iBulletDamage, FIELD_INTEGER, "bullet_damage" ),
	DEFINE_KEYFIELD( m_iszMaster, FIELD_STRING, "master" ),
	DEFINE_KEYFIELD( m_soundStartRotate, FIELD_SOUNDNAME, "rotatestartsound" ),
	DEFINE_KEYFIELD( m_soundStopRotate, FIELD_SOUNDNAME, "rotatestopsound" ),
	DEFINE_KEYFIELD( m_soundLoopRotate, FIELD_SOUNDNAME, "rotatesound" ),
	DEFINE_KEYFIELD( m_iszControlVolume, FIELD_STRING, "control_volume" ),
	
	DEFINE_FIELD( m_yawCenter, FIELD_FLOAT ),
	DEFINE_FIELD( m_pitchCenter, FIELD_FLOAT ),
	DEFINE_FIELD( m_fireLast, FIELD_TIME ),
	DEFINE_FIELD( m_lastSightTime, FIELD_TIME ),
	DEFINE_FIELD( m_barrelPos, FIELD_VECTOR ),
	DEFINE_FIELD( m_sightOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_pController, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_vecControllerUsePos, FIELD_VECTOR ),
	DEFINE_FIELD( m_flNextAttack, FIELD_TIME ),
	DEFINE_FIELD( m_targetEntityName, FIELD_STRING ),
	DEFINE_FIELD( m_vTargetPosition, FIELD_VECTOR ),
	DEFINE_FIELD( m_persist2burst, FIELD_FLOAT),
	//DEFINE_FIELD( m_parentMatrix, FIELD_MATRIX ), // DON'T SAVE
	DEFINE_FIELD( m_hTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hControlVolume, FIELD_EHANDLE ),


	// Do not save these. 
	//DEFINE_FIELD( m_iSmallAmmoType, FIELD_INTEGER ),
	//DEFINE_FIELD( m_iMediumAmmoType, FIELD_INTEGER ),
	//DEFINE_FIELD( m_iLargeAmmoType, FIELD_INTEGER ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Activate", InputActivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Deactivate", InputDeactivate ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetFireRate", InputSetFireRate ),
	DEFINE_INPUTFUNC( FIELD_VECTOR, "SetTargetPosition", InputSetTargetPosition ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetTargetEntityName", InputSetTargetEntityName ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "SetTargetEntity", InputSetTargetEntity ),

	// Outputs
	DEFINE_OUTPUT(m_OnFire,			"OnFire"),
	DEFINE_OUTPUT(m_OnLoseTarget,	"OnLoseTarget"),
	DEFINE_OUTPUT(m_OnAquireTarget,	"OnAquireTarget"),

END_DATADESC()



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFuncTank::~CFuncTank( void )
{
	if ( m_soundLoopRotate != NULL_STRING && (m_spawnflags & SF_TANK_SOUNDON) )
	{
		StopSound( entindex(), CHAN_STATIC, STRING(m_soundLoopRotate) );
	}
}


int	CFuncTank::ObjectCaps( void ) 
{ 
	int iCaps = BaseClass::ObjectCaps();

	if ( m_spawnflags & SF_TANK_CANCONTROL )
	{
		iCaps |= FCAP_IMPULSE_USE;
	}

	return iCaps;
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
inline bool CFuncTank::CanFire( void )
{ 
	float flTimeDelay = gpGlobals->curtime - m_lastSightTime;
	// Fire when can't see enemy if time is less that persistence time
	if ( flTimeDelay <= m_persist)
	{
		return true;
	}
	// Fire when I'm in a persistence2 burst
	else if (flTimeDelay <= m_persist2burst)
	{
		return true;
	}
	// If less than persistence2, occasionally do another burst
	else if (flTimeDelay <= m_persist2)
	{
		if (random->RandomInt(0,30)==0)
		{
			m_persist2burst = flTimeDelay+0.5;
			return true;
		}
	}
	return false;
}


//------------------------------------------------------------------------------
// Purpose: Input handler for activating the tank.
//------------------------------------------------------------------------------
void CFuncTank::InputActivate( inputdata_t &inputdata )
{	
	TankActivate();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTank::TankActivate(void)
{
	m_spawnflags	|= SF_TANK_ACTIVE; 
	SetNextThink( gpGlobals->curtime + 0.1f ); 
	m_fireLast		= 0;
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for deactivating the tank.
//-----------------------------------------------------------------------------
void CFuncTank::InputDeactivate( inputdata_t &inputdata )
{
	TankDeactivate();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTank::TankDeactivate(void)
{
	m_spawnflags	&= ~SF_TANK_ACTIVE; 
	m_fireLast		= 0; 
	StopRotSound();
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for changing the name of the tank's target entity.
//-----------------------------------------------------------------------------
void CFuncTank::InputSetTargetEntityName( inputdata_t &inputdata )
{
	m_targetEntityName = inputdata.value.StringID();
	m_hTarget = FindTarget( m_targetEntityName, inputdata.pActivator );

	// No longer aim at target position if have one
	m_spawnflags		&= ~SF_TANK_AIM_AT_POS; 
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for setting a new target entity by ehandle.
//-----------------------------------------------------------------------------
void CFuncTank::InputSetTargetEntity( inputdata_t &inputdata )
{
	if (inputdata.value.Entity() != NULL)
	{
		m_targetEntityName = inputdata.value.Entity()->GetEntityName();
	}
	else
	{
		m_targetEntityName = NULL_STRING;
	}
	m_hTarget = inputdata.value.Entity();

	// No longer aim at target position if have one
	m_spawnflags		&= ~SF_TANK_AIM_AT_POS; 
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for setting the rate of fire in shots per second.
//-----------------------------------------------------------------------------
void CFuncTank::InputSetFireRate( inputdata_t &inputdata )
{
	m_fireRate = inputdata.value.Float();
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for setting the target as a position.
//-----------------------------------------------------------------------------
void CFuncTank::InputSetTargetPosition( inputdata_t &inputdata )
{
	m_spawnflags		|= SF_TANK_AIM_AT_POS; 
	m_hTarget			= NULL;

	inputdata.value.Vector3D(m_vTargetPosition);
}


//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CFuncTank::DrawDebugTextOverlays(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		// --------------
		// State
		// --------------
		char tempstr[255];
		if (IsActive()) 
		{
			Q_strncpy(tempstr,"State: Active",sizeof(tempstr));
		}
		else 
		{
			Q_strncpy(tempstr,"State: Inactive",sizeof(tempstr));
		}
		EntityText(text_offset,tempstr,0);
		text_offset++;
		
		// -------------------
		// Print Firing Speed
		// --------------------
		Q_snprintf(tempstr,sizeof(tempstr),"Fire Rate: %f",m_fireRate);

		EntityText(text_offset,tempstr,0);
		text_offset++;
		
		// --------------
		// Print Target
		// --------------
		if (m_hTarget!=NULL) 
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Target: %s",m_hTarget->GetDebugName());
		}
		else
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Target:   -  ");
		}
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// --------------
		// Target Pos
		// --------------
		if (m_spawnflags & SF_TANK_AIM_AT_POS) 
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Aim Pos: %3.0f %3.0f %3.0f",m_vTargetPosition.x,m_vTargetPosition.y,m_vTargetPosition.z);
		}
		else
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Aim Pos:    -  ");
		}
		EntityText(text_offset,tempstr,0);
		text_offset++;

	}
	return text_offset;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pAttacker - 
//			flDamage - 
//			vecDir - 
//			ptr - 
//			bitsDamageType - 
//-----------------------------------------------------------------------------
void CFuncTank::TraceAttack( CBaseEntity *pAttacker, float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType)
{
	if (m_spawnflags & SF_TANK_DAMAGE_KICK)
	{
		// Deflect the func_tank
		// Only adjust yaw for now
		if (pAttacker)
		{
			Vector vFromAttacker = (pAttacker->EyePosition()-GetAbsOrigin());
			vFromAttacker.z = 0;
			VectorNormalize(vFromAttacker);

			Vector vFromAttacker2 = (ptr->endpos-GetAbsOrigin());
			vFromAttacker2.z = 0;
			VectorNormalize(vFromAttacker2);


			Vector vCrossProduct;
			CrossProduct(vFromAttacker,vFromAttacker2, vCrossProduct);
			Msg( "%f\n",vCrossProduct.z);
			QAngle angles;
			angles = GetLocalAngles();
			if (vCrossProduct.z > 0)
			{
				angles.y		+= 10;
			}
			else
			{
				angles.y		-= 10;
			}

			// Limit against range in y
			if ( angles.y > m_yawCenter + m_yawRange )
			{
				angles.y = m_yawCenter + m_yawRange;
			}
			else if ( angles.y < (m_yawCenter - m_yawRange) )
			{
				angles.y = (m_yawCenter - m_yawRange);
			}

			SetLocalAngles( angles );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : targetName - 
//			pActivator - 
//-----------------------------------------------------------------------------
CBaseEntity *CFuncTank::FindTarget( string_t targetName, CBaseEntity *pActivator ) 
{
	return gEntList.FindEntityGenericNearest( STRING( targetName ), GetAbsOrigin(), 0, this, pActivator );
}


//-----------------------------------------------------------------------------
// Purpose: Caches entity key values until spawn is called.
// Input  : szKeyName - 
//			szValue - 
// Output : 
//-----------------------------------------------------------------------------
bool CFuncTank::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "barrel"))
	{
		m_barrelPos.x = atof(szValue);
	}
	else if (FStrEq(szKeyName, "barrely"))
	{
		m_barrelPos.y = atof(szValue);
	}
	else if (FStrEq(szKeyName, "barrelz"))
	{
		m_barrelPos.z = atof(szValue);
	}
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}


static Vector gTankSpread[] =
{
	Vector( 0, 0, 0 ),		// perfect
	Vector( 0.025, 0.025, 0.025 ),	// small cone
	Vector( 0.05, 0.05, 0.05 ),  // medium cone
	Vector( 0.1, 0.1, 0.1 ),	// large cone
	Vector( 0.25, 0.25, 0.25 ),	// extra-large cone
};
#define MAX_FIRING_SPREADS ARRAYSIZE(gTankSpread)


void CFuncTank::Spawn( void )
{
	Precache();

	SetMoveType( MOVETYPE_PUSH );  // so it doesn't get pushed by anything
	SetSolid( SOLID_VPHYSICS );
	SetModel( STRING( GetModelName() ) );

	m_yawCenter			= GetLocalAngles().y;
	m_pitchCenter		= GetLocalAngles().x;
	m_vTargetPosition	= vec3_origin;

	if ( IsActive() )
		SetNextThink( gpGlobals->curtime + 1.0f );

	UpdateMatrix();

	m_sightOrigin = WorldBarrelPosition(); // Point at the end of the barrel

	if ( m_spread > MAX_FIRING_SPREADS )
	{
		m_spread = 0;
	}

	// No longer aim at target position if have one
	m_spawnflags		&= ~SF_TANK_AIM_AT_POS; 

	if (m_spawnflags & SF_TANK_DAMAGE_KICK)
	{
		m_takedamage = DAMAGE_YES;
	}

	// UNDONE: Do this?
	//m_targetEntityName = m_target;
	CreateVPhysics();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTank::Activate( void )
{
	BaseClass::Activate();

	m_hControlVolume = NULL;

	// Find our control volume
	if ( m_iszControlVolume != NULL_STRING )
	{
		m_hControlVolume = dynamic_cast<CBaseTrigger*>( gEntList.FindEntityByName( NULL, m_iszControlVolume ) );
	}

	if (( !m_hControlVolume ) && (m_spawnflags & SF_TANK_CANCONTROL))
	{
		Msg( "ERROR: Couldn't find control volume for player-controllable func_tank %s.\n", STRING(GetEntityName()) );
		UTIL_Remove( this );
	}
}

bool CFuncTank::CreateVPhysics()
{
	VPhysicsInitShadow( false, false );
	return true;
}


void CFuncTank::Precache( void )
{
	m_iSmallAmmoType	= GetAmmoDef()->Index("9mmRound");
	m_iMediumAmmoType	= GetAmmoDef()->Index("9mmRound");
	m_iLargeAmmoType	= GetAmmoDef()->Index("12mmRound");

	if ( m_iszSpriteSmoke != NULL_STRING )
		PrecacheModel( STRING(m_iszSpriteSmoke) );
	if ( m_iszSpriteFlash != NULL_STRING )
		PrecacheModel( STRING(m_iszSpriteFlash) );

	if ( m_soundStartRotate != NULL_STRING )
		PrecacheScriptSound( STRING(m_soundStartRotate) );
	if ( m_soundStopRotate != NULL_STRING )
		PrecacheScriptSound( STRING(m_soundStopRotate) );
	if ( m_soundLoopRotate != NULL_STRING )
		PrecacheScriptSound( STRING(m_soundLoopRotate) );
}



//==================================================================================
// TANK CONTROLLING
bool CFuncTank::OnControls( CBaseEntity *pTest )
{
	if ( !(m_spawnflags & SF_TANK_CANCONTROL) )
		return FALSE;

	Vector offset = pTest->GetLocalOrigin() - GetLocalOrigin();

	if ( (m_vecControllerUsePos - pTest->GetLocalOrigin()).Length() < 30 )
		return TRUE;

	return FALSE;
}

bool CFuncTank::StartControl( CBasePlayer *pController )
{
	if ( m_pController != NULL )
		return FALSE;

	// Team only or disabled?
	if ( m_iszMaster != NULL_STRING )
	{
		if ( !UTIL_IsMasterTriggered( m_iszMaster, pController ) )
			return FALSE;
	}

	// Holster player's weapon
	m_pController = pController;
	if ( m_pController->GetActiveWeapon() )
	{
		m_pController->GetActiveWeapon()->Holster();
	}

	m_pController->m_Local.m_iHideHUD |= HIDEHUD_WEAPONSELECTION;
	m_vecControllerUsePos = m_pController->GetLocalOrigin();
	
	SetNextThink( gpGlobals->curtime + 0.1f );
	
	return TRUE;
}

void CFuncTank::StopControl()
{
	// TODO: bring back the controllers current weapon
	if ( !m_pController )
		return;

	if ( m_pController->GetActiveWeapon() )
		m_pController->GetActiveWeapon()->Deploy();

	m_pController->m_Local.m_iHideHUD &= ~HIDEHUD_WEAPONSELECTION;
	SetNextThink( TICK_NEVER_THINK );
	m_pController = NULL;

	if ( IsActive() )
		SetNextThink( gpGlobals->curtime + 1.0f );
}

// Called each frame by the player's ItemPostFrame
void CFuncTank::ControllerPostFrame( void )
{
	Assert(m_pController != NULL);

	if ( gpGlobals->curtime < m_flNextAttack )
		return;

	Vector forward;
	AngleVectors( GetAbsAngles(), &forward );
	if ( m_pController->m_nButtons & IN_ATTACK )
	{
		m_fireLast = gpGlobals->curtime - (1/m_fireRate) - 0.01;  // to make sure the gun doesn't fire too many bullets

		int bulletCount = (gpGlobals->curtime - m_fireLast) * m_fireRate;

		Fire( bulletCount, WorldBarrelPosition(), forward, m_pController );
		
		// HACKHACK -- make some noise (that the AI can hear)
		CSoundEnt::InsertSound( SOUND_COMBAT, WorldSpaceCenter(), FUNCTANK_FIREVOLUME, 0.2 );

		m_flNextAttack = gpGlobals->curtime + (1/m_fireRate);
	}
}


void CFuncTank::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_spawnflags & SF_TANK_CANCONTROL )
	{  
		// player controlled turret
		CBasePlayer *pPlayer = ToBasePlayer( pActivator );
		if ( !pPlayer )
			return;

		if ( value == 2 && useType == USE_SET )
		{
			ControllerPostFrame();
		}
		else if ( !m_pController && useType != USE_OFF )
		{
			// The player must be within the func_tank controls
			Assert( m_hControlVolume );
			if ( !m_hControlVolume->IsTouching( pPlayer ) )
				return;

			pPlayer->SetUseEntity( this );
			StartControl( pPlayer );
		}
		else
		{
			StopControl();
		}
	}
	else
	{
		if ( !ShouldToggle( useType, IsActive() ) )
			return;

		if ( IsActive() )
		{
			TankDeactivate();
		}
		else
		{
			TankActivate();
		}
	}
}


bool CFuncTank::InRange( float range )
{
	if ( range < m_minRange )
		return FALSE;
	if ( m_maxRange > 0 && range > m_maxRange )
		return FALSE;

	return TRUE;
}


void CFuncTank::Think( void )
{
	// refresh the matrix
	UpdateMatrix();

	SetLocalAngularVelocity( vec3_angle );
	TrackTarget();

	if ( fabs(GetLocalAngularVelocity().x) > 1 || fabs(GetLocalAngularVelocity().y) > 1 )
		StartRotSound();
	else
		StopRotSound();
}


//-----------------------------------------------------------------------------
// Purpose: Aim the offset barrel at a position in parent space
// Input  : parentTarget - the position of the target in parent space
// Output : Vector - angles in local space
//-----------------------------------------------------------------------------
QAngle CFuncTank::AimBarrelAt( const Vector &parentTarget )
{
	Vector target = parentTarget - GetLocalOrigin();
	float quadTarget = target.LengthSqr();
	float quadTargetXY = target.x*target.x + target.y*target.y;

	// Target is too close!  Can't aim at it
	if ( quadTarget <= m_barrelPos.LengthSqr() )
	{
		return GetLocalAngles();
	}
	else
	{
		// We're trying to aim the offset barrel at an arbitrary point.
		// To calculate this, I think of the target as being on a sphere with 
		// it's center at the origin of the gun.
		// The rotation we need is the opposite of the rotation that moves the target 
		// along the surface of that sphere to intersect with the gun's shooting direction
		// To calculate that rotation, we simply calculate the intersection of the ray 
		// coming out of the barrel with the target sphere (that's the new target position)
		// and use atan2() to get angles

		// angles from target pos to center
		float targetToCenterYaw = atan2( target.y, target.x );
		float centerToGunYaw = atan2( m_barrelPos.y, sqrt( quadTarget - (m_barrelPos.y*m_barrelPos.y) ) );

		float targetToCenterPitch = atan2( target.z, sqrt( quadTargetXY ) );
		float centerToGunPitch = atan2( -m_barrelPos.z, sqrt( quadTarget - (m_barrelPos.z*m_barrelPos.z) ) );
		return QAngle( -RAD2DEG(targetToCenterPitch+centerToGunPitch), RAD2DEG( targetToCenterYaw - centerToGunYaw ), 0 );		
	}
}


void CFuncTank::TrackTarget( void )
{
	trace_t tr;
	bool updateTime = FALSE, lineOfSight;
	QAngle angles;
	Vector barrelEnd;
	CBaseEntity *pTarget = NULL;

	barrelEnd.Init();

	// Get a position to aim for
	if (m_pController)
	{
		// Tanks attempt to mirror the player's angles
		angles = m_pController->EyeAngles();
		SetNextThink( gpGlobals->curtime + 0.05 );
	}
	else
	{
		if ( IsActive() )
		{
			SetNextThink( gpGlobals->curtime + 0.1f );
		}
		else
		{
			return;
		}

		// -----------------------------------
		//  Get world target position
		// -----------------------------------
		barrelEnd = WorldBarrelPosition();
		Vector worldTargetPosition;
		if (m_spawnflags & SF_TANK_AIM_AT_POS)
		{
			worldTargetPosition = m_vTargetPosition;
		}
		else
		{
			CBaseEntity *pEntity = (CBaseEntity *)m_hTarget;
			if ( !pEntity || ( pEntity->GetFlags() & FL_NOTARGET ) )
			{
				if ( m_targetEntityName != NULL_STRING )	// New HL2 behavior
				{
					m_hTarget = FindTarget( m_targetEntityName, NULL );
				}
				else	// HL1 style
				{
					m_hTarget = ToBasePlayer( GetContainingEntity( UTIL_FindClientInPVS( edict() ) ) );
				}

				if ( m_hTarget != NULL )
				{
					SetNextThink( gpGlobals->curtime );	// Think again immediately
				}
				else
				{
					if ( IsActive() )
					{
						SetNextThink( gpGlobals->curtime + 2 );	// Wait 2 secs
					}

					if ( m_fireLast !=0 )
					{
						m_OnLoseTarget.FireOutput(this, this);
						m_fireLast = 0;
					}
				}

				return;
			}
			pTarget = pEntity;

			// Calculate angle needed to aim at target
			worldTargetPosition = pEntity->EyePosition();
		}

		float range = (worldTargetPosition - barrelEnd).Length();
		
		if ( !InRange( range ) )
		{
			m_fireLast = 0;
			return;
		}

		UTIL_TraceLine( barrelEnd, worldTargetPosition, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

		if (m_spawnflags & SF_TANK_AIM_AT_POS)
		{
			updateTime		= TRUE;
			m_sightOrigin	= m_vTargetPosition;
		}
		else
		{
			lineOfSight = FALSE;
			// No line of sight, don't track
			if ( tr.fraction == 1.0 || tr.m_pEnt == pTarget )
			{
				lineOfSight = TRUE;

				CBaseEntity *pInstance = pTarget;
				if ( InRange( range ) && pInstance && pInstance->IsAlive() )
				{
					updateTime = TRUE;

					// Sight position is BodyTarget with no noise (so gun doesn't bob up and down)
					m_sightOrigin = pInstance->BodyTarget( GetLocalOrigin(), false );
				}
			}
		}

		// Convert targetPosition to parent
		angles = AimBarrelAt( m_parentMatrix.WorldToLocal( m_sightOrigin ) );
	}

	// Force the angles to be relative to the center position
	float offsetY = UTIL_AngleDistance( angles.y, m_yawCenter );
	float offsetX = UTIL_AngleDistance( angles.x, m_pitchCenter );
	angles.y = m_yawCenter + offsetY;
	angles.x = m_pitchCenter + offsetX;

	// Limit against range in y

	// MDB - don't check pitch! If two func_tanks are meant to align,
	// and one can pitch and the other cannot, this can lead to them getting 
	// different values for angles.y. Nothing is lost by not updating yaw
	// because the target is not in pitch range.

	bool bOutsideYawRange = ( fabs( offsetY ) > m_yawRange + m_yawTolerance );
	bool bOutsidePitchRange = ( fabs( offsetX ) > m_pitchRange + m_pitchTolerance );

	Vector vecToTarget = m_sightOrigin - GetLocalOrigin();

	// if target is outside yaw range
	if ( bOutsideYawRange )
	{
		if ( angles.y > m_yawCenter + m_yawRange )
		{
			angles.y = m_yawCenter + m_yawRange;
		}
		else if ( angles.y < (m_yawCenter - m_yawRange) )
		{
			angles.y = (m_yawCenter - m_yawRange);
		}
	}

	if ( bOutsidePitchRange || bOutsideYawRange || ( vecToTarget.Length() < ( barrelEnd - GetAbsOrigin() ).Length() ) )
	{
		// Don't update if you saw the player, but out of range
		updateTime = false;
	}

	if ( updateTime )
	{
		m_lastSightTime = gpGlobals->curtime;
		m_persist2burst = 0;
	}

	// Move toward target at rate or less
	float distY = UTIL_AngleDistance( angles.y, GetLocalAngles().y );

	QAngle vecAngVel = GetLocalAngularVelocity();
	vecAngVel.y = distY * 10;
	vecAngVel.y = clamp( vecAngVel.y, -m_yawRate, m_yawRate );

	// Limit against range in x
	angles.x = clamp( angles.x, m_pitchCenter - m_pitchRange, m_pitchCenter + m_pitchRange );

	// Move toward target at rate or less
	float distX = UTIL_AngleDistance( angles.x, GetLocalAngles().x );
	vecAngVel.x = distX  * 10;
	vecAngVel.x = clamp( vecAngVel.x, -m_pitchRate, m_pitchRate );
	SetLocalAngularVelocity( vecAngVel );

	SetMoveDoneTime( 0.1 );
	if ( m_pController )
		return;

	if ( CanFire() && ( (fabs(distX) < m_pitchTolerance && fabs(distY) < m_yawTolerance) || (m_spawnflags & SF_TANK_LINEOFSIGHT) ) )
	{
		bool fire = FALSE;
		Vector forward;
		AngleVectors( GetLocalAngles(), &forward );
		forward = m_parentMatrix.ApplyRotation( forward );


		if ( m_spawnflags & SF_TANK_LINEOFSIGHT )
		{
			float length = (m_maxRange > 0) ? m_maxRange : MAX_TRACE_LENGTH;
			UTIL_TraceLine( barrelEnd, barrelEnd + forward * length, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

			if ( tr.m_pEnt == pTarget )
				fire = TRUE;
		}
		else
			fire = TRUE;

		if ( fire )
		{
			if (m_fireLast == 0)
			{
				m_OnAquireTarget.FireOutput(this, this);
			}
			FiringSequence( barrelEnd, forward, this );
		}
		else 
		{
			if (m_fireLast !=0)
			{
				m_OnLoseTarget.FireOutput(this, this);
			}
			m_fireLast = 0;
		}
	}
	else 
	{
		if (m_fireLast !=0)
		{
			m_OnLoseTarget.FireOutput(this, this);
		}
		m_fireLast = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Start of firing sequence.  By default, just fire now.
// Input  : &barrelEnd - 
//			&forward - 
//			*pAttacker - 
//-----------------------------------------------------------------------------
void CFuncTank::FiringSequence( const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker )
{
	if ( m_fireLast != 0 )
	{
		int bulletCount = (gpGlobals->curtime - m_fireLast) * m_fireRate;
		
		if ( bulletCount > 0 )
		{
			Fire( bulletCount, barrelEnd, forward, pAttacker );
			m_fireLast = gpGlobals->curtime;
		}
	}
	else
	{
		m_fireLast = gpGlobals->curtime;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Fire targets and spawn sprites.
// Input  : bulletCount - 
//			barrelEnd - 
//			forward - 
//			pAttacker - 
//-----------------------------------------------------------------------------
void CFuncTank::Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker )
{
	if ( m_iszSpriteSmoke != NULL_STRING )
	{
		CSprite *pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteSmoke), barrelEnd, TRUE );
		pSprite->AnimateAndDie( random->RandomFloat( 15.0, 20.0 ) );
		pSprite->SetTransparency( kRenderTransAlpha, m_clrRender->r, m_clrRender->g, m_clrRender->b, 255, kRenderFxNone );

		Vector vecVelocity( 0, 0, random->RandomFloat(40, 80) ); 
		pSprite->SetAbsVelocity( vecVelocity );
		pSprite->SetScale( m_spriteScale );
	}
	if ( m_iszSpriteFlash != NULL_STRING )
	{
		CSprite *pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteFlash), barrelEnd, TRUE );
		pSprite->AnimateAndDie( 60 );
		pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
		pSprite->SetScale( m_spriteScale );
	}

	m_OnFire.FireOutput(this, this);
}


void CFuncTank::TankTrace( const Vector &vecStart, const Vector &vecForward, const Vector &vecSpread, trace_t &tr )
{
	Vector forward, right, up;

	AngleVectors( GetAbsAngles(), &forward, &right, &up );
	// get circular gaussian spread
	float x, y, z;
	do {
		x = random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);
		y = random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);
		z = x*x+y*y;
	} while (z > 1);
	Vector vecDir = vecForward +
		x * vecSpread.x * right +
		y * vecSpread.y * up;
	Vector vecEnd;
	
	vecEnd = vecStart + vecDir * MAX_TRACE_LENGTH;
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
}

	
void CFuncTank::StartRotSound( void )
{
	if ( m_spawnflags & SF_TANK_SOUNDON )
		return;
	m_spawnflags |= SF_TANK_SOUNDON;
	
	if ( m_soundLoopRotate != NULL_STRING )
	{
		CPASAttenuationFilter filter( this );
		filter.MakeReliable();

		EmitSound_t ep;
		ep.m_nChannel = CHAN_STATIC;
		ep.m_pSoundName = (char*)STRING(m_soundLoopRotate);
		ep.m_flVolume = 0.85f;
		ep.m_SoundLevel = SNDLVL_NORM;

		EmitSound( filter, entindex(), ep );
	}
	
	if ( m_soundStartRotate != NULL_STRING )
	{
		CPASAttenuationFilter filter( this );

		EmitSound_t ep;
		ep.m_nChannel = CHAN_BODY;
		ep.m_pSoundName = (char*)STRING(m_soundStartRotate);
		ep.m_flVolume = 1.0f;
		ep.m_SoundLevel = SNDLVL_NORM;

		EmitSound( filter, entindex(), ep );
	}
}


void CFuncTank::StopRotSound( void )
{
	if ( m_spawnflags & SF_TANK_SOUNDON )
	{
		if ( m_soundLoopRotate != NULL_STRING )
		{
			StopSound( entindex(), CHAN_STATIC, (char*)STRING(m_soundLoopRotate) );
		}
		if ( m_soundStopRotate != NULL_STRING )
		{
			CPASAttenuationFilter filter( this );

			EmitSound_t ep;
			ep.m_nChannel = CHAN_BODY;
			ep.m_pSoundName = (char*)STRING(m_soundStopRotate);
			ep.m_flVolume = 1.0f;
			ep.m_SoundLevel = SNDLVL_NORM;

			EmitSound( filter, entindex(), ep );
		}
	}
	m_spawnflags &= ~SF_TANK_SOUNDON;
}

// #############################################################################
//   CFuncTankGun
// #############################################################################
class CFuncTankGun : public CFuncTank
{
public:
	DECLARE_CLASS( CFuncTankGun, CFuncTank );

	void Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker );
};
LINK_ENTITY_TO_CLASS( func_tank, CFuncTankGun );

void CFuncTankGun::Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker )
{
	int i;

	for ( i = 0; i < bulletCount; i++ )
	{
		switch( m_bulletType )
		{
		case TANK_BULLET_SMALL:
			FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], MAX_TRACE_LENGTH, m_iSmallAmmoType, 1,  -1, -1, m_iBulletDamage, pAttacker );
			break;

		case TANK_BULLET_MEDIUM:
			FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], MAX_TRACE_LENGTH, m_iMediumAmmoType, 1, -1, -1, m_iBulletDamage, pAttacker );
			break;

		case TANK_BULLET_LARGE:
			FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], MAX_TRACE_LENGTH, m_iLargeAmmoType, 1,  -1, -1, m_iBulletDamage, pAttacker );
			break;
		default:
		case TANK_BULLET_NONE:
			break;
		}
	}
	CFuncTank::Fire( bulletCount, barrelEnd, forward, pAttacker );
}

// #############################################################################
//   CFuncTankPulseLaser
// #############################################################################
class CFuncTankPulseLaser : public CFuncTankGun
{
public:
	DECLARE_CLASS( CFuncTankPulseLaser, CFuncTankGun );
	DECLARE_DATADESC();

	void Precache();
	void Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker );

	float		m_flPulseSpeed;
	float		m_flPulseWidth;
	color32		m_flPulseColor;
	float		m_flPulseLife;
	float		m_flPulseLag;
	string_t	m_sPulseFireSound;
};
LINK_ENTITY_TO_CLASS( func_tankpulselaser, CFuncTankPulseLaser );

BEGIN_DATADESC( CFuncTankPulseLaser )

	DEFINE_KEYFIELD( m_flPulseSpeed,	 FIELD_FLOAT,		"PulseSpeed" ),
	DEFINE_KEYFIELD( m_flPulseWidth,	 FIELD_FLOAT,		"PulseWidth" ),
	DEFINE_KEYFIELD( m_flPulseColor,	 FIELD_COLOR32,		"PulseColor" ),
	DEFINE_KEYFIELD( m_flPulseLife,	 FIELD_FLOAT,		"PulseLife" ),
	DEFINE_KEYFIELD( m_flPulseLag,		 FIELD_FLOAT,		"PulseLag" ),
	DEFINE_KEYFIELD( m_sPulseFireSound, FIELD_SOUNDNAME,	"PulseFireSound" ),

END_DATADESC()

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CFuncTankPulseLaser::Precache(void)
{
	UTIL_PrecacheOther( "grenade_beam" );

	if ( m_sPulseFireSound != NULL_STRING )
	{
		PrecacheScriptSound( STRING(m_sPulseFireSound) );
	}
	BaseClass::Precache();
}
//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CFuncTankPulseLaser::Fire( int bulletCount, const Vector &barrelEnd, const Vector &vecForward, CBaseEntity *pAttacker )
{
	// --------------------------------------------------
	//  Get direction vectors for spread
	// --------------------------------------------------
	Vector vecUp = Vector(0,0,1);
	Vector vecRight;
	CrossProduct ( vecForward,  vecUp,		vecRight );	
	CrossProduct ( vecForward, -vecRight,   vecUp  );	

	for ( int i = 0; i < bulletCount; i++ )
	{
		// get circular gaussian spread
		float x, y, z;
		do {
			x = random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);
			y = random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);
			z = x*x+y*y;
		} while (z > 1);

		Vector vecDir = vecForward + x * gTankSpread[m_spread].x * vecRight + y * gTankSpread[m_spread].y * vecUp;

		CGrenadeBeam *pPulse =  CGrenadeBeam::Create( pAttacker, barrelEnd);
		pPulse->Format(m_flPulseColor, m_flPulseWidth);
		pPulse->Shoot(vecDir,m_flPulseSpeed,m_flPulseLife,m_flPulseLag,m_iBulletDamage);

		if ( m_sPulseFireSound != NULL_STRING )
		{
			CPASAttenuationFilter filter( this, 0.6f );

			EmitSound_t ep;
			ep.m_nChannel = CHAN_WEAPON;
			ep.m_pSoundName = (char*)STRING(m_sPulseFireSound);
			ep.m_flVolume = 1.0f;
			ep.m_SoundLevel = ATTN_TO_SNDLVL( 0.6 );

			EmitSound( filter, entindex(), ep );
		}

	}
	CFuncTank::Fire( bulletCount, barrelEnd, vecForward, pAttacker );
}

// #############################################################################
//   CFuncTankLaser
// #############################################################################
class CFuncTankLaser : public CFuncTank
{
	DECLARE_CLASS( CFuncTankLaser, CFuncTank );
public:
	void	Activate( void );
	void	Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker );
	void	Think( void );
	CEnvLaser *GetLaser( void );
	void	UpdateOnRemove( void );

	DECLARE_DATADESC();

private:
	CEnvLaser	*m_pLaser;
	float	m_laserTime;
	string_t m_iszLaserName;
};
LINK_ENTITY_TO_CLASS( func_tanklaser, CFuncTankLaser );

BEGIN_DATADESC( CFuncTankLaser )

	DEFINE_KEYFIELD( m_iszLaserName, FIELD_STRING, "laserentity" ),

	DEFINE_FIELD( m_pLaser, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_laserTime, FIELD_TIME ),

END_DATADESC()


void CFuncTankLaser::Activate( void )
{
	BaseClass::Activate();

	if ( !GetLaser() )
	{
		UTIL_Remove(this);
		Warning( "Laser tank with no env_laser!\n" );
	}
	else
	{
		m_pLaser->TurnOff();
	}
}

void CFuncTankLaser::UpdateOnRemove( void )
{
	if( GetLaser() )
	{
		m_pLaser->TurnOff();
	}

	BaseClass::UpdateOnRemove();
}

CEnvLaser *CFuncTankLaser::GetLaser( void )
{
	if ( m_pLaser )
		return m_pLaser;

	CBaseEntity *pLaser = gEntList.FindEntityByName( NULL, m_iszLaserName );
	while ( pLaser )
	{
		// Found the landmark
		if ( FClassnameIs( pLaser, "env_laser" ) )
		{
			m_pLaser = (CEnvLaser *)pLaser;
			break;
		}
		else
		{
			pLaser = gEntList.FindEntityByName( pLaser, m_iszLaserName );
		}
	}

	return m_pLaser;
}


void CFuncTankLaser::Think( void )
{
	if ( m_pLaser && (gpGlobals->curtime > m_laserTime) )
		m_pLaser->TurnOff();

	CFuncTank::Think();
}


void CFuncTankLaser::Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker )
{
	int i;
	trace_t tr;

	if ( GetLaser() )
	{
		for ( i = 0; i < bulletCount; i++ )
		{
			m_pLaser->SetLocalOrigin( barrelEnd );
			TankTrace( barrelEnd, forward, gTankSpread[m_spread], tr );
			
			m_laserTime = gpGlobals->curtime;
			m_pLaser->TurnOn();
			m_pLaser->SetFireTime( gpGlobals->curtime - 1.0 );
			m_pLaser->FireAtPoint( tr );
			m_pLaser->SetNextThink( TICK_NEVER_THINK );
		}
		CFuncTank::Fire( bulletCount, barrelEnd, forward, this );
	}
}

class CFuncTankRocket : public CFuncTank
{
public:
	DECLARE_CLASS( CFuncTankRocket, CFuncTank );

	void Precache( void );
	void Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker );
};
LINK_ENTITY_TO_CLASS( func_tankrocket, CFuncTankRocket );

void CFuncTankRocket::Precache( void )
{
	UTIL_PrecacheOther( "rpg_rocket" );
	CFuncTank::Precache();
}



void CFuncTankRocket::Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker )
{
	int i;

	for ( i = 0; i < bulletCount; i++ )
	{
		CBaseEntity *pRocket = CBaseEntity::Create( "rpg_rocket", barrelEnd, GetAbsAngles(), this );
		pRocket->SetAbsAngles( GetAbsAngles() );
	}
	CFuncTank::Fire( bulletCount, barrelEnd, forward, this );
}


class CFuncTankMortar : public CFuncTank
{
public:
	DECLARE_CLASS( CFuncTankMortar, CFuncTank );

	void Precache( void );
	void FiringSequence( const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker );
	void Fire( int bulletCount, const Vector &barrelEnd, const Vector &vecForward, CBaseEntity *pAttacker );
	void ShootGun(void);
	
	// Input handlers.
	void InputShootGun( inputdata_t &inputdata );

	DECLARE_DATADESC();

	int			m_Magnitude;
	float		m_fireDelay;
	string_t	m_fireStartSound;
	string_t	m_fireEndSound;

	// store future firing event
	CBaseEntity *m_pAttacker;
};

LINK_ENTITY_TO_CLASS( func_tankmortar, CFuncTankMortar );

BEGIN_DATADESC( CFuncTankMortar )

	DEFINE_KEYFIELD( m_Magnitude, FIELD_INTEGER, "iMagnitude" ),
	DEFINE_KEYFIELD( m_fireDelay, FIELD_FLOAT, "firedelay" ),
	DEFINE_KEYFIELD( m_fireStartSound, FIELD_STRING, "firestartsound" ),
	DEFINE_KEYFIELD( m_fireEndSound, FIELD_STRING, "fireendsound" ),

	DEFINE_FIELD( m_pAttacker, FIELD_CLASSPTR ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "ShootGun", InputShootGun ),

END_DATADESC()



void CFuncTankMortar::Precache( void )
{
	if ( m_fireStartSound != NULL_STRING )
		PrecacheScriptSound( STRING(m_fireStartSound) );
	if ( m_fireEndSound != NULL_STRING )
		PrecacheScriptSound( STRING(m_fireEndSound) );
	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: Input handler to make the tank shoot.
//-----------------------------------------------------------------------------
void CFuncTankMortar::InputShootGun( inputdata_t &inputdata )
{
	ShootGun();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTankMortar::ShootGun( void )
{
	Vector forward;
	AngleVectors( GetLocalAngles(), &forward );
	UpdateMatrix();
	forward = m_parentMatrix.ApplyRotation( forward );

	// use cached firing state
	Fire( 1, WorldBarrelPosition(), forward, m_pAttacker ); 
}


void CFuncTankMortar::FiringSequence( const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker )
{
	if ( m_fireLast != 0 )
	{
		int bulletCount = (gpGlobals->curtime - m_fireLast) * m_fireRate;
		// Only create 1 explosion
		if ( bulletCount > 0 )
		{
			// fire in "firedelay" seconds
			if ( m_fireStartSound != NULL_STRING )
			{
				CPASAttenuationFilter filter( this );

				EmitSound_t ep;
				ep.m_nChannel = CHAN_WEAPON;
				ep.m_pSoundName = (char*)STRING(m_fireStartSound);
				ep.m_flVolume = 1.0f;
				ep.m_SoundLevel = SNDLVL_NORM;

				EmitSound( filter, entindex(), ep );
			}

			if ( m_fireDelay != 0 )
			{
				g_EventQueue.AddEvent( this, "ShootGun", m_fireDelay, pAttacker, this, 0 );
			}
			else
			{
				ShootGun();
			}
			m_fireLast = gpGlobals->curtime;
		}
	}
	else
	{
		m_fireLast = gpGlobals->curtime;
	}
}	

void CFuncTankMortar::Fire( int bulletCount, const Vector &barrelEnd, const Vector &vecForward, CBaseEntity *pAttacker )
{
	if ( m_fireEndSound != NULL_STRING )
	{
		CPASAttenuationFilter filter( this );
		
		EmitSound_t ep;
		ep.m_nChannel = CHAN_WEAPON;
		ep.m_pSoundName = STRING(m_fireEndSound);
		ep.m_flVolume = 1.0f;
		ep.m_SoundLevel = SNDLVL_NORM;

		EmitSound( filter, entindex(), ep );
	}

	trace_t tr;

	TankTrace( barrelEnd, vecForward, gTankSpread[m_spread], tr );

	const CBaseEntity * ent = NULL;
	if ( g_pGameRules->IsMultiplayer() )
	{
		// temp remove suppress host
		ent = te->GetSuppressHost();
		te->SetSuppressHost( NULL );
	}

	ExplosionCreate( tr.endpos, GetAbsAngles(), this, m_Magnitude, 0, true );

	if ( g_pGameRules->IsMultiplayer() )
	{
		te->SetSuppressHost( (CBaseEntity *) ent );
	}

	BaseClass::Fire( bulletCount, barrelEnd, vecForward, this );
}

//-----------------------------------------------------------------------------
// Purpose: Func tank that fires physics cannisters placed on it
//-----------------------------------------------------------------------------
class CFuncTankPhysCannister : public CFuncTank
{
public:
	DECLARE_CLASS( CFuncTankPhysCannister, CFuncTank );
	DECLARE_DATADESC();

	void Activate( void );
	void Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker );

protected:
	string_t				m_iszBarrelVolume;
	CHandle<CBaseTrigger>	m_hBarrelVolume;
};

LINK_ENTITY_TO_CLASS( func_tankphyscannister, CFuncTankPhysCannister );

BEGIN_DATADESC( CFuncTankPhysCannister )

	DEFINE_KEYFIELD( m_iszBarrelVolume, FIELD_STRING, "barrel_volume" ),
	DEFINE_FIELD( m_hBarrelVolume, FIELD_EHANDLE ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTankPhysCannister::Activate( void )
{
	BaseClass::Activate();

	m_hBarrelVolume = NULL;

	// Find our barrel volume
	if ( m_iszBarrelVolume != NULL_STRING )
	{
		m_hBarrelVolume = dynamic_cast<CBaseTrigger*>( gEntList.FindEntityByName( NULL, m_iszBarrelVolume ) );
	}

	if ( !m_hBarrelVolume )
	{
		Msg("ERROR: Couldn't find barrel volume for func_tankphyscannister %s.\n", STRING(GetEntityName()) );
		UTIL_Remove( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTankPhysCannister::Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker )
{
	Assert( m_hBarrelVolume );

	// Do we have a cannister in our barrel volume?
	CPhysicsCannister *pCannister = (CPhysicsCannister *)m_hBarrelVolume->GetTouchedEntityOfType( "physics_cannister" );
	if ( !pCannister )
	{
		// Play a no-ammo sound
		return;
	}

	// Fire the cannister!
	pCannister->CannisterFire( pAttacker );
}

