#include "cbase.h"
#include "nb_select_save_panel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Select_Save_Panel::CNB_Select_Save_Panel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	// == MANAGED_MEMBER_CREATION_END ==
}

CNB_Select_Save_Panel::~CNB_Select_Save_Panel()
{

}

void CNB_Select_Save_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_select_save_panel.res" );
}

void CNB_Select_Save_Panel::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Select_Save_Panel::OnThink()
{
	BaseClass::OnThink();
}
