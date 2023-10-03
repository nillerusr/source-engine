#ifndef _INCLUDED_C_ASW_SNOW_EMITTER_H
#define _INCLUDED_C_ASW_SNOW_EMITTER_H

#include "c_asw_generic_emitter.h"

class C_ASW_Marine;

class CASWSnowEmitter : public CASWGenericEmitter
{
	DECLARE_CLASS( CASWSnowEmitter, CASWGenericEmitter );	
public:
							CASWSnowEmitter( const char *pDebugName );
	static CSmartPtr<CASWSnowEmitter>	Create( const char *pDebugName );
	
	// creates a single particle	
	virtual ASWParticle*	AddASWParticle( PMaterialHandle hMaterial, const Vector &vOrigin, float flDieTime=3, unsigned char uchSize=10 );

	CHandle<C_ASW_Marine> m_hLastMarine;

private:
	CASWSnowEmitter( const CASWSnowEmitter & ); // not defined, not accessible

	friend class C_ASW_Emitter;
	friend class C_ASW_Mesh_Emitter;
	friend class CASW_VGUI_Edit_Emitter;
};


#endif /* _INCLUDED_C_ASW_SNOW_EMITTER_H */