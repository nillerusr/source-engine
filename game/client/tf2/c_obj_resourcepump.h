//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_OBJ_RESOURCEPUMP_H
#define C_OBJ_RESOURCEPUMP_H
#ifdef _WIN32
#pragma once
#endif

#include "CommanderOverlay.h"
#include "c_baseobject.h"

class C_ResourceZone;

class C_ObjectResourcePump : public C_BaseObject
{
	DECLARE_CLASS( C_ObjectResourcePump, C_BaseObject );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_ENTITY_PANEL();

	C_ObjectResourcePump();

	virtual void	SetDormant( bool bDormant );
	int				GetLevel( void ) { return m_iPumpLevel; }

	C_ResourceZone*	GetResourceZone() { return m_hResourceZone.Get(); }

private:
	CHealthBarPanel	*m_pResourceBar;
	int				m_iPumpLevel;
	CHandle<C_ResourceZone>	m_hResourceZone;

private:
	C_ObjectResourcePump( const C_ObjectResourcePump & ); // not defined, not accessible
};



#endif // C_OBJ_RESOURCEPUMP_H
