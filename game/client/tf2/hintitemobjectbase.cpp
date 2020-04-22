//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hintitemobjectbase.h"
#include <KeyValues.h>
#include "c_obj_resourcepump.h"
#include "c_func_resource.h"


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*panelName - 
//-----------------------------------------------------------------------------
CHintItemObjectBase::CHintItemObjectBase( vgui::Panel *parent, const char *panelName )
: BaseClass( parent, panelName )
{
	SetObjectType( "" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pKeyValues - 
//-----------------------------------------------------------------------------
void CHintItemObjectBase::ParseItem( KeyValues *pKeyValues )
{
	BaseClass::ParseItem( pKeyValues );
	
	const char *type = pKeyValues->GetString( "type", "" );
	if ( type )
	{
		SetObjectType( type );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *object - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHintItemObjectBase::IsObjectOfType( C_BaseEntity *object )
{
	if ( !stricmp( GetObjectType(), "Resource Zone" ) )
	{
		return dynamic_cast< C_ResourceZone *>( object ) ? true : false;
	}
	else if ( !stricmp( GetObjectType(), "Resource Pump" ) )
	{
		return dynamic_cast< C_ObjectResourcePump * >( object) ? true : false;
	}
	else if ( !stricmp( GetObjectType(), "BaseObject" ) )
	{
		return dynamic_cast< C_BaseObject * >( object) ? true : false;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *type - 
//-----------------------------------------------------------------------------
void CHintItemObjectBase::SetObjectType( const char *type )
{
	Q_strncpy( m_szObjectType, type, MAX_OBJECT_TYPE );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *CHintItemObjectBase::GetObjectType( void )
{
	return m_szObjectType;
}