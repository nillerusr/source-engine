#ifndef _INCLUDED_C_ASW_CLIENT_CORPSE_H
#define _INCLUDED_C_ASW_CLIENT_CORPSE_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseanimating.h"

// a clientside ragdoll (to save on server cpu + bandwidth)

class C_ASW_Client_Corpse : public C_BaseAnimating
{
	DECLARE_CLASS( C_ASW_Client_Corpse, C_BaseAnimating );
public:
	DECLARE_CLIENTCLASS();

	C_ASW_Client_Corpse();

	virtual bool					AddRagdollToFadeQueue( void ) { return false; }
	virtual C_BaseAnimating*		BecomeRagdollOnClient( void );
	virtual C_ClientRagdoll*		CreateClientRagdoll( bool bRestoring = false );
};

#endif /* _INCLUDED_C_ASW_CLIENT_CORPSE_H */