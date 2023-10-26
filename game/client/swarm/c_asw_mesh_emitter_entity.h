#ifndef _DEFINED_C_ASW_MESH_EMITTER_ENTITY_H
#define _DEFINED_C_ASW_MESH_EMITTER_ENTITY_H

class CASW_VGUI_Edit_Emitter;
class CASWGenericEmitter;

class C_ASW_Mesh_Emitter : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_ASW_Mesh_Emitter, C_BaseAnimating );

	C_ASW_Mesh_Emitter();
	virtual void OnDataChanged( DataUpdateType_t updateType );	// Called when data changes on the server
	virtual void ClientThink( void );							// Client-side think function for the entity
	virtual void UseTemplate(const char* pTemplateName, bool bLoadFromCache);
	virtual void SaveAsTemplate(const char* pTemplateName);
	virtual void CreateEmitter(const Vector &force);
	virtual void SetDieTime(float fDieTime);
	virtual void Die();
	virtual void ClientAttach(C_BaseAnimating *pParent, const char *szAttach);	// clientside attach to a specific entity's attachment point

	virtual IClientModelRenderable*	GetClientModelRenderable();
	virtual int InternalDrawModel( int flags, const RenderableInstance_t &instance );
	bool PrepareToDraw();
	int DrawParticle(Vector &vecPos);	
	virtual void GetRenderBounds( Vector& theMins, Vector& theMaxs );

	void	SetFrozen( bool bFrozen = true )	{ m_bFrozen = bFrozen; }

public:
	bool	m_bEmit;	// Determines whether or not we should emit particles
	float   m_fScale;
	float	m_fDesiredScale;
	float	m_fScaleRate;
	float	m_fDieTime;

	// for effects on gibs (like ice, fire, etc)
	bool	m_bFrozen;

	CSmartPtr<CASWGenericEmitter>	m_hEmitter;			// Particle emitter for this entity
	char m_szTemplateName[MAX_PATH];

	CHandle<C_BaseAnimating> m_hClientAttach;
	char m_szAttach[64];

	friend class CASW_VGUI_Edit_Emitter;
};

#endif /* _DEFINED_C_ASW_MESH_EMITTER_ENTITY_H */