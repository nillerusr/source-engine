#include "cbase.h"
#include "nb_campaign_map.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Campaign_Map::CNB_Campaign_Map( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	// == MANAGED_MEMBER_CREATION_END ==
}

CNB_Campaign_Map::~CNB_Campaign_Map()
{

}

void CNB_Campaign_Map::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_campaign_map.res" );
}

void CNB_Campaign_Map::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Campaign_Map::OnThink()
{
	BaseClass::OnThink();
}

void CNB_Campaign_Map::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );
}
