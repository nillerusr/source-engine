//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: All matchmaking dialogs inherit from this
//
//=============================================================================//

#ifndef BASEDIALOG_H
#define BASEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "dialogmenu.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/KeyRepeat.h"
#include "KeyValues.h"
#include "BasePanel.h"

#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif

class CFooterPanel;

//-----------------------------------------------------------------------------
// Purpose: A Label with an extra string to hold a session property lookup key
//-----------------------------------------------------------------------------
class CPropertyLabel : public vgui::Label
{
	DECLARE_CLASS_SIMPLE( CPropertyLabel, vgui::Label );

public:
	CPropertyLabel( Panel *parent, const char *panelName, const char *text ) : BaseClass( parent, panelName, text ) 
	{
	}

	virtual void ApplySettings( KeyValues *pResourceData )
	{
		BaseClass::ApplySettings( pResourceData );

		m_szPropertyString[0] = 0;
		const char *pString = pResourceData->GetString( "PropertyString", NULL );
		if ( pString )
		{
			Q_strncpy( m_szPropertyString, pString, sizeof( m_szPropertyString ) );
		}
	}

	char m_szPropertyString[ MAX_PATH ];
};

//--------------------------------
// CBaseDialog
//--------------------------------
class CBaseDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CBaseDialog, vgui::Frame ); 

public:
	CBaseDialog( vgui::Panel *parent, const char *pName );
	~CBaseDialog();

	// IPanel interface
	virtual void		ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void		ApplySettings( KeyValues *pResourceData );
	virtual void		PerformLayout();
	virtual void		Activate();

	virtual void		OnKeyCodePressed( vgui::KeyCode code );
	virtual void		OnKeyCodeReleased( vgui::KeyCode code);
	virtual void		OnCommand( const char *pCommand );
	virtual void		OnClose();
	virtual void		OnThink();

	virtual void		OverrideMenuItem( KeyValues *pKeys );
	virtual void		SwapMenuItems( int iOne, int iTwo );

	virtual void		HandleKeyRepeated( vgui::KeyCode code );

protected:
	int				m_nBorderWidth;
	int				m_nMinWide;

	CDialogMenu		m_Menu;
	vgui::Label		*m_pTitle;
	vgui::Panel		*m_pParent;

	KeyValues		*m_pFooterInfo;
	int				m_nButtonGap;

	vgui::CKeyRepeatHandler	m_KeyRepeat;
};


//---------------------------------------------------------------------
// Helper object to display the map picture and descriptive text
//---------------------------------------------------------------------
class CScenarioInfoPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CScenarioInfoPanel, vgui::EditablePanel );

public:
	CScenarioInfoPanel( vgui::Panel *parent, const char *pName );
	~CScenarioInfoPanel();

	virtual void	PerformLayout();
	virtual void	ApplySettings( KeyValues *pResourceData );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );

	vgui::ImagePanel	*m_pMapImage;
	CPropertyLabel		*m_pTitle;
	CPropertyLabel		*m_pSubtitle;	
	CPropertyLabel		*m_pDescOne;	
	CPropertyLabel		*m_pDescTwo;	
	CPropertyLabel		*m_pDescThree;	
	CPropertyLabel		*m_pValueTwo;	
	CPropertyLabel		*m_pValueThree;	
};

#endif	// BASEDIALOG_H