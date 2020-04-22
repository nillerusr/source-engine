//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:			The Escort's Shield weapon
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "in_buttons.h"
#include "tf_shieldshared.h"
#include "tf_shareddefs.h"
#include "baseentity_shared.h"

#if defined( CLIENT_DLL )

#include "c_shield.h"

#else

#include "tf_shield.h"
#include "gamerules.h"

#endif


#if defined( CLIENT_DLL )
#define CShieldMobile C_ShieldMobile
#define CShield C_Shield
#endif


//-----------------------------------------------------------------------------
// ConVars
//-----------------------------------------------------------------------------
ConVar	shield_mobile_power( "shield_mobile_power","30", FCVAR_REPLICATED, "Max power level of a escort's mobile projected shield." );
ConVar	shield_mobile_recharge_delay( "shield_mobile_recharge_delay","0.1", FCVAR_REPLICATED, "Time after taking damage before mobile projected shields begin to recharge." );
ConVar	shield_mobile_recharge_amount( "shield_mobile_recharge_amount","2", FCVAR_REPLICATED, "Power recharged each recharge tick for mobile projected shields." );
ConVar	shield_mobile_recharge_time( "shield_mobile_recharge_time","0.5", FCVAR_REPLICATED, "Time between each recharge tick for mobile projected shields." );


#define EMP_WAVE_AMPLITUDE 8.0f


//-----------------------------------------------------------------------------
// Mobile version of the shield 
//-----------------------------------------------------------------------------
class CShieldMobile;
class CShieldMobileActiveVertList : public IActiveVertList
{
public:
	void			Init( CShieldMobile *pShield );

// IActiveVertList overrides.
public:

	virtual int		GetActiveVertState( int iVert );
	virtual void	SetActiveVertState( int iVert, int bOn );

private:
	CShieldMobile	*m_pShield;
};


//-----------------------------------------------------------------------------
// Mobile version of the shield 
//-----------------------------------------------------------------------------
class CShieldMobile : public CShield, public IEntityEnumerator
{
	DECLARE_CLASS( CShieldMobile, CShield );

public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	friend class CShieldMobileActiveVertList;

	CShieldMobile();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif

public:
	void Spawn( void );
	void Precache( void );
	void ShieldThink( void );
	virtual void ClientThink();
	virtual void SetAngularSpringConstant( float flConstant );
	virtual void SetFrontDistance( float flDistance );
	virtual void ComputeWorldSpaceSurroundingBox( Vector *pWorldMins, Vector *pWorldMaxs );

	virtual void SetAttachmentIndex( int nAttachmentIndex );
	virtual void SetEMPed( bool isEmped );
	virtual void SetAlwaysOrient( bool bOrient );
 	virtual bool IsAlwaysOrienting( );

	virtual int Width();
	virtual int Height();
	virtual bool IsPanelActive( int x, int y );
	virtual const Vector& GetPoint( int x, int y );
	virtual void SetCenterAngles( const QAngle& angles );
	virtual void SetThetaPhi( float flTheta, float flPhi );

	virtual void GetRenderBounds( Vector& mins, Vector& maxs );

	// All predicted weapons need to implement and return true
	virtual bool IsPredicted( void ) const
	{ 
		return true;
	}

public:
#ifdef CLIENT_DLL
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void GetBounds( Vector& mins, Vector& maxs );
	virtual void AddEntity( );
	virtual void GetShieldData( const Vector** ppVerts, float* pOpacity, float* pBlend );

