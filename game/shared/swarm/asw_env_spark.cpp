//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A point entity that periodically emits sparks and "bzzt" sounds.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "IEffects.h"
#include "engine/IEngineSound.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( CLIENT_DLL )
#define CASWEnvSpark C_ASWEnvSpark
#endif

const int SF_ASW_SPARK_START_ON			= 64;
const int SF_ASW_SPARK_SILENT			= 128;
const int SF_ASW_SPARK_ELECTRICAL		= 256;

class CASWEnvSpark : public CBaseEntity
{
	DECLARE_CLASS( CASWEnvSpark, CBaseEntity );
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

public:
	CASWEnvSpark( void );

#ifndef CLIENT_DLL
	virtual void	Spawn(void);
	virtual int		ShouldTransmit( const CCheckTransmitInfo *pInfo ) { return FL_EDICT_PVSCHECK; }
	virtual void	Precache(void);

	// Input handlers
	void InputStartSpark( inputdata_t &inputdata );
	void InputStopSpark( inputdata_t &inputdata );
	void InputToggleSpark( inputdata_t &inputdata );
	void InputSparkOnce( inputdata_t &inputdata );
	void InputSetMinDelay( inputdata_t &inputdata );
	void InputSetMaxDelay( inputdata_t &inputdata );
#else
	virtual void	ClientThink( void );
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	OnSetDormant( bool bDormant );
	void			CreateSpark( void );
#endif

	CNetworkVar( float, m_flMinDelay );
	CNetworkVar( float, m_flMaxDelay );
	CNetworkVar( float, m_flPercentCollide );
	CNetworkVar( float, m_flMagnitude );
	CNetworkVar( int, m_nDoSparkCount );

	CNetworkVar( bool, m_bPlaySound );
	CNetworkVar( bool, m_bEnabled );
	CNetworkVar( bool, m_bElectrical );
#if defined( GAME_DLL )
	COutputEvent	m_OnSpark;
#else
	int m_nPrevDoSparkCount;
	float m_flNextSpark;
#endif
	
};

IMPLEMENT_NETWORKCLASS_ALIASED( ASWEnvSpark, DT_ASWEnvSpark );

BEGIN_NETWORK_TABLE( CASWEnvSpark, DT_ASWEnvSpark )
#ifndef CLIENT_DLL

	SendPropFloat	(SENDINFO(m_flMinDelay),	0,	SPROP_NOSCALE ),	
	SendPropFloat	(SENDINFO(m_flMaxDelay),	0,	SPROP_NOSCALE ),
	SendPropFloat	(SENDINFO(m_flPercentCollide),	0,	SPROP_NOSCALE ),
	SendPropFloat	(SENDINFO(m_flMagnitude),	0,	SPROP_NOSCALE ),
	SendPropInt		(SENDINFO(m_nDoSparkCount),	3, SPROP_UNSIGNED ),

	SendPropBool	(SENDINFO(m_bPlaySound)),
	SendPropBool	(SENDINFO(m_bEnabled)),
	SendPropBool	(SENDINFO(m_bElectrical)),
	SendPropInt		(SENDINFO(m_clrRender),		32,	SPROP_UNSIGNED, SendProxy_Color32ToInt32 ),

#else
	RecvPropFloat	(RECVINFO(m_flMinDelay)),	
	RecvPropFloat	(RECVINFO(m_flMaxDelay)),
	RecvPropFloat	(RECVINFO(m_flPercentCollide)),
	RecvPropFloat	(RECVINFO(m_flMagnitude)),
	RecvPropInt		(RECVINFO(m_nDoSparkCount)),

	RecvPropBool	(RECVINFO(m_bPlaySound)),
	RecvPropBool	(RECVINFO(m_bEnabled)),
	RecvPropBool	(RECVINFO(m_bElectrical)),
	RecvPropInt		(RECVINFO(m_clrRender), 0, RecvProxy_Int32ToColor32 ),

#endif
 END_NETWORK_TABLE()


BEGIN_DATADESC( CASWEnvSpark )

	DEFINE_KEYFIELD( m_flMaxDelay, FIELD_FLOAT, "MinDelay" ),	
	DEFINE_KEYFIELD( m_flMaxDelay, FIELD_FLOAT, "MaxDelay" ),
	DEFINE_KEYFIELD( m_flPercentCollide, FIELD_FLOAT, "PercentCollide" ),
	DEFINE_KEYFIELD( m_flMagnitude, FIELD_FLOAT, "Magnitude" ),

