#include "cbase.h"
#include "nb_commander_list_entry.h"
#include "vgui_avatarimage.h"
#include <vgui/ILocalize.h>
#include <vgui_controls/Label.h>
#include "c_asw_player.h"
#include "c_playerresource.h"
#include <vgui/IVGUI.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Commander_List_Entry::CNB_Commander_List_Entry( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pCommanderName = new vgui::Label( this, "CommanderName", "" );
	m_pAvatar = new CAvatarImagePanel( this, "Avatar" );
	m_pAvatarBackground = new vgui::Panel( this, "AvatarBackground" );
	m_pCommanderLevel = new vgui::Label( this, "CommanderLevel", "" );
	// == MANAGED_MEMBER_CREATION_END ==

	m_nClientIndex = -1;
	m_nLastClientIndex = -1;
	vgui::ivgui()->AddTickSignal( GetVPanel() );
}

CNB_Commander_List_Entry::~CNB_Commander_List_Entry()
{

}

void CNB_Commander_List_Entry::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_commander_list_entry.res" );
}

void CNB_Commander_List_Entry::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( m_pAvatar )
	{
		int wide, tall;
		m_pAvatar->GetSize( wide, tall );
		if ( ((CAvatarImage*)m_pAvatar->GetImage()) )
		{
			((CAvatarImage*)m_pAvatar->GetImage())->SetAvatarSize( wide, tall );
			((CAvatarImage*)m_pAvatar->GetImage())->SetPos( -AVATAR_INDENT_X, -AVATAR_INDENT_Y );
		}
	}
}

void CNB_Commander_List_Entry::OnTick()
{
	BaseClass::OnTick();

	SetVisible( !( m_nClientIndex < 1 || m_nClientIndex > gpGlobals->maxClients || !g_PR->IsConnected( m_nClientIndex ) ) );
}

void CNB_Commander_List_Entry::OnThink()
{
	BaseClass::OnThink();

	if ( m_nClientIndex < 1 || m_nClientIndex > gpGlobals->maxClients || !g_PR->IsConnected( m_nClientIndex ) )
	{
		return;
	}

	if ( m_nLastClientIndex != m_nClientIndex )
	{
		m_nLastClientIndex = m_nClientIndex;

		m_pAvatar->SetPlayerByIndex( m_nClientIndex );

		int wide, tall;
		m_pAvatar->GetSize( wide, tall );
		((CAvatarImage*)m_pAvatar->GetImage())->SetAvatarSize( wide, tall );
		((CAvatarImage*)m_pAvatar->GetImage())->SetPos( -AVATAR_INDENT_X, -AVATAR_INDENT_Y );
	}

	C_ASW_Player *pPlayer = static_cast<C_ASW_Player*>( UTIL_PlayerByIndex( m_nClientIndex ) );
	if ( !pPlayer )
		return;

	m_pCommanderName->SetText( pPlayer->GetPlayerName() );

	int nXP = pPlayer->GetExperienceBeforeDebrief() + pPlayer->GetEarnedXP( ASW_XP_TOTAL );
	int nLevel = LevelFromXP( nXP, pPlayer->GetPromotion() );

	wchar_t szLevelNum[16]=L"";
	_snwprintf( szLevelNum, ARRAYSIZE( szLevelNum ), L"%i", nLevel + 1 );  // levels start at 0 in code, but show from 1 in the UI

	wchar_t wzLevelLabel[64];
	g_pVGuiLocalize->ConstructString( wzLevelLabel, sizeof( wzLevelLabel ), g_pVGuiLocalize->Find( "#asw_experience_level" ), 1, szLevelNum );
	m_pCommanderLevel->SetText( wzLevelLabel );
}

void CNB_Commander_List_Entry::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );
}

void CNB_Commander_List_Entry::SetClientIndex( int nIndex )
{
	m_nClientIndex = nIndex;
}