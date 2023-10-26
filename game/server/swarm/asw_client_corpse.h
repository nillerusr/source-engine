#ifndef _INCLUDED_ASW_CLIENT_CORPSE_H
#define _INCLUDED_ASW_CLIENT_CORPSE_H
#ifdef _WIN32
#pragma once
#endif

#include "gib.h"

class CASW_Client_Corpse : public CRagGib
{
public:
	DECLARE_CLASS( CASW_Client_Corpse, CRagGib );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn();
	virtual void Precache();
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo );
	virtual bool BecomeRagdollOnClient( const Vector &force );

	string_t m_szRagdollSequence;
};

#endif /* _INCLUDED_ASW_CLIENT_CORPSE_H */