//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "DemoPage.h"

#include "vgui/IVGui.h"
#include "vgui_controls/Controls.h"
#include <vgui/IScheme.h>
#include <vgui/IImage.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// An Image is an class that handles drawing of a tga image
// They are not panels.
//-----------------------------------------------------------------------------
class ImageDemo: public DemoPage
{
	typedef DemoPage BaseClass;

public:
	ImageDemo(Panel *parent, const char *name);
	~ImageDemo();

	virtual void ApplySchemeSettings(IScheme *pScheme);
	virtual void Paint();
	
private:
	IImage *m_pImage;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ImageDemo::ImageDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// now insert an image
	m_pImage = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ImageDemo::~ImageDemo()
{
}

//-----------------------------------------------------------------------------
// Scheme settings
//-----------------------------------------------------------------------------
void ImageDemo::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );

	// now insert an image
	m_pImage = scheme()->GetImage("Resource/valve_logo", false);
}

//-----------------------------------------------------------------------------
// Purpose: Paint the image on screen. Images are not panels, 
// You must call the Paint method explicitly for them.
// and set thier position in the frame every time you draw them.
//-----------------------------------------------------------------------------
void ImageDemo::Paint()
{
	m_pImage->SetPos(100, 100);
	m_pImage->Paint();
}


Panel* ImageDemo_Create(Panel *parent)
{
	return new ImageDemo(parent, "ImageDemo");
}

