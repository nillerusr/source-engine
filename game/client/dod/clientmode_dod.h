//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_CLIENTMODE_H
#define DOD_CLIENTMODE_H
#ifdef _WIN32
#pragma once
#endif

#include "clientmode_shared.h"
#include "dodviewport.h"

class CDODFreezePanel;

class ClientModeDODNormal : public ClientModeShared 
{
DECLARE_CLASS( ClientModeDODNormal, ClientModeShared );

private:

// IClientMode overrides.
public:

					ClientModeDODNormal();

	virtual void	Init();
	virtual void	InitViewport();

	virtual float	GetViewModelFOV( void );

	int				GetDeathMessageStartHeight( void );

	virtual void	FireGameEvent( IGameEvent * event);
	virtual void	PostRenderVGui();

	virtual bool	ShouldDrawViewModel( void );

	virtual int		HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

	
private:
	
	//	void	UpdateSpectatorMode( void );

	void RadioMessage( const char *pszSoundName, const char *pszSubtitle, const char *pszSender = NULL, int iSenderIndex = 0 );
	char m_szLastRadioSound[128];

	CDODFreezePanel		*m_pFreezePanel;

};


extern IClientMode *GetClientModeNormal();
extern ClientModeDODNormal* GetClientModeDODNormal();


#endif // DOD_CLIENTMODE_H
