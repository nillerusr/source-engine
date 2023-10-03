#ifndef _INCLUDED_ASW_DYNAMIC_LIGHT_H
#define _INCLUDED_ASW_DYNAMIC_LIGHT_H

class CASW_Dynamic_Light : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Dynamic_Light, CBaseEntity );

	void Spawn( void );
	void DynamicLightThink( void );
	bool KeyValue( const char *szKeyName, const char *szValue );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	// Turn on and off the light
	void InputTurnOn( inputdata_t &inputdata );
	void InputTurnOff( inputdata_t &inputdata );
	void InputToggle( inputdata_t &inputdata );

	void SetLightRadius(float fRadius) { m_Radius = fRadius; }
	void SetExponent(int iExponent) { m_Exponent = iExponent; }

public:
	color32 m_LightColor;

	unsigned char m_ActualFlags;
	CNetworkVar( unsigned char, m_Flags );
	CNetworkVar( unsigned char, m_LightStyle );
	bool	m_On;
	CNetworkVar( float, m_Radius );
	CNetworkVar( int, m_Exponent );
	CNetworkVar( float, m_InnerAngle );
	CNetworkVar( float, m_OuterAngle );
	CNetworkVar( float, m_SpotRadius );
};

#endif // _INCLUDED_ASW_DYNAMIC_LIGHT_H