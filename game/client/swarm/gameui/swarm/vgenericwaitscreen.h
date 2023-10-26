//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VGENERICWAITSCREEN_H__
#define __VGENERICWAITSCREEN_H__

#include "basemodui.h"
#include <utlvector.h>
#include <utlstring.h>

namespace BaseModUI {

class GenericWaitScreen : public CBaseModFrame, public IBaseModFrameListener
{
	DECLARE_CLASS_SIMPLE( GenericWaitScreen, CBaseModFrame );

public:
	GenericWaitScreen( vgui::Panel *parent, const char *panelName );
	~GenericWaitScreen();

	void			SetMaxDisplayTime( float flMaxTime, void (*pfn)() = NULL );
	void			AddMessageText( const char* message, float minDisplayTime );
	void			AddMessageText( const wchar_t* message, float minDisplayTime );
	void			SetCloseCallback( vgui::Panel * panel, const char * message );
	void			ClearData();
	void			OnThink();
	void			RunFrame();

	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void SetDataSettings( KeyValues *pSettings );

protected:
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	PaintBackground();

private:
	void			SetUpText();
	void			ClockAnim();
	void			CheckIfNeedsToClose();
	void			UpdateFooter();
	void			SetMessageText( const char* message );
	void			SetMessageText( const wchar_t *message );

	float			m_LastEngineTime;
	int				m_CurrentSpinnerValue;

	vgui::Panel		*m_callbackPanel;
	CUtlString		m_callbackMessage;

	CUtlString		m_currentDisplayText;
	bool			m_bTextSet;

	float			m_MsgStartDisplayTime;
	float			m_MsgMinDisplayTime;
	float			m_MsgMaxDisplayTime;
	void			(*m_pfnMaxTimeOut)();
	bool			m_bClose;

	struct WaitMessage
	{
		WaitMessage() : minDisplayTime( 0 ), mWchMsgText( NULL ) {}
		CUtlString	mDisplayString;
		float		minDisplayTime;
		wchar_t const *mWchMsgText;
	};
	CUtlVector< WaitMessage > m_MsgVector;

	IMatchAsyncOperation *m_pAsyncOperationAbortable;
};

}

#endif // __VSEARCHINGFORLIVEGAMES_H__