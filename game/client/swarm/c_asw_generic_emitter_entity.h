#ifndef _DEFINED_C_ASW_GENERIC_EMITTER_ENTITY_H
#define _DEFINED_C_ASW_GENERIC_EMITTER_ENTITY_H

class CASW_VGUI_Edit_Emitter;
class CASWGenericEmitter;

class C_ASW_Emitter : public C_BaseEntity
{
public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_ASW_Emitter, C_BaseEntity );

	C_ASW_Emitter();
	virtual void OnDataChanged( DataUpdateType_t updateType );	// Called when data changes on the server
	virtual void ClientThink( void );							// Client-side think function for the entity
	virtual void UseTemplate(const char* pTemplateName, bool bLoadFromCache);
	virtual void SaveAsTemplate(const char* pTemplateName);
	virtual void CreateEmitter();
	virtual void SetDieTime(float fDieTime);
	virtual void Die();
	virtual void ClientAttach(C_BaseAnimating *pParent, const char *szAttach);	// clientside attach to a specific entity's attachment point

public:
	bool	m_bEmit;	// Determines whether or not we should emit particles
	float   m_fScale;
	float	m_fDesiredScale;
	float	m_fScaleRate;
	float	m_fDieTime;
	
	CSmartPtr<CASWGenericEmitter>	m_hEmitter;			// Particle emitter for this entity
	char m_szTemplateName[MAX_PATH];

	CHandle<C_BaseAnimating> m_hClientAttach;
	char m_szAttach[64];

	friend class CASW_VGUI_Edit_Emitter;
};

#endif /* _DEFINED_C_ASW_GENERIC_EMITTER_ENTITY_H */