//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CPortalGameAccount object
//
// $NoKeywords: $
//=============================================================================//

#include "gcsdk/schemasharedobject.h"

//---------------------------------------------------------------------------------
// Purpose: All the account-level information that the GC tracks for TF
//---------------------------------------------------------------------------------
class CPortalGameAccount : public GCSDK::CSchemaSharedObject< CSchGameAccount >
{
public:
	CPortalGameAccount() : GCSDK::CSchemaSharedObject< CSchGameAccount >() {}
	CPortalGameAccount( uint32 unAccountID ) : GCSDK::CSchemaSharedObject< CSchGameAccount >()
	{
		Obj().m_unAccountID = unAccountID;
	}
};