#ifndef CLIENT_DLL
	DEFINE_INPUTFUNC( FIELD_VOID, "StartSpark", InputStartSpark ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StopSpark", InputStopSpark ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ToggleSpark", InputToggleSpark ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SparkOnce", InputSparkOnce ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetMinDelay", InputSetMaxDelay ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetMaxDelay", InputSetMaxDelay ),

	DEFINE_OUTPUT( m_OnSpark, "OnSpark" ),
#endif

END_DATADESC()



LINK_ENTITY_TO_CLASS(asw_env_spark, CASWEnvSpark);


//-----------------------------------------------------------------------------
// Purpose: CONSTRUCT THE SPARK (class)
//-----------------------------------------------------------------------------
CASWEnvSpark::CASWEnvSpark( void )
{
	m_flMinDelay = 1;
	m_flMaxDelay = 1;
	m_flMagnitude = 1;
	m_flPercentCollide = 0;
	m_nDoSparkCount = 0;

	m_bPlaySound = true;
	m_bEnabled = false;
	m_bElectrical = false;

#ifdef CLIENT_DLL
	m_nPrevDoSparkCount = 0;
	m_flNextSpark = 0;
#endif
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CASWEnvSpark::Spawn(void)
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	SetCollisionGroup( COLLISION_GROUP_NONE );
	SetThink( NULL );

	if ( FBitSet(m_spawnflags, SF_ASW_SPARK_START_ON) )
	{
		m_bEnabled = true;
	}

	if ( FBitSet(m_spawnflags, SF_ASW_SPARK_SILENT) )
	{
		m_bPlaySound = false;
	}

	if ( FBitSet(m_spawnflags, SF_ASW_SPARK_ELECTRICAL) )
	{
		m_bElectrical = true;
	}

	// Clamp the delay and percentage of aolliding particles
	m_flMinDelay = clamp( m_flMinDelay, 0, 100 );
	m_flMaxDelay = clamp( m_flMaxDelay, 0, 100 );
	m_flPercentCollide = clamp( m_flPercentCollide, 0, 100 );

	Precache( );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASWEnvSpark::Precache(void)
{
	PrecacheParticleSystem( "asw_env_sparks" );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for starting the sparks.
//-----------------------------------------------------------------------------
void CASWEnvSpark::InputStartSpark( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: Updates the min delay that a spark will shoot
//-----------------------------------------------------------------------------
void CASWEnvSpark::InputSetMinDelay( inputdata_t &inputdata )
{
	m_flMinDelay = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: Updates the max delay that a spark will shoot
//-----------------------------------------------------------------------------
void CASWEnvSpark::InputSetMaxDelay( inputdata_t &inputdata )
{
	m_flMaxDelay = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: Shoot one spark.
//-----------------------------------------------------------------------------
void CASWEnvSpark::InputSparkOnce( inputdata_t &inputdata )
{
	m_nDoSparkCount += 1;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for starting the sparks.
//-----------------------------------------------------------------------------
void CASWEnvSpark::InputStopSpark( inputdata_t &inputdata )
{
	m_bEnabled = false;
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for toggling the on/off state of the sparks.
//-----------------------------------------------------------------------------
void CASWEnvSpark::InputToggleSpark( inputdata_t &inputdata )
{
	if ( m_bEnabled == false )
	{
		InputStartSpark( inputdata );
	}
	else
	{
		InputStopSpark( inputdata );
	}
}

#else
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASWEnvSpark::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

void CASWEnvSpark::ClientThink( void )
{
	// if the server increased the number of sparks that we should spit out
	// go ahead and spit out those number of sparks
	if ( m_nDoSparkCount != m_nPrevDoSparkCount )
	{
		// we queue them up because we can get more than one sark call in a single network update
		int iSparks = m_nDoSparkCount - m_nPrevDoSparkCount;
		if ( m_nDoSparkCount < m_nPrevDoSparkCount )
			iSparks += 4;

		for ( int i = 0; i < iSparks; i++ )
		{
			CreateSpark();
		}

		m_nPrevDoSparkCount = m_nDoSparkCount;
	}

	if ( m_bEnabled )
	{
		if ( m_flNextSpark < gpGlobals->curtime )
		{
			CreateSpark();

			float flMin = m_flMinDelay;
			if ( m_flMaxDelay < m_flMinDelay )
				flMin = m_flMaxDelay;

			m_flNextSpark = gpGlobals->curtime + 0.05f + random->RandomFloat( flMin, m_flMaxDelay );
		}
	}
}

void CASWEnvSpark::OnSetDormant( bool bDormant )
{
	// we do this so that when the player comes back into pvs, 
	// we dont get a flood of updates that cause many sparks to come out all at once
	if ( !bDormant )
	{
		m_nPrevDoSparkCount = m_nDoSparkCount;
	}
}

void CASWEnvSpark::CreateSpark( void )
{
	CUtlReference<CNewParticleEffect> pEffect = CNewParticleEffect::CreateOrAggregate( NULL, "asw_env_sparks", GetAbsOrigin(), NULL );
	if ( pEffect )
	{
		pEffect->SetControlPoint( 0, GetAbsOrigin() );
		pEffect->SetControlPointOrientation( 0, Forward(), -Left(), Up() );
		pEffect->SetControlPoint( 2, Vector( GetRenderColorR(), GetRenderColorG(), GetRenderColorB() ) );
		float flMagnitude = m_flMagnitude/100;
		float flElecReduction = 1.0f;
		float flCollide = (m_flPercentCollide/100);
		float flAmtElectrical = 0;
		if ( m_bElectrical )
		{
			flAmtElectrical = flMagnitude;
			flElecReduction = 0.6;
		}

		pEffect->SetControlPoint( 3, Vector( ((1.0f-flCollide)* flMagnitude)*flElecReduction, (flCollide*flMagnitude)*flElecReduction, flMagnitude ) );
		pEffect->SetControlPoint( 4, Vector( flAmtElectrical, 0, 0 ) );
		//Msg( "Spark - Magnitude = %f\n", flMagnitude );
	}

	if ( m_bPlaySound )
	{
		EmitSound( "DoSpark" );
	}
}

#endif