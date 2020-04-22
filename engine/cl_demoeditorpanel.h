//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CL_DEMOEDITORPANEL_H
#define CL_DEMOEDITORPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

namespace vgui
{
class Button;
class Label;
class ListPanel;
};

class CBaseDemoAction;
class CNewActionButton;
class CBaseActionEditDialog;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDemoEditorPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CDemoEditorPanel, vgui::Frame );

public:
	CDemoEditorPanel( vgui::Panel *parent );
	~CDemoEditorPanel();

	virtual void OnTick();

	// Command issued
	virtual void OnCommand(const char *command);

	void	OnVDMChanged( void );

	void		OnRefresh();

protected:

	bool		IsNewActionCommand( char const *command );

	void		CreateNewAction( char const *actiontype );

	void		OnEdit();
	void		OnDelete();
	void		OnSave();
	void		OnRevert();

	void		PurgeActionList();
	void		PopulateActionList();

	CBaseDemoAction *FindActionByName( char const *name );

	vgui::Label		*m_pCurrentDemo;
	vgui::Button	*m_pSave;
	vgui::Button	*m_pRevert;
	vgui::Button	*m_pOK;
	vgui::Button	*m_pCancel;

	CNewActionButton *m_pNew;
	vgui::Button	*m_pEdit;
	vgui::Button	*m_pDelete;

	vgui::ListPanel	*m_pActions;
	
	vgui::DHANDLE< CBaseActionEditDialog >	m_hCurrentEditor;

};

#endif // CL_DEMOEDITORPANEL_H
