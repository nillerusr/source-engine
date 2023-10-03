#ifndef _INCLUDED_ASW_GENERIC_EMITTER_ENTITY_H
#define _INCLUDED_ASW_GENERIC_EMITTER_ENTITY_H

// Declare the sparkler entity for the server-side
class CASW_Emitter : public CBaseEntity
{
public:
	DECLARE_SERVERCLASS();
	DECLARE_CLASS( CASW_Emitter, CBaseEntity );

	CASW_Emitter();

	void Spawn( void );
	void OnRestore();
	void SetScale(float fScale, float fScaleRate=1.0f) { m_fDesiredScale = fScale; m_fScaleRate = fScaleRate; }
	float GetScale() { return m_fScale; }
	void UseTemplate(const char* szTemplateName);
	void InputTurnOn( inputdata_t &input );	// Input function for toggling our effect's state
	void InputTurnOff( inputdata_t &input );	// Input function for toggling our effect's state
	//void InputSupplyParticles( inputdata_t &input );	// Input function for toggling our effect's state
	//void InputReset( inputdata_t &input );
	void InputToggle( inputdata_t &input );	// Input function for toggling our effect's state

private:
	CNetworkVar( bool, m_bEmit );		// Marks whether the effect should be active or not	
	CNetworkString( m_szTemplateName, MAX_PATH );
	//CNetworkVar( float, m_fScale );
	float m_fScale;
	CNetworkVar( float, m_fDesiredScale );
	CNetworkVar( float, m_fScaleRate );		
	bool m_bSetTemplateName;
	string_t m_szTemplate;	

	DECLARE_DATADESC();
};

#endif /* _INCLUDED_ASW_GENERIC_EMITTER_ENTITY_H */