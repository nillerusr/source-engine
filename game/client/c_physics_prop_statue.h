//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef C_PHYSICS_PROP_STATUE_H
#define C_PHYSICS_PROP_STATUE_H

#ifdef _WIN32
#pragma once
#endif


#include "c_physicsprop.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_StatueProp : public C_PhysicsProp
{
public:
	DECLARE_CLASS( C_StatueProp, C_PhysicsProp );
	DECLARE_CLIENTCLASS();

	C_StatueProp();
	virtual			~C_StatueProp();

	virtual void Spawn( void );

	virtual void ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	virtual void	OnDataChanged( DataUpdateType_t updateType );

public:

	CHandle<CBaseAnimating>		m_hInitBaseAnimating;

	bool	m_bShatter;
	int		m_nShatterFlags;
	Vector	m_vShatterPosition;
	Vector	m_vShatterForce;
};


#endif // C_PHYSICS_PROP_STATUE_H