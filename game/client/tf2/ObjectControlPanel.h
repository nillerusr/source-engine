//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Clients CBaseObject
//
// $NoKeywords: $
//=============================================================================//

#ifndef OBJECTCONTROLPANEL_H
#define OBJECTCONTROLPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "c_vguiscreen.h"

namespace vgui
{
	class Panel;
	class Label;
	class Button;
}

class C_BaseObject;
class CRotationSlider;
class C_BaseTFPlayer;

//-----------------------------------------------------------------------------
// Base class for all vgui screens on objects: 
//-----------------------------------------------------------------------------
class CObjectControlPanel : public CVGuiScreenPanel
{
	DECLARE_CLASS( CObjectControlPanel, CVGuiScreenPanel );

public:
	CObjectControlPanel( vgui::Panel *parent, const char *panelName );

	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnCommand( const char *command );
	virtual void OnTick();

protected:
	// Method to add controls to particular panels
	vgui::Panel *GetActivePanel() { return m_pActivePanel; }
	vgui::Panel *GetDeterioratingPanel() { return m_pDeterioratingPanel; }
	vgui::Panel *GetDismantlingPanel() { return m_pDismantlingPanel; }

	// Override these to deal with various controls in various modes
	virtual void OnTickDeteriorating( C_BaseObject *pObj, C_BaseTFPlayer *pLocalPlayer );
	virtual void OnTickActive( C_BaseObject *pObj, C_BaseTFPlayer *pLocalPlayer );
	virtual void OnTickDismantling( C_BaseObject *pObj, C_BaseTFPlayer *pLocalPlayer );

	C_BaseObject *GetOwningObject() const;

	// This should update the current panel and return that panel.
	virtual vgui::Panel* TickCurrentPanel();

	// The dismantle button has its own logic about whether or not to hide itself.
	// Use this to make it go away.
	void ShowDismantleButton( bool bShow );
	void ShowOwnerLabel( bool bShow );
	void ShowHealthLabel( bool bShow );

	// Send a message to the owner.
	void SendToServerObject( const char *pMsg );

private:
	// Operations performed through the controls 
	void AssumeControl();
	void Dismantle();
	void StartDismantling();
	void StopDismantling();
	bool IsDismantling() const;

	vgui::EditablePanel	*m_pActivePanel;
	vgui::EditablePanel	*m_pDeterioratingPanel;
	vgui::EditablePanel	*m_pDismantlingPanel;

	vgui::Label *m_pHealthLabel;
	vgui::Label *m_pOwnerLabel;
	vgui::Button *m_pDismantleButton;
	vgui::Button *m_pAssumeControlButton;
	vgui::Label *m_pDismantleTimeLabel;

	vgui::Panel *m_pCurrentPanel;

	bool	m_bDismantled;
	float	m_flDismantleTime;
};


// This is used for child panels. It forwards the messages to the parent panel.
class CCommandChainingPanel : public vgui::EditablePanel
{
	typedef vgui::EditablePanel BaseClass;

public:
	CCommandChainingPanel( vgui::Panel *parent, const char *panelName ) :
		BaseClass( parent, panelName ) 
	{
		SetPaintBackgroundEnabled( false );
	}

	void OnCommand( const char *command )
	{
		BaseClass::OnCommand( command );
		if (GetParent())
		{
			GetParent()->OnCommand(command);
		}
	}
};


//-----------------------------------------------------------------------------
// This is a panel for an object that has rotational controls 
//-----------------------------------------------------------------------------
class CRotatingObjectControlPanel : public CObjectControlPanel
{
	DECLARE_CLASS( CRotatingObjectControlPanel, CObjectControlPanel );

public:
	CRotatingObjectControlPanel( vgui::Panel *parent, const char *panelName );

	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );

protected:
	virtual void OnTickActive( C_BaseObject *pObj, C_BaseTFPlayer *pLocalPlayer );

private:
	CRotationSlider *m_pRotationSlider;
	vgui::Label *m_pRotationLabel;
};

#endif // OBJECTCONTROLPANEL_H
