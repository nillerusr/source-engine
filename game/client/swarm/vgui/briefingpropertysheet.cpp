#include "cbase.h"
#include "BriefingPropertySheet.h"
#include "BriefingTooltip.h"
#include "vgui_controls/Button.h"
#include "controller_focus.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

BriefingPropertySheet::BriefingPropertySheet(vgui::Panel *parent, const char *name) : vgui::PropertySheet(parent, name)
{	
	m_bPlayTabSounds = false;

	if (!g_hBriefingTooltip.Get())
	{
		g_hBriefingTooltip = new BriefingTooltip(this, "BriefingTooltip");
		g_hBriefingTooltip->SetTooltip(this, "INIT", "INIT", 0, 0);	// have to do this, as the tooltip "jumps" for one frame when it's first drawn (this makes the jump happen while the briefing is fading in)
	}
}

BriefingPropertySheet::~BriefingPropertySheet()
{
	if (g_hBriefingTooltip.Get())
	{
		g_hBriefingTooltip->SetVisible(false);
		g_hBriefingTooltip->MarkForDeletion();
		g_hBriefingTooltip = NULL;
	}
	if (GetControllerFocus())
	{
		for (int i=0;i<GetNumPages();i++)
		{
			vgui::Panel* b = GetTab(i);
			if (b)
			{
				GetControllerFocus()->RemoveFromFocusList(b);
			}
		}
	}
}

// this is our tabbed display, we let it take up the entire screen
void BriefingPropertySheet::PerformLayout()
{
	BaseClass::PerformLayout();

	int w = ScreenWidth();
	int h = ScreenHeight();
	SetSize(w, h);
	if (g_hBriefingTooltip.Get())
		g_hBriefingTooltip->InvalidateLayout(true);
}

void BriefingPropertySheet::AddPageCustomButton(Panel *page, const char *title, HScheme scheme)
{
	AddPage(page, title);

	SetActivePage(page);
	Button *b = static_cast<Button*>(GetActiveTab());
	if (b)
	{
		b->SetFont(vgui::scheme()->GetIScheme(scheme)->GetFont( "DefaultLarge", IsProportional() ) );
		//b->SetReleasedSound("swarm/interface/tabclick2.wav");

		if (GetControllerFocus())
			GetControllerFocus()->AddToFocusList(b);
	}
}

void BriefingPropertySheet::ChangeActiveTab( int index )
{
	if (m_bPlayTabSounds)
	{
		if (GetActiveTab() != NULL && GetActiveTab() != GetTab(index) && GetNumPages() > 3)
		{
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.TabClick2" );
		}
	}
	BaseClass::ChangeActiveTab(index);
}