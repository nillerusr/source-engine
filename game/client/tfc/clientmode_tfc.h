//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef TFC_CLIENTMODE_H
#define TFC_CLIENTMODE_H
#ifdef _WIN32
#pragma once
#endif

#include "clientmode_shared.h"
#include "tfcviewport.h"

class ClientModeTFCNormal : public ClientModeShared 
{
DECLARE_CLASS( ClientModeTFCNormal, ClientModeShared );

private:

// IClientMode overrides.
public:

					ClientModeTFCNormal();
	virtual			~ClientModeTFCNormal();

	virtual void	InitViewport();

	virtual int		KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

	virtual float	GetViewModelFOV( void );

	int				GetDeathMessageStartHeight( void );

	virtual void	FireGameEvent( KeyValues * event);
	virtual void	PostRenderVGui();

	
private:
	
	//	void	UpdateSpectatorMode( void );

};


extern IClientMode *GetClientModeNormal();
extern ClientModeTFCNormal* GetClientModeTFCNormal();


#endif // TFC_CLIENTMODE_H
