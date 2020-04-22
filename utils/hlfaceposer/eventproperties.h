//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef EVENTPROPERTIES_H
#define EVENTPROPERTIES_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialogparams.h"

class CChoreoScene;
class CChoreoEvent;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEventParams : public CBaseDialogParams
{
public:
	// e.g. CChoreoEvent::GESTURE
	int				m_nType;

	// Event descriptive name
	char			m_szName[ 256 ];

	// Expression name/wav name/gesture name/look at name
	char			m_szParameters[ 256 ];
	char			m_szParameters2[ 256 ];
	char			m_szParameters3[ 256 ];

	CChoreoScene	*m_pScene;

	float			m_flStartTime;
	float			m_flEndTime;
	bool			m_bHasEndTime;

	CChoreoEvent	*m_pEvent;

	bool			m_bDisabled;
	bool			m_bFixedLength;

	bool			m_bResumeCondition;

	bool			m_bLockBodyFacing;
	float			m_flDistanceToTarget;

	bool			m_bForceShortMovement;

	bool			m_bSyncToFollowingGesture;

	bool			m_bPlayOverScript;

	bool			m_bUsesTag;
	char			m_szTagName[ 256 ];
	char			m_szTagWav[ 256 ];

	// For Lookat events
	int				pitch;
	int				yaw;
	bool			usepitchyaw;

	// For speak
	bool			m_bCloseCaptionNoAttenuate;

};

int EventProperties( CEventParams *params );

class CBaseEventPropertiesDialog
{
public:
	virtual void		InitDialog( HWND hwndDlg ) = 0;
	virtual BOOL		HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam ) = 0;
	virtual void		SetTitle() = 0;

	HWND		GetControl( int id ) { return GetDlgItem( m_hDialog, id ); }

	virtual void ShowControlsForEventType( CEventParams *params );
	virtual void		InitControlData( CEventParams *params );

protected:
	virtual BOOL InternalHandleMessage( CEventParams *params, HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam, bool& handled );

	void		SetDialogTitle( CEventParams *params, char const *eventname, char const *desc );

	void		UpdateTagRadioButtons( CEventParams *params );
	void		PopulateTagList( CEventParams *params );
	void		ParseTags( CEventParams *params );

	void		PopulateNamedActorList( HWND wnd, CEventParams *params );

	void		GetSplineRect( HWND placeholder, RECT& rcOut );
	void		DrawSpline( HDC hdc, HWND placeholder, CChoreoEvent *e );

protected:

	HWND		m_hDialog;
};

#endif // EVENTPROPERTIES_H
