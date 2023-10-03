//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_ENTITY_FREEZING_H
#define C_ENTITY_FREEZING_H

#include "cbase.h"


struct EntityFreezingHitboxBlobData_t
{
	CUtlVector<Vector> m_vPoints;
};


//-----------------------------------------------------------------------------
// Entity Dissolve, client-side implementation
//-----------------------------------------------------------------------------
class C_EntityFreezing : public C_BaseEntity
{
public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_EntityFreezing, C_BaseEntity );

	virtual void	GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual RenderableTranslucencyType_t ComputeTranslucencyType( );
	virtual int		DrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool	ShouldDraw() { return true; }
	virtual void	OnDataChanged( DataUpdateType_t updateType );

	void			ClientThink( void );

private:
	Vector	m_vFreezingOrigin;
	float	m_flFrozenPerHitbox[ 50 ];
	float	m_flFrozen;
	bool	m_bFinishFreezing;

	CUtlVector<EntityFreezingHitboxBlobData_t> m_HitboxBlobData;
};

#endif // C_ENTITY_FREEZING_H

