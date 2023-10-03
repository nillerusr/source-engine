//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "baseanimating.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


class CProp_Hallucination : public CBaseAnimating
{
public:
	DECLARE_CLASS( CProp_Hallucination, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CProp_Hallucination( void );
	virtual void Precache( void );
	virtual void Spawn( void );
	virtual int DrawDebugTextOverlays( void );
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );
	void InputSetVisibleTime( inputdata_t &inputdata );
	void InputSetRechargeTime( inputdata_t &inputdata );

	CNetworkVar( bool, m_bEnabled );
	float m_fStartEnabledChance; //0.0 - 100.0% chance that this hallucination will start enabled
	CNetworkVar( float, m_fVisibleTime ); //how long in seconds this hallucination can remain on screen from first sighting
	CNetworkVar( float, m_fRechargeTime ); //how long in seconds it takes the hallucination to recharge before becoming visible again. 0 to disable
};

BEGIN_DATADESC( CProp_Hallucination )
	DEFINE_FIELD( m_bEnabled, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_fStartEnabledChance, FIELD_FLOAT, "EnabledChance" ),
	DEFINE_KEYFIELD( m_fVisibleTime, FIELD_FLOAT, "VisibleTime" ),
	DEFINE_KEYFIELD( m_fRechargeTime, FIELD_FLOAT, "RechargeTime" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetVisibleTime", InputSetVisibleTime ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetRechargeTime", InputSetRechargeTime ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CProp_Hallucination, DT_Prop_Hallucination )
	SendPropBool( SENDINFO(m_bEnabled) ),
	SendPropFloat( SENDINFO(m_fVisibleTime) ),
	SendPropFloat( SENDINFO(m_fRechargeTime) ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( prop_hallucination, CProp_Hallucination );


CProp_Hallucination::CProp_Hallucination( void )
{
}

void CProp_Hallucination::Precache( void )
{
	BaseClass::Precache();
	PrecacheModel( m_ModelName.ToCStr() );
}

void CProp_Hallucination::Spawn( void )
{
	Precache();
	BaseClass::Spawn();
	SetModel( m_ModelName.ToCStr() );

	if( m_fStartEnabledChance > 0.0f )
	{
		m_bEnabled = RandomFloat() <= (m_fStartEnabledChance * 0.01f);	
	}
}

void CProp_Hallucination::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

void CProp_Hallucination::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}

void CProp_Hallucination::InputSetVisibleTime( inputdata_t &inputdata )
{
	m_fVisibleTime = inputdata.value.Float();
}

void CProp_Hallucination::InputSetRechargeTime( inputdata_t &inputdata )
{
	m_fRechargeTime = inputdata.value.Float();
}


//-----------------------------------------------------------------------------
// Draw debug overlays
//-----------------------------------------------------------------------------
int CProp_Hallucination::DrawDebugTextOverlays()
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[255];

		Q_snprintf( tempstr, sizeof(tempstr), "%s", m_bEnabled ? "Enabled" : "Disabled" );
		EntityText( text_offset++, tempstr, 0 );

		Q_snprintf( tempstr, sizeof(tempstr), "Start Enabled Chance: %f%%", m_fStartEnabledChance );
		EntityText( text_offset++, tempstr, 0 );

		Q_snprintf( tempstr, sizeof(tempstr), "Visible Time: %fs", m_fVisibleTime.Get() );
		EntityText( text_offset++, tempstr, 0 );

		Q_snprintf( tempstr, sizeof(tempstr), "Recharge Time: %fs", m_fRechargeTime.Get() );
		EntityText( text_offset++, tempstr, 0 );
	}
	

	return text_offset;
}
