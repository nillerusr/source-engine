//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CPortalGameAccount object
//
// $NoKeywords: $
//=============================================================================//

#ifdef USE_GC_IN_PORTAL1
#include "gcsdk/schemasharedobject.h"

//---------------------------------------------------------------------------------
// Purpose: All the account-level information that the GC tracks for Portal
//---------------------------------------------------------------------------------
class CPortalGameAccountClient : public GCSDK::CSchemaSharedObject< CSchGameAccountClient >
{
public:
	CPortalGameAccountClient() : GCSDK::CSchemaSharedObject< CSchGameAccountClient >() {}
	CPortalGameAccountClient( uint32 unAccountID ) : GCSDK::CSchemaSharedObject< CSchGameAccountClient >()
	{
		Obj().m_unAccountID = unAccountID;
	}

	
};
#endif //#ifdef USE_GC_IN_PORTAL1