//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HINTITEMOBJECTBASE_H
#define HINTITEMOBJECTBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "hintitemorderbase.h"

class C_BaseEntity;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHintItemObjectBase : public CHintItemOrderBase
{
	DECLARE_CLASS( CHintItemObjectBase, CHintItemOrderBase );
	
public:
	CHintItemObjectBase( vgui::Panel *parent, const char *panelName );
	
	virtual void		ParseItem( KeyValues *pKeyValues );
	// Is the object of type m_szObjectType
	virtual bool		IsObjectOfType( C_BaseEntity *object );
	virtual void		SetObjectType( const char *type );
	virtual char const	*GetObjectType( void );
	
private:
	enum
	{
		MAX_OBJECT_TYPE = 128,
	};
	
	char				m_szObjectType[ MAX_OBJECT_TYPE ];
};

#endif // HINTITEMOBJECTBASE_H
