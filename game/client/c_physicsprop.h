//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef C_PHYSICSPROP_H
#define C_PHYSICSPROP_H
#ifdef _WIN32
#pragma once
#endif

#include "c_breakableprop.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_PhysicsProp : public C_BreakableProp
{
	typedef C_BreakableProp BaseClass;
public:
	DECLARE_CLIENTCLASS();

	// Inherited from IClientUnknown
public:
	virtual IClientModelRenderable*	GetClientModelRenderable();

	// Inherited from IClientModelRenderable
public:
	virtual bool GetRenderData( void *pData, ModelDataCategory_t nCategory );

	// Other public methods
public:
	C_PhysicsProp();
	virtual ~C_PhysicsProp();

	virtual void OnDataChanged( DataUpdateType_t type );
	virtual bool OnInternalDrawModel( ClientModelRenderInfo_t *pInfo );

protected:
	// Networked vars.
	bool m_bAwake;
	bool m_bAwakeLastTime;
	bool m_bCanUseStaticLighting;
};

#endif // C_PHYSICSPROP_H 
