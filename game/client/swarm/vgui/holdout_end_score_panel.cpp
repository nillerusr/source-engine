#include "cbase.h"
#include "holdout_end_score_panel.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


Holdout_End_Score_Panel::Holdout_End_Score_Panel( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{	

}

Holdout_End_Score_Panel::~Holdout_End_Score_Panel()
{

}

void Holdout_End_Score_Panel::PerformLayout()
{
	BaseClass::PerformLayout();
}

void Holdout_End_Score_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/HoldoutEndScorePanel.res" );
}