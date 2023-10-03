#ifndef _INCLUDED_HOLDOUT_RESUPPLY_FRAME_H
#define _INCLUDED_HOLDOUT_RESUPPLY_FRAME_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Frame.h>

namespace vgui
{
	class Label;
	class ImagePanel;
}
class LoadoutPanel;

// this panel allows you to choose new weapons for the next holdout wave

class Holdout_Resupply_Frame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( Holdout_Resupply_Frame, vgui::EditablePanel );
public:
	Holdout_Resupply_Frame( vgui::Panel *parent, const char *name );
	virtual ~Holdout_Resupply_Frame();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnThink();
	virtual void OnCommand( const char *command );

	static bool HasResupplyFrameOpen();

protected:
	vgui::EditablePanel *m_pBackground;
	vgui::Label *m_pTitleLabel;
	vgui::Label *m_pCountdownLabel;
	vgui::Label *m_pCountdownTimeLabel;
	vgui::Button *m_pOkayButton;
	LoadoutPanel *m_pLoadoutPanel;			// Note: this panel will call cl_loadout when player picks an item

	// TODO: some kind of 'OKAY DONE' button
};


#endif // _INCLUDED_HOLDOUT_RESUPPLY_FRAME_H
