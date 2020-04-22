//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	c_prop_energy_ball.cpp
// 
// Purpose: Portal version of the combine ball. This client code is needed to provide a different
//			look when the energy ball has infinite life and to have modified client effects.
//
//=====================================================================================//


#include "cbase.h"							// precompiled headers
#include "c_prop_combine_ball.h"			// Our parent class
#include "clienteffectprecachesystem.h"		// To precache our new material

ConVar cl_energy_ball_start_fade_time ( "cl_energy_ball_start_fade_time", "8", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: Portal version of a combine ball
//-----------------------------------------------------------------------------
class C_PropEnergyBall : public C_PropCombineBall
{
public:
	DECLARE_CLASS( C_PropEnergyBall, C_PropCombineBall );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_PropEnergyBall();

	virtual void	OnDataChanged( DataUpdateType_t updateType );

protected:
	bool InitMaterials();

	bool	m_bIsInfiniteLife;			// if this energy ball is an infinite life variety
	float	m_fTimeTillDeath;			// If this is a finite life energy ball, the time remaining until detonation
	float	m_fCurAlpha;				// The amount of alpha to apply at DrawModel, to simulate a decaying energy ball
};

LINK_ENTITY_TO_CLASS( prop_energy_ball, C_PropEnergyBall );

// precache our different materials for the infinite life energy balls
CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectEnergyBall )

	CLIENTEFFECT_MATERIAL( "effects/eball_infinite_life" )
	CLIENTEFFECT_MATERIAL( "effects/eball_finite_life" )

CLIENTEFFECT_REGISTER_END()


IMPLEMENT_CLIENTCLASS_DT( C_PropEnergyBall, DT_PropEnergyBall, CPropEnergyBall )

	RecvPropBool( RECVINFO( m_bIsInfiniteLife ) ),
	RecvPropFloat( RECVINFO( m_fTimeTillDeath ) ),

END_RECV_TABLE()


BEGIN_PREDICTION_DATA( C_PropEnergyBall )

	DEFINE_PRED_FIELD( m_bIsInfiniteLife, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_fTimeTillDeath, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PropEnergyBall::C_PropEnergyBall(): m_bIsInfiniteLife(false), m_fTimeTillDeath(-1), m_fCurAlpha ( 1.0f )
{
}

//-----------------------------------------------------------------------------
// Purpose: Flag our data as new this frame
// Input  : DataUpdateType_t, either created or updated
// Output : void
//-----------------------------------------------------------------------------
void C_PropEnergyBall::OnDataChanged(DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// If our data changed this frame, then operate based on it next think
	if ( updateType == DATA_UPDATE_DATATABLE_CHANGED )
	{
		float fStartFadeTime = cl_energy_ball_start_fade_time.GetFloat();

		if ( fStartFadeTime < 1.0f )
		{ 
			fStartFadeTime = 1.0f;
		}

		// The last x seconds of life, fade
		if ( (m_fTimeTillDeath > 0.0f) )
		{
			float fNewAlpha = m_fTimeTillDeath / fStartFadeTime;
			clamp( fNewAlpha, 0.0f, 1.0f );
			m_fCurAlpha = fNewAlpha;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Use our custom body materials for energy ball, but otherwise use the base class materials (base being C_PropCombineBall)
// Output : bool
//-----------------------------------------------------------------------------
bool C_PropEnergyBall::InitMaterials()
{
	// Use the same materials as a combine ball
	bool bRetVal = BaseClass::InitMaterials();

	// If we're an infinite life combine ball, swap out the body material (and the base implementation didnt fail)
	IMaterial* pBodyMat;
	if ( m_bIsInfiniteLife )
	{
		pBodyMat = materials->FindMaterial( "effects/eball_infinite_life", NULL, false );
	}
	else
	{
		pBodyMat = materials->FindMaterial( "effects/eball_finite_life", NULL, false );
	}

	// If we can find our custom material, use it.
	if ( pBodyMat == NULL )
	{
		bRetVal = false;
	}
	else
	{
		m_pBodyMaterial = pBodyMat;
		m_pBodyMaterial->AlphaModulate( m_fCurAlpha );
	}

	return bRetVal;
}