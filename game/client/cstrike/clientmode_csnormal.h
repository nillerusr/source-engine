//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CS_CLIENTMODE_H
#define CS_CLIENTMODE_H
#ifdef _WIN32
#pragma once
#endif

#include "clientmode_shared.h"
#include "counterstrikeviewport.h"

class ClientModeCSNormal : public ClientModeShared 
{
DECLARE_CLASS( ClientModeCSNormal, ClientModeShared );

private:

// IClientMode overrides.
public:

					ClientModeCSNormal();

	virtual void	Init();
	virtual void	InitViewport();
	virtual void	Update();

	virtual int		KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

	virtual float	GetViewModelFOV( void );

	int				GetDeathMessageStartHeight( void );

	virtual void	FireGameEvent( IGameEvent *event );
	virtual void	PostRenderVGui();

	virtual bool	ShouldDrawViewModel( void );

	virtual bool	CanRecordDemo( char *errorMsg, int length ) const;

	//=============================================================================
	// HPE_BEGIN:
	// [menglish] Save server information shown to the client in a persistent place
	//=============================================================================
	 
	virtual wchar_t* GetServerName() { return m_pServerName; }
	virtual void SetServerName(wchar_t* name);
	virtual wchar_t* GetMapName() { return m_pMapName; }
	virtual void SetMapName(wchar_t* name);

private:
	wchar_t			m_pServerName[256];
	wchar_t			m_pMapName[256];

	//=============================================================================
	// HPE_END
	//=============================================================================
};


extern IClientMode *GetClientModeNormal();
extern ClientModeCSNormal* GetClientModeCSNormal();


#endif // CS_CLIENTMODE_H
