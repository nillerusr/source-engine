//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HL1_CLIENTMODE_H
#define HL1_CLIENTMODE_H
#ifdef _WIN32
#pragma once
#endif

#include "clientmode_shared.h"

class ClientModeHL1Normal : public ClientModeShared 
{
DECLARE_CLASS( ClientModeHL1Normal, ClientModeShared );

public:
					ClientModeHL1Normal();
	virtual			~ClientModeHL1Normal();

	virtual	void	InitViewport();

	virtual float	GetViewModelFOV( void );

	virtual int		GetDeathMessageStartHeight( void );
};


extern IClientMode *GetClientModeNormal();
extern ClientModeHL1Normal* GetClientModeHL1Normal();


#endif // HL1_CLIENTMODE_H