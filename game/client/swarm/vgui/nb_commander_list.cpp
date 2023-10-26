#include "cbase.h"
#include "nb_commander_list.h"
#include "vgui_controls/Label.h"
#include "nb_commander_list_entry.h"
#include "c_playerresource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_debug_commander_list( "asw_debug_commander_list", "0", FCVAR_NONE, "Display fake names in commander list" );

CNB_Commander_List::CNB_Commander_List( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pBackground = new vgui::Panel( this, "Background" );
	m_pBackgroundInner = new vgui::Panel( this, "BackgroundInner" );
	m_pTitleBG = new vgui::Panel( this, "TitleBG" );
	m_pTitleBGBottom = new vgui::Panel( this, "TitleBGBottom" );
	m_pTitle = new vgui::Label( this, "Title", "" );
	// == MANAGED_MEMBER_CREATION_END ==
	for ( int i = 0; i < COMMANDER_LIST_MAX_COMMANDERS; i++ )
	{
		m_Entries[ i ] = new CNB_Commander_List_Entry( this, VarArgs( "Commander_List_Entry%d", i ) );
	}
}

CNB_Commander_List::~CNB_Commander_List()
{

}

void CNB_Commander_List::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_commander_list.res" );
}

void CNB_Commander_List::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Commander_List::OnThink()
{
	BaseClass::OnThink();

	int nSlot = 0;
	for ( int i = 1; i <= gpGlobals->maxClients && nSlot < COMMANDER_LIST_MAX_COMMANDERS; i++ )
	{
		if ( !g_PR->IsConnected( i ) )
			continue;

		m_Entries[ nSlot ]->SetClientIndex( i );
		nSlot++;
	}

	for ( int i = nSlot; i < COMMANDER_LIST_MAX_COMMANDERS; i++ )
	{
		m_Entries[ i ]->SetClientIndex( -1 );
	}

	if ( asw_debug_commander_list.GetBool() )
	{
		for ( int i = 0; i < 4; i++ )
		{
			m_Entries[ i ]->SetClientIndex( i + 1 );
		}
	}
}

void CNB_Commander_List::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );
}
