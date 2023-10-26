#include "cbase.h"
#include "vgui\PlayerListContainer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

PlayerListContainer::PlayerListContainer(vgui::Panel *parent, const char *name) :
	vgui::Panel(parent, name)
{

}

PlayerListContainer::~PlayerListContainer()
{
	
}

void PlayerListContainer::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundEnabled(false);
	SetBgColor(Color(65,74,96,128));
}

void PlayerListContainer::PerformLayout()
{	
	BaseClass::PerformLayout();

	//SetPos( ScreenWidth() * 0.15f, ScreenHeight() * 0.07f);
	//SetSize( ScreenWidth() * 0.7f, ScreenHeight() * 0.86f );
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
}

// ================================== Frame version ==================================================

PlayerListContainerFrame::PlayerListContainerFrame(Panel *parent, const char *panelName, bool showTaskbarIcon) :
	vgui::Frame(parent, panelName, showTaskbarIcon)
{
	SetMoveable(false);
	SetSizeable(false);
	SetMenuButtonVisible(false);
	SetMaximizeButtonVisible(false);
	SetMinimizeToSysTrayButtonVisible(false);
	SetCloseButtonVisible(true);
	SetTitleBarVisible(false);
}

PlayerListContainerFrame::~PlayerListContainerFrame()
{
}

void PlayerListContainerFrame::PerformLayout()
{
	BaseClass::PerformLayout();

	//SetPos( ScreenWidth() * 0.15f, ScreenHeight() * 0.1f);
	//SetSize( ScreenWidth() * 0.7f, ScreenHeight() * 0.8f );
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
}

void PlayerListContainerFrame::ApplySchemeSettings(vgui::IScheme *scheme)
{
	BaseClass::ApplySchemeSettings(scheme);

	SetPaintBackgroundEnabled(false);
	SetBgColor(Color(65,74,96,128));
}