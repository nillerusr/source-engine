//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CTFCoachRating object
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_COACH_RATING_H
#define TF_COACH_RATING_H
#ifdef _WIN32
#pragma once
#endif

#ifdef GC
#include "gcsdk/schemasharedobject.h"

//---------------------------------------------------------------------------------
// Purpose: 
//---------------------------------------------------------------------------------
class CTFCoachRating : public GCSDK::CSchemaSharedObject< CSchCoachRating, k_EEconTypeCoachRating >
{
#ifdef GC_DLL
	DECLARE_CLASS_MEMPOOL( CTFCoachRating );
#endif

public:
	CTFCoachRating() {}
	CTFCoachRating( uint32 unAccountID ) 
	{
		Obj().m_unAccountIDCoach = unAccountID;
	}
};

#endif // GC

#endif // TF_COACH_RATING_H