	virtual bool	ShouldPredict( void )
	{
		if ( GetOwnerEntity() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

#endif

public:
	// Inherited from IEntityEnumerator
	virtual bool EnumEntity( IHandleEntity *pHandleEntity ); 

private:
	// Teleport!
	void OnTeleported(  );
	void SimulateShield( void );

private:
	struct SweepContext_t
	{
		SweepContext_t( const CBaseEntity *passentity, int collisionGroup ) :
			m_Filter( passentity, collisionGroup ) {}

		CTraceFilterSimple m_Filter;
		Vector m_vecStartDelta;
		Vector m_vecEndDelta;
	};

	enum
	{
		NUM_SUBDIVISIONS = 21,
	};

	enum
	{
		SHIELD_ORIENT_TO_OWNER = 0x2
	};

private:
	CShieldMobile( const CShieldMobile & );
	void ComputeBoundingBox( void );
	void DetermineObstructions( );

private:
#ifdef CLIENT_DLL
	// Is a particular panel an edge?
	bool IsVertexValid( float s, float t ) const;
	void PreRender( );
#endif

private:
	CShieldMobileActiveVertList	m_VertList;
	QAngle m_tmpAngLockedAngles;

	// Bitfield indicating which vertices are active
	CShieldEffect m_ShieldEffect;
	CNetworkArray( unsigned char, m_pVertsActive, SHIELD_NUM_CONTROL_POINTS >> 3 );
	CNetworkVar( unsigned char, m_ShieldState );
	CNetworkVar( float, m_flFrontDistance );
	CNetworkVar( QAngle, m_angLockedAngles );
	SweepContext_t *m_pEnumCtx;

	// This is the width + height of the shield, not the current theta, phi
	CNetworkVar( float, m_flShieldTheta );
	CNetworkVar( float, m_flShieldPhi );
	CNetworkVar( float, m_flSpringConstant );
	CNetworkVar( int, m_nAttachmentIndex );
};


//=============================================================================
// Shield effect
//=============================================================================
#ifndef CLIENT_DLL

BEGIN_DATADESC( CShieldMobile )

	DEFINE_THINKFUNC( ShieldThink ),

END_DATADESC()

#endif

LINK_ENTITY_TO_CLASS( shield_mobile, CShieldMobile );

IMPLEMENT_NETWORKCLASS_ALIASED( ShieldMobile, DT_ShieldMobile );

// -------------------------------------------------------------------------------- //
// This data only gets sent to clients that ARE this player entity.
// -------------------------------------------------------------------------------- //

BEGIN_NETWORK_TABLE(CShieldMobile, DT_ShieldMobile)
#if !defined( CLIENT_DLL )
	SendPropInt		(SENDINFO(m_ShieldState),	2, SPROP_UNSIGNED ),
	SendPropArray(
		SendPropInt( SENDINFO_ARRAY(m_pVertsActive), 8, SPROP_UNSIGNED),
		m_pVertsActive),

	SendPropFloat( SENDINFO(m_flFrontDistance), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flShieldTheta), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flShieldPhi), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flSpringConstant), 0, SPROP_NOSCALE ),
	SendPropQAngles( SENDINFO(m_angLockedAngles), 9 ),
	SendPropInt	(SENDINFO(m_nAttachmentIndex),	10, SPROP_UNSIGNED ),

	// Don't bother sending these, they are totally controlled by the think function
	SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),
	SendPropExclude( "DT_BaseEntity", "m_angAbsRotation[0]" ),
	SendPropExclude( "DT_BaseEntity", "m_angAbsRotation[1]" ),
	SendPropExclude( "DT_BaseEntity", "m_angAbsRotation[2]" ),

#else
	RecvPropInt( RECVINFO(m_ShieldState) ),
	RecvPropArray( 
		RecvPropInt( RECVINFO(m_pVertsActive[0])), 
		m_pVertsActive 
	),
	RecvPropFloat( RECVINFO(m_flFrontDistance) ),
	RecvPropFloat( RECVINFO(m_flShieldTheta) ),
	RecvPropFloat( RECVINFO(m_flShieldPhi) ),
	RecvPropFloat( RECVINFO(m_flSpringConstant) ),
	RecvPropQAngles( RECVINFO( m_angLockedAngles ) ),
	RecvPropInt	(RECVINFO(m_nAttachmentIndex) ),
#endif

END_NETWORK_TABLE()


