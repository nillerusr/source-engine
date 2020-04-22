//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>

#include <vgui_controls/Controls.h>
#include <vgui/IScheme.h>
#include <vgui_controls/AnimatingImagePanel.h>
#include <vgui/IImage.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// An AnimatingImagePanel is a panel class that handles drawing of Animated Images and gives
// them all kinds of panel features.
//-----------------------------------------------------------------------------
class AnimatingImagePanelDemo: public DemoPage
{
	public:
		AnimatingImagePanelDemo(Panel *parent, const char *name);
		~AnimatingImagePanelDemo();
		
	private:
		AnimatingImagePanel *m_pAnimImagePanel;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
AnimatingImagePanelDemo::AnimatingImagePanelDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Create an Animating Image Panel
	m_pAnimImagePanel = new AnimatingImagePanel(this, "AnAnimatingImagePanel");

	// Each image file is named c1, c2, c3... c20, one image for each frame of the animation.
	// This loads the 20 images in to the Animation class.
	m_pAnimImagePanel->LoadAnimation("resource\\steam\\c", 20);

	// Set the position
	m_pAnimImagePanel->SetPos(100, 100);

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
AnimatingImagePanelDemo::~AnimatingImagePanelDemo()
{
}



Panel* AnimatingImagePanelDemo_Create(Panel *parent)
{
	return new AnimatingImagePanelDemo(parent, "AnimatingImagePanelDemo");
}

