#include "cbase.h"
#include "asw_logo_panel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/AnimationController.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CASW_Logo_Panel::CASW_Logo_Panel(vgui::Panel *parent, const char *name) : vgui::ImagePanel(parent, name)
{
	SetImage( "../console/startup_loading" );
	SetShouldScaleImage(true);
}

CASW_Logo_Panel::~CASW_Logo_Panel()
{

}


void CASW_Logo_Panel::PerformLayout()
{
	// Get the screen size
	int wide, tall;
	vgui::surface()->GetScreenSize(wide, tall);

	float LogoWidth = wide * 0.8f;
	SetBounds( (wide - LogoWidth) * 0.5f,tall * 0.12f, LogoWidth, LogoWidth * 0.25f );
}