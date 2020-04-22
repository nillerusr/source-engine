//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_BUTTON_H
#define DOD_BUTTON_H

#include "mouseoverpanelbutton.h"
#include "KeyValues.h"
#include <vgui/IScheme.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/EditablePanel.h>

// a button with the bottom right corner cut out

/*

|``````````````|
|   PRESS ME!  |
|_____________/

*/

class CDODButtonShape
{
public:
	CDODButtonShape()
	{
		m_iWhiteTexture = -1;
	}

	void DrawShapedBorder( int x, int y, int wide, int tall, Color fgcolor );
	void DrawShapedBackground( int x, int y, int wide, int tall, Color bgcolor );

protected:
	int	m_iWhiteTexture;
};

class CDODButton : public vgui::Button, public CDODButtonShape
{
private:
	DECLARE_CLASS_SIMPLE( CDODButton, vgui::Button );
	
public:
	CDODButton(vgui::Panel *parent ) :
					vgui::Button( parent, "DODButton", "" )
	{
	}

protected:
	virtual void PaintBackground();
	virtual void PaintBorder();
};

class CDODClassInfoPanel : public vgui::EditablePanel
{
private:
	DECLARE_CLASS_SIMPLE( CDODClassInfoPanel, vgui::EditablePanel );

public:
	CDODClassInfoPanel( vgui::Panel *parent, const char *panelName ) : vgui::EditablePanel( parent, panelName )
	{
	}

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual vgui::Panel *CreateControlByName( const char *controlName );
};


// Solid coloured progress bar with no border
class CDODProgressBar : public vgui::ProgressBar
{
private:
	DECLARE_CLASS_SIMPLE( CDODProgressBar, vgui::ProgressBar );
	
public:
	CDODProgressBar(vgui::Panel *parent) : vgui::ProgressBar( parent, "statBar" )
	{
	}

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
};

#endif //DOD_BUTTON_H