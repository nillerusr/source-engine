//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CL_DEMOACTIONEDITORS_H
#define CL_DEMOACTIONEDITORS_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

class CDemoEditorPanel;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseActionEditDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CBaseActionEditDialog, vgui::Frame );

public:
	CBaseActionEditDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction );

	virtual void	Init( void );

	virtual void	OnClose();
	virtual void	OnCancel();

	virtual void	OnCommand( char const *commands );

	// Returns true if changes were effected
	virtual bool	OnSaveChanges( void );

private:
	vgui::Button	*m_pOK;
	vgui::Button	*m_pCancel;

	vgui::TextEntry	*m_pActionName;

	vgui::ComboBox	*m_pStartType;
	vgui::TextEntry	*m_pStart;

protected:
	CDemoEditorPanel	*m_pEditor;
	CBaseDemoAction		*m_pAction;
	bool				m_bNewAction;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseActionWithTargetDialog : public CBaseActionEditDialog
{
	DECLARE_CLASS_SIMPLE( CBaseActionWithTargetDialog, CBaseActionEditDialog );

public:
	CBaseActionWithTargetDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction );

	// Also a pure baseclass
	virtual void	Init( void );

	// Returns true if changes were effected
	virtual bool	OnSaveChanges( void );

private:

	vgui::TextEntry	*m_pActionTarget;
};


#endif // CL_DEMOACTIONEDITORS_H