BEGIN_PREDICTION_DATA( CShieldMobile )

	DEFINE_PRED_FIELD( m_ShieldState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flFrontDistance, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_angLockedAngles, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_TYPEDESCRIPTION( m_ShieldEffect, CShieldEffect ),
	DEFINE_FIELD( m_nNextThinkTick, FIELD_INTEGER ),
	DEFINE_FIELD( m_tmpAngLockedAngles, FIELD_VECTOR ),

	// FIXME: How can I make this work now that I have an embedded collision property?
//	DEFINE_PRED_FIELD( m_vecMins, FIELD_VECTOR, FTYPEDESC_INSENDTABLE | FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
//	DEFINE_PRED_FIELD( m_vecMaxs, FIELD_VECTOR, FTYPEDESC_INSENDTABLE | FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),

#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_vecNetworkOrigin, FIELD_VECTOR, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_angNetworkAngles, FIELD_VECTOR, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
#endif

END_PREDICTION_DATA()


	
//-----------------------------------------------------------------------------
// CShieldMobileActiveVertList functions
//-----------------------------------------------------------------------------
void CShieldMobileActiveVertList::Init( CShieldMobile *pShield )
{
	m_pShield = pShield;
}


int CShieldMobileActiveVertList::GetActiveVertState( int iVert )
{
	return m_pShield->m_pVertsActive[iVert>>3] & (1 << (iVert & 7));
}


void CShieldMobileActiveVertList::SetActiveVertState( int iVert, int bOn )
{
	unsigned char val;	
	if ( bOn )
		val = m_pShield->m_pVertsActive[iVert>>3] | (1 << (iVert & 7));
	else
		val = m_pShield->m_pVertsActive[iVert>>3] & ~(1 << (iVert & 7));

	m_pShield->m_pVertsActive.Set( iVert>>3, val );
}


//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CShieldMobile::CShieldMobile()
{
	SetPredictionEligible( true );
	m_VertList.Init( this );
	m_flFrontDistance = 0;
	SetAngularSpringConstant( 2.0f );
	SetThetaPhi( SHIELD_INITIAL_THETA, SHIELD_INITIAL_PHI ); 
	m_nAttachmentIndex = 0;

#ifdef CLIENT_DLL
	InitShield( SHIELD_NUM_HORIZONTAL_POINTS, SHIELD_NUM_VERTICAL_POINTS, NUM_SUBDIVISIONS );
#endif
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CShieldMobile::Precache( void )
{
	m_ShieldEffect.SetActiveVertexList( &m_VertList );
	m_ShieldEffect.SetCollisionGroup( TFCOLLISION_GROUP_SHIELD );
	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CShieldMobile::Spawn( void )
{
	Precache();
	BaseClass::Spawn();
	AddSolidFlags( FSOLID_FORCE_WORLD_ALIGNED );

//	Assert( GetOwnerEntity() );
	m_angLockedAngles.Set( vec3_angle );
	m_tmpAngLockedAngles = vec3_angle;
	m_ShieldEffect.Spawn(GetAbsOrigin(), GetAbsAngles());
	m_ShieldState = 0;
	SetAlwaysOrient( true );

	// All movement occurs during think
	SetMoveType( MOVETYPE_NONE );

	SetThink( ShieldThink );
	SetNextThink( gpGlobals->curtime + 0.01f );

#ifdef CLIENT_DLL
	// This goofiness is required so that non-predicted entities work
	SetNextClientThink( CLIENT_THINK_ALWAYS );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CShieldMobile::SetAlwaysOrient( bool bOrient ) 
{
	if (bOrient)
	{
		m_ShieldState.Set( m_ShieldState.Get() | SHIELD_ORIENT_TO_OWNER );
	}
	else
	{
		m_angLockedAngles.Set( m_tmpAngLockedAngles );
		m_ShieldState.Set( m_ShieldState.Get() & (~SHIELD_ORIENT_TO_OWNER) );
	}
}

int CShieldMobile::Width()
{ 
	return SHIELD_NUM_HORIZONTAL_POINTS; 
}

int CShieldMobile::Height()
{ 
	return SHIELD_NUM_VERTICAL_POINTS; 
}

const Vector& CShieldMobile::GetPoint( int x, int y )
{ 
	return m_ShieldEffect.GetPoint( x, y ); 
}

	
void CShieldMobile::SetAttachmentIndex( int nAttachmentIndex )
{
	m_nAttachmentIndex = nAttachmentIndex;
}


//-----------------------------------------------------------------------------
// Returns the render bounds
//-----------------------------------------------------------------------------
void CShieldMobile::GetRenderBounds( Vector& mins, Vector& maxs )
{
	mins = m_ShieldEffect.GetRenderMins();
	maxs = m_ShieldEffect.GetRenderMaxs();
}

//-----------------------------------------------------------------------------
// Return true if the panel is active 
//-----------------------------------------------------------------------------
bool CShieldMobile::IsPanelActive( int x, int y )
{
	return m_ShieldEffect.IsPanelActive(x, y);
}


//-----------------------------------------------------------------------------
// Called when the shield is EMPed
//-----------------------------------------------------------------------------
void CShieldMobile::SetEMPed( bool isEmped )
{
	CShield::SetEMPed(isEmped);
	if (IsEMPed())
		m_ShieldState |= SHIELD_MOBILE_EMP;
	else
		m_ShieldState &= ~SHIELD_MOBILE_EMP;
}


//-----------------------------------------------------------------------------
// Set the shield angles
//-----------------------------------------------------------------------------
void CShieldMobile::SetCenterAngles( const QAngle& angles )
{
	// The tmp ang locked angles is simply there to prevent unnecessary network traffic
	m_tmpAngLockedAngles = angles;
	if ( ( m_ShieldState.Get() & SHIELD_ORIENT_TO_OWNER ) == 0 ) 
	{
		m_angLockedAngles.Set( angles );
	}
}

bool CShieldMobile::IsAlwaysOrienting( )
{
	return ( m_ShieldState.Get() & SHIELD_ORIENT_TO_OWNER ) != 0;
}

void CShieldMobile::SetAngularSpringConstant( float flConstant )
{
	m_flSpringConstant = flConstant;
	m_ShieldEffect.SetAngularSpringConstant( flConstant );
}

void CShieldMobile::SetFrontDistance( float flDistance )
{
	m_flFrontDistance = flDistance;
}

void CShieldMobile::SetThetaPhi( float flTheta, float flPhi )
{ 
	// This sets the bounds of the shield; how tall + wide is it?
	m_flShieldTheta = flTheta;
	m_flShieldPhi = flPhi;
	m_ShieldEffect.SetThetaPhi(flTheta, flPhi); 
}


//-----------------------------------------------------------------------------
// Computes the shield bounding box
//-----------------------------------------------------------------------------
void CShieldMobile::ComputeBoundingBox( void )
{
	Vector mins, maxs;
	m_ShieldEffect.ComputeBounds(mins, maxs); 
	SetCollisionBounds( mins, maxs );
}


//-----------------------------------------------------------------------------
// Compute world axis-aligned bounding box
//-----------------------------------------------------------------------------
void CShieldMobile::ComputeWorldSpaceSurroundingBox( Vector *pWorldMins, Vector *pWorldMaxs )
{
	// We don't use USE_SPECIFIED_BOUNDS because that would generate a ton of network traffic
	VectorAdd( CollisionProp()->GetCollisionOrigin(), CollisionProp()->OBBMins(), *pWorldMins );
	VectorAdd( CollisionProp()->GetCollisionOrigin(), CollisionProp()->OBBMaxs(), *pWorldMaxs );
}


//-----------------------------------------------------------------------------
// Determines shield obstructions 
//-----------------------------------------------------------------------------
void CShieldMobile::DetermineObstructions( )
{
	m_ShieldEffect.ComputeVertexActivity();
}


//-----------------------------------------------------------------------------
// Called by the enumerator call in ShieldThink 
//-----------------------------------------------------------------------------
bool CShieldMobile::EnumEntity( IHandleEntity *pHandleEntity )
{
#ifdef CLIENT_DLL
	CBaseEntity *pOther = cl_entitylist->GetBaseEntityFromHandle( pHandleEntity->GetRefEHandle() );
#else
	CBaseEntity *pOther = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
#endif

	if (!pOther)
		return true;

	// Blow off non-solid things
	if ( !::IsSolid(pOther->GetSolid(), pOther->GetSolidFlags()) )
		return true;

	// No model, blow it off
	if ( !pOther->GetModelIndex() )
		return true;
	
	// Blow off point-sized things....
	if ( pOther->IsPointSized() )
		return true;

	// Don't bother if we shouldn't be colliding with this guy...
	if (!m_pEnumCtx->m_Filter.ShouldHitEntity( pOther, MASK_SOLID ))
		return true;

	// The shield is in its final position, so we're gonna have to determine the
	// point of collision by working in the space of the final position....
	// We do this by moving the obstruction by the relative movement amount...
	Vector vecObsMins, vecObsMaxs, vecObsCenter;
	pOther->CollisionProp()->WorldSpaceAABB( &vecObsMins, &vecObsMaxs );
	vecObsCenter = (vecObsMins + vecObsMaxs) * 0.5f;
	vecObsMins -= vecObsCenter;
	vecObsMaxs -= vecObsCenter;

	Vector vecStart, vecEnd;
	VectorAdd( vecObsCenter, m_pEnumCtx->m_vecStartDelta, vecStart );
	VectorAdd( vecStart, m_pEnumCtx->m_vecEndDelta, vecEnd );


	Ray_t ray;
	ray.Init( vecStart, vecEnd, vecObsMins, vecObsMaxs );

	trace_t tr;
	if (TestCollision( ray, pOther->PhysicsSolidMaskForEntity(), tr ))
	{
		// Ok, we got a collision. Let's indicate it happened...
		// At the moment, we'll report the collision point as being on the
		// surface of the shield in its final position, which is kind of bogus...
		pOther->PhysicsImpact( this, tr );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Update the shield position: 
//-----------------------------------------------------------------------------
void CShieldMobile::SimulateShield( void )
{
	CBaseEntity *owner = GetOwnerEntity();
	Vector origin;
	if ( owner )
	{
		if ( m_ShieldState & SHIELD_ORIENT_TO_OWNER ) 
		{
			if ( owner->IsPlayer() )
			{
				m_ShieldEffect.SetDesiredAngles( owner->EyeAngles() );
			}
			else
			{
				m_ShieldEffect.SetDesiredAngles( owner->GetAbsAngles() );
			}
		}
		else
		{
			m_ShieldEffect.SetDesiredAngles( m_angLockedAngles );
		}

		if ( m_nAttachmentIndex == 0 )
		{
			origin = owner->EyePosition();
		}
		else
		{
			QAngle angles;
			CBaseAnimating *pAnim = dynamic_cast<CBaseAnimating*>( owner );
			if (pAnim)
			{
				pAnim->GetAttachment( m_nAttachmentIndex, origin, angles );
			}
			else
			{
				origin = owner->EyePosition();
			}
		}

		if ( m_flFrontDistance )
		{
			Vector vForward;
			AngleVectors( m_ShieldEffect.GetDesiredAngles(), &vForward );
			origin += vForward * m_flFrontDistance;
		}
	}
	else
	{
		Assert( 0 );
		origin = vec3_origin;
	}

	// We pretty much always need to recompute this
	CollisionProp()->MarkSurroundingBoundsDirty();

	Vector vecOldOrigin = m_ShieldEffect.GetCurrentPosition();
	
	Vector vecDelta;
	VectorSubtract( origin, vecOldOrigin, vecDelta );

	float flMaxDist = 100 + m_flFrontDistance;
	if (vecDelta.LengthSqr() > flMaxDist * flMaxDist )
	{
		OnTeleported();
		return;
	}

	m_ShieldEffect.SetDesiredOrigin( origin );
	m_ShieldEffect.Simulate(gpGlobals->frametime);
	DetermineObstructions();
	SetAbsOrigin( m_ShieldEffect.GetCurrentPosition() );
	SetAbsAngles( m_ShieldEffect.GetCurrentAngles() );

#ifdef CLIENT_DLL
	// Necessary because we exclude the network origin
	SetNetworkOrigin( m_ShieldEffect.GetCurrentPosition() );
	SetNetworkAngles( m_ShieldEffect.GetCurrentAngles() );
#endif

	// Compute a composite bounding box surrounding the initial + new positions..
	Vector vecCompositeMins = WorldAlignMins() + vecOldOrigin;
	Vector vecCompositeMaxs = WorldAlignMaxs() + vecOldOrigin;

	ComputeBoundingBox();

	// Sweep the shield through the world + touch things it hits...
	SweepContext_t ctx( this, GetCollisionGroup() );
	VectorSubtract( GetAbsOrigin(), vecOldOrigin, ctx.m_vecStartDelta );

	if (ctx.m_vecStartDelta != vec3_origin)
	{
		// FIXME: Brutal hack; needed because IntersectRayWithTriangle misses stuff
		// especially with short rays; I'm not sure what to do about this.
		// This basically simulates a shield thickness of 15 units
		ctx.m_vecEndDelta = ctx.m_vecStartDelta;
		VectorNormalize( ctx.m_vecEndDelta );
		ctx.m_vecEndDelta *= -15.0f;

		Vector vecNewMins = WorldAlignMins() + GetAbsOrigin();
		Vector vecNewMaxs = WorldAlignMaxs() + GetAbsOrigin();
		VectorMin( vecCompositeMins, vecNewMins, vecCompositeMins );
		VectorMax( vecCompositeMaxs, vecNewMaxs, vecCompositeMaxs );

		m_pEnumCtx = &ctx;
		enginetrace->EnumerateEntities( vecCompositeMins, vecCompositeMaxs, this );
	}
}


//-----------------------------------------------------------------------------
// Update the shield position: 
//-----------------------------------------------------------------------------
void CShieldMobile::ShieldThink( void )
{
	SimulateShield();

#ifdef CLIENT_DLL
	m_ShieldEffect.ComputeControlPoints();
	m_ShieldEffect.ComputePanelActivity();
#endif

	SetNextThink( gpGlobals->curtime + 0.01f );
}

void CShieldMobile::ClientThink()
{
#ifdef CLIENT_DLL
	if ( GetPredictable() )
		return;

	SimulateShield();
	m_ShieldEffect.ComputeControlPoints();
	m_ShieldEffect.ComputePanelActivity();
#endif
}


//-----------------------------------------------------------------------------
// Teleport!
//-----------------------------------------------------------------------------
void CShieldMobile::OnTeleported(  )
{
	CBaseEntity *owner = GetOwnerEntity();
	if (!owner)
		return;

	m_ShieldEffect.SetCurrentAngles( owner->GetAbsAngles() );

	Vector origin;
	origin = owner->EyePosition();
	if ( m_flFrontDistance )
	{
		Vector vForward;
		AngleVectors( m_ShieldEffect.GetCurrentAngles(), &vForward );
		origin += vForward * m_flFrontDistance;
	}

	m_ShieldEffect.SetCurrentPosition( origin );
}



#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Get this after the data changes
//-----------------------------------------------------------------------------
void CShieldMobile::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	m_ShieldEffect.SetThetaPhi( m_flShieldTheta, m_flShieldPhi );
	m_ShieldEffect.SetAngularSpringConstant( m_flSpringConstant );
}


//-----------------------------------------------------------------------------
// A little pre-render processing
//-----------------------------------------------------------------------------
void C_ShieldMobile::PreRender( )
{
	if (m_ShieldState & SHIELD_MOBILE_EMP)
	{
		// Decay fade if we've been EMPed or if we're inactive
		if (m_FadeValue > 0.0f)
		{
			m_FadeValue -= gpGlobals->frametime / SHIELD_EMP_FADE_TIME;
			if (m_FadeValue < 0.0f)
			{
				m_FadeValue = 0.0f;

				// Reset the shield to un-wobbled state
				m_ShieldEffect.ComputeControlPoints();
			}
			else
			{
				Vector dir;
				AngleVectors( m_ShieldEffect.GetCurrentAngles(), & dir, 0, 0 );

				// Futz with the control points if we've been EMPed
				for (int i = 0; i < SHIELD_NUM_CONTROL_POINTS; ++i)
				{
					// Get the direction for the point
					float factor = -EMP_WAVE_AMPLITUDE * sin( i * M_PI * 0.5f + gpGlobals->curtime * M_PI / SHIELD_EMP_WOBBLE_TIME );
					m_ShieldEffect.GetPoint(i) += dir * factor;
				}
			}
		}
	}
	else
	{
		// Fade back in, no longer EMPed
		if (m_FadeValue < 1.0f)
		{
			m_FadeValue += gpGlobals->frametime / SHIELD_EMP_FADE_TIME;
			if (m_FadeValue >= 1.0f)
			{
				m_FadeValue = 1.0f;
			}
		}
	}
}

void CShieldMobile::AddEntity( )
{
	BaseClass::AddEntity( );
	PreRender();
}


//-----------------------------------------------------------------------------
// Bounds computation
//-----------------------------------------------------------------------------
void CShieldMobile::GetBounds( Vector& mins, Vector& maxs )
{
	m_ShieldEffect.ComputeBounds( mins, maxs );
}


//-----------------------------------------------------------------------------
// Gets at the control point data; who knows how it was made?
//-----------------------------------------------------------------------------
void CShieldMobile::GetShieldData( const Vector** ppVerts, float* pOpacity, float* pBlend )
{
	for ( int i = 0; i < SHIELD_NUM_CONTROL_POINTS; ++i )
	{
		ppVerts[i] = &m_ShieldEffect.GetControlPoint(i);

		if ( m_pVertsActive[i >> 3] & (1 << (i & 0x7)) )
		{
			pOpacity[i] = m_ShieldEffect.ComputeOpacity( *ppVerts[i], GetAbsOrigin() );
			pBlend[i] = 1.0f;
		}
		else
		{
			pOpacity[i] = 192.0f;
			pBlend[i] = 0.0f;
		}
	}
}


#endif // CLIENT_DLL


#ifndef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Create a mobile version of the shield
//-----------------------------------------------------------------------------
CShield *CreateMobileShield( CBaseEntity *owner, float flFrontDistance )
{
	CShieldMobile *pShield = (CShieldMobile*)CreateEntityByName("shield_mobile");

	pShield->SetOwnerEntity( owner );
	pShield->SetLocalAngles( owner->GetAbsAngles() );
	pShield->SetFrontDistance( flFrontDistance );

	// Start it in the right place
	Vector vForward;
	AngleVectors( owner->GetAbsAngles(), &vForward );
	Vector vecOrigin = owner->EyePosition() + (vForward * flFrontDistance);
	UTIL_SetOrigin( pShield, vecOrigin );

	pShield->ChangeTeam( owner->GetTeamNumber() );
	pShield->SetupRecharge( shield_mobile_power.GetFloat(), shield_mobile_recharge_delay.GetFloat(), shield_mobile_recharge_amount.GetFloat(), shield_mobile_recharge_time.GetFloat() );
	pShield->Spawn();

	return pShield;
}

#endif

