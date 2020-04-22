//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: TF's custom C_PlayerResource
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TFPLAYERRESOURCE_H
#define C_TFPLAYERRESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"
#include "c_playerresource.h"

class C_TFPlayerResource : public C_PlayerResource
{
	DECLARE_CLASS( C_TFPlayerResource, C_PlayerResource );
public:
	DECLARE_CLIENTCLASS();

					C_TFPlayerResource();
	virtual			~C_TFPlayerResource();

public:
};


#endif // C_TFPLAYERRESOURCE_H
