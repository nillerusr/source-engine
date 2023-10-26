//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#ifndef C_ENTITY_FLAME_H
#define C_ENTITY_FLAME_H

#include "c_baseentity.h"

//
// Entity flame, client-side implementation
//

class C_EntityFlame : public C_BaseEntity
{
public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_EntityFlame, C_BaseEntity );

	C_EntityFlame( void );
	virtual ~C_EntityFlame( void );

	virtual bool	Simulate( void );
	virtual void	UpdateOnRemove( void );
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );

	EHANDLE				m_hEntAttached;		// The entity that we are burning (attached to).

private:
	void	CreateEffect( void );
	void	StopEffect( void );

	CUtlReference<CNewParticleEffect> m_hEffect;
	EHANDLE				m_hOldAttached;
	bool				m_bCheapEffect;
};


#endif // C_ENTITY_FLAME_H