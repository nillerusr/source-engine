//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>
#include <vgui_controls/Controls.h>

#include <Keyvalues.h>
#include <vgui_controls/Frame.h>


using namespace vgui;


class FrameDemo: public DemoPage
{
	public:
		FrameDemo(Panel *parent, const char *name);
		~FrameDemo();

		void OnButtonClicked();

		void SetVisible(bool status);
	
	private:
		Frame *m_pFrame;

};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
FrameDemo::FrameDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Create a Frame.
	// This frame has no parent, this way if you press the minimize button
	// a label will go on the taskbar.
	m_pFrame = new Frame(NULL, "AFrame");

	// Frames are well, a "frame" around a panel. They have a name bar
	// at the top where a title can be displayed, sizing hotspots on the corners,
	// and a minimize and close box in the upper right corner.
	
	// Set the title of the frame
	m_pFrame->SetTitle("A Demo Frame", "");

	// Frames are sizable and moveable by default.

	// Set its position and size
	m_pFrame->SetSize(300, 100);

	// Set its Position
	m_pFrame->MoveToCenterOfScreen();

	// Start the frame off invisible. This way it will not be visible when
	// other demo tabs are selected. It becomes visible when this demo tab is selected.
	m_pFrame->SetVisible(false);

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
FrameDemo::~FrameDemo()
{
}

//-----------------------------------------------------------------------------
// Purpose: When we make this this demo page we make the frame visible.
//-----------------------------------------------------------------------------
void FrameDemo::SetVisible(bool status)
{
	if (status)
		m_pFrame->SetVisible(true);
	else
		m_pFrame->SetVisible(false);

	DemoPage::SetVisible(status);	
}


Panel* FrameDemo_Create(Panel *parent)
{
	return new FrameDemo(parent, "FrameDemo");
}


