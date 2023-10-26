//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef C_TE_EFFECT_DISPATCH_H
#define C_TE_EFFECT_DISPATCH_H
#ifdef _WIN32
#pragma once
#endif

#include "effect_dispatch_data.h"
#include "precache_register.h"

typedef void (*ClientEffectCallback)( const CEffectData &data );


class CClientEffectRegistration
{
public:
	CClientEffectRegistration( const char *pEffectName, ClientEffectCallback fn );

public:
	const char *m_pEffectName;
	ClientEffectCallback m_pFunction;
	CClientEffectRegistration *m_pNext;

	static CClientEffectRegistration *s_pHead;
};


//
// Use this macro to register a client effect callback. 
// If you do DECLARE_CLIENT_EFFECT( MyEffectName, MyCallback ), then MyCallback will be 
// called when the server does DispatchEffect( "MyEffectName", data )
//
#define DECLARE_CLIENT_EFFECT_INTERNAL( effectName, callbackFunction ) \
	static CClientEffectRegistration ClientEffectReg_##callbackFunction( #effectName, callbackFunction );

#define DECLARE_CLIENT_EFFECT( effectName, callbackFunction )		\
	DECLARE_CLIENT_EFFECT_INTERNAL( effectName, callbackFunction )	\
	PRECACHE_REGISTER_BEGIN( DISPATCH_EFFECT, effectName )			\
	PRECACHE_REGISTER_END()

#define DECLARE_CLIENT_EFFECT_BEGIN( effectName, callbackFunction ) \
	DECLARE_CLIENT_EFFECT_INTERNAL( effectName, callbackFunction )	\
	PRECACHE_REGISTER_BEGIN( DISPATCH_EFFECT, effectName )

#define DECLARE_CLIENT_EFFECT_END()	PRECACHE_REGISTER_END()

void DispatchEffectToCallback( const char *pEffectName, const CEffectData &m_EffectData );
void DispatchEffect( const char *pName, const CEffectData &data );
void DispatchEffect( IRecipientFilter& filter, float flDelay, const char *pName, const CEffectData &data );
void DispatchEffect( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );

#endif // C_TE_EFFECT_DISPATCH_H
