//========= Copyright © 1996-2010, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CFuncInstanceIoProxy : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncInstanceIoProxy, CBaseEntity );

	// Input handlers
	void InputProxyRelay1( inputdata_t &inputdata );
	void InputProxyRelay2( inputdata_t &inputdata );
	void InputProxyRelay3( inputdata_t &inputdata );
	void InputProxyRelay4( inputdata_t &inputdata );
	void InputProxyRelay5( inputdata_t &inputdata );
	void InputProxyRelay6( inputdata_t &inputdata );
	void InputProxyRelay7( inputdata_t &inputdata );
	void InputProxyRelay8( inputdata_t &inputdata );
	void InputProxyRelay9( inputdata_t &inputdata );
	void InputProxyRelay10( inputdata_t &inputdata );
	void InputProxyRelay11( inputdata_t &inputdata );
	void InputProxyRelay12( inputdata_t &inputdata );
	void InputProxyRelay13( inputdata_t &inputdata );
	void InputProxyRelay14( inputdata_t &inputdata );
	void InputProxyRelay15( inputdata_t &inputdata );
	void InputProxyRelay16( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:

	COutputEvent m_OnProxyRelay1;
	COutputEvent m_OnProxyRelay2;
	COutputEvent m_OnProxyRelay3;
	COutputEvent m_OnProxyRelay4;
	COutputEvent m_OnProxyRelay5;
	COutputEvent m_OnProxyRelay6;
	COutputEvent m_OnProxyRelay7;
	COutputEvent m_OnProxyRelay8;
	COutputEvent m_OnProxyRelay9;
	COutputEvent m_OnProxyRelay10;
	COutputEvent m_OnProxyRelay11;
	COutputEvent m_OnProxyRelay12;
	COutputEvent m_OnProxyRelay13;
	COutputEvent m_OnProxyRelay14;
	COutputEvent m_OnProxyRelay15;
	COutputEvent m_OnProxyRelay16;

};

LINK_ENTITY_TO_CLASS( func_instance_io_proxy, CFuncInstanceIoProxy );

BEGIN_DATADESC( CFuncInstanceIoProxy )

	// Inputs
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay1",  InputProxyRelay1 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay2",  InputProxyRelay2 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay3",  InputProxyRelay3 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay4",  InputProxyRelay4 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay5",  InputProxyRelay5 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay6",  InputProxyRelay6 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay7",  InputProxyRelay7 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay8",  InputProxyRelay8 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay9",  InputProxyRelay9 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay10",  InputProxyRelay10 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay11",  InputProxyRelay11 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay12",  InputProxyRelay12 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay13",  InputProxyRelay13 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay14",  InputProxyRelay14 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay15",  InputProxyRelay15 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay16",  InputProxyRelay16 ),

	// Outputs
	DEFINE_OUTPUT( m_OnProxyRelay1, "OnProxyRelay1" ),
	DEFINE_OUTPUT( m_OnProxyRelay2, "OnProxyRelay2" ),
	DEFINE_OUTPUT( m_OnProxyRelay3, "OnProxyRelay3" ),
	DEFINE_OUTPUT( m_OnProxyRelay4, "OnProxyRelay4" ),
	DEFINE_OUTPUT( m_OnProxyRelay5, "OnProxyRelay5" ),
	DEFINE_OUTPUT( m_OnProxyRelay6, "OnProxyRelay6" ),
	DEFINE_OUTPUT( m_OnProxyRelay7, "OnProxyRelay7" ),
	DEFINE_OUTPUT( m_OnProxyRelay8, "OnProxyRelay8" ),
	DEFINE_OUTPUT( m_OnProxyRelay9, "OnProxyRelay9" ),
	DEFINE_OUTPUT( m_OnProxyRelay10, "OnProxyRelay10" ),
	DEFINE_OUTPUT( m_OnProxyRelay11, "OnProxyRelay11" ),
	DEFINE_OUTPUT( m_OnProxyRelay12, "OnProxyRelay12" ),
	DEFINE_OUTPUT( m_OnProxyRelay13, "OnProxyRelay13" ),
	DEFINE_OUTPUT( m_OnProxyRelay14, "OnProxyRelay14" ),
	DEFINE_OUTPUT( m_OnProxyRelay15, "OnProxyRelay15" ),
	DEFINE_OUTPUT( m_OnProxyRelay16, "OnProxyRelay16" ),

END_DATADESC()

//------------------------------------------------------------------------------
// Purpose : Route the incomming to the outgoing proxy messages. 
//------------------------------------------------------------------------------
void CFuncInstanceIoProxy::InputProxyRelay1( inputdata_t &inputdata )
{
	m_OnProxyRelay1.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay2( inputdata_t &inputdata )
{
	m_OnProxyRelay2.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay3( inputdata_t &inputdata )
{
	m_OnProxyRelay3.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay4( inputdata_t &inputdata )
{
	m_OnProxyRelay4.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay5( inputdata_t &inputdata )
{
	m_OnProxyRelay5.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay6( inputdata_t &inputdata )
{
	m_OnProxyRelay6.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay7( inputdata_t &inputdata )
{
	m_OnProxyRelay7.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay8( inputdata_t &inputdata )
{
	m_OnProxyRelay8.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay9( inputdata_t &inputdata )
{
	m_OnProxyRelay9.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay10( inputdata_t &inputdata )
{
	m_OnProxyRelay10.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay11( inputdata_t &inputdata )
{
	m_OnProxyRelay11.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay12( inputdata_t &inputdata )
{
	m_OnProxyRelay12.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay13( inputdata_t &inputdata )
{
	m_OnProxyRelay13.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay14( inputdata_t &inputdata )
{
	m_OnProxyRelay14.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay15( inputdata_t &inputdata )
{
	m_OnProxyRelay15.FireOutput( this, this );
}

void CFuncInstanceIoProxy::InputProxyRelay16( inputdata_t &inputdata )
{
	m_OnProxyRelay16.FireOutput( this, this );
	DevWarning( "Maximun Proxy Messages used - ask a programmer for more.\n" );
}