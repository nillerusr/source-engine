//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "tf_item_inspection_panel.h"
#include "item_model_panel.h"
#include "navigationpanel.h"
#include "gc_clientsystem.h"
#include "econ_ui.h"
#include "backpack_panel.h"
#include "vgui_int.h"
#include "cdll_client_int.h"
#include "clientmode_tf.h"
#include "ienginevgui.h"
#include "tf_hud_mainmenuoverride.h"
#include "econ_item_system.h"

using namespace vgui;

#ifdef STAGING_ONLY
void cc_toggle_debug_inspect( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	// Too early?
	if ( ItemSystem()->GetItemSchema()->GetVersion() == 0 )
		return;

	ConVarRef var( pConVar );
	if ( var.IsValid() )
	{
		CTFItemInspectionPanel* pInspectionPanel = dynamic_cast< CTFItemInspectionPanel* >( EconUI()->GetBackpackPanel()->FindChildByName( "InspectionPanel" ) );
		if ( pInspectionPanel )
		{
			bool bVisible = pInspectionPanel->IsVisible();		// Save visibility
			pInspectionPanel->Reset();
			pInspectionPanel->SetVisible( bVisible );			// Restore visibility
		}
	}
}



ConVar tf_use_debug_inspection_panel( "tf_use_debug_inspection_panel", "0", FCVAR_ARCHIVE, "Toggles developer controls for the item inspection panel", cc_toggle_debug_inspect );
extern ConVar tf_paint_kit_force_regen;
#endif



// ********************************************************************************************************************************
// Given proto data, create an inspect panel
static void Helper_LaunchPreviewWithPreviewDataBlock( CEconItemPreviewDataBlock const &protoData )
{
	// Create an econ item view
	CEconItem econItem;
	econItem.DeserializeFromProtoBufItem( protoData.econitem() );
	
	CEconItemView itemView;
	itemView.Init( econItem.GetDefinitionIndex(), econItem.GetQuality(), econItem.GetItemLevel(), econItem.GetAccountID() );
	itemView.SetNonSOEconItem( &econItem );

	// Get the Backpack to tell the inspect panel to render
	if ( EconUI()->GetBackpackPanel() )
	{
		EconUI()->GetBackpackPanel()->OpenInspectModelPanelAndCopyItem( &itemView );
	}
}

// ********************************************************************************************************************************
// Request an item from the GC to inspect
static void Helper_RequestEconActionPreview( uint64 paramS, uint64 paramA, uint64 paramD, uint64 paramM )
{
	if ( !paramA || !paramD )
		return;
	if ( !paramS && !paramM )
		return;

	static double s_flTime = 0.0f;
	double flNow = Plat_FloatTime();
	if ( s_flTime && ( flNow - s_flTime <= 2.5 ) )
		return;

	s_flTime = flNow;
	GCSDK::CProtoBufMsg< CMsgGC_Client2GCEconPreviewDataBlockRequest > msg( k_EMsgGC_Client2GCEconPreviewDataBlockRequest );
	msg.Body().set_param_s( paramS );
	msg.Body().set_param_a( paramA );
	msg.Body().set_param_d( paramD );
	msg.Body().set_param_m( paramM );
	GCClientSystem()->GetGCClient()->BSendMessage( msg );
}

// ********************************************************************************************************************************
class CGCClient2GCEconPreviewDataBlockResponse : public GCSDK::CGCClientJob
{
public:
	CGCClient2GCEconPreviewDataBlockResponse( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }

	virtual bool BYieldingRunJobFromMsg( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgGC_Client2GCEconPreviewDataBlockResponse> msg( pNetPacket );
		Helper_LaunchPreviewWithPreviewDataBlock( msg.Body().iteminfo() );
		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCClient2GCEconPreviewDataBlockResponse, "CGCClient2GCEconPreviewDataBlockResponse", k_EMsgGC_Client2GCEconPreviewDataBlockResponse, GCSDK::k_EServerTypeGCClient );

// ********************************************************************************************************************************
CON_COMMAND_F( tf_econ_item_preview, "Preview an economy item", FCVAR_DONTRECORD | FCVAR_HIDDEN | FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( args.ArgC() < 2 )
		return;

	//// kill the workshop preview dialog if it's up
	//if ( g_pWorkshopWorkbenchDialog )
	//{
	//	delete g_pWorkshopWorkbenchDialog;
	//	g_pWorkshopWorkbenchDialog = NULL;
	//}

	//extern float g_flReadyToCheckForPCBootInvite;
	//if ( !g_flReadyToCheckForPCBootInvite || !gpGlobals->curtime || !gpGlobals->framecount )
	//{
	//	ConMsg( "Deferring csgo_econ_action_preview command!\n" );
	//	return;
	//}

	// Encoded parameter, validate basic length
	char const *pchEncodedAscii = args.Arg( 1 );
	int nLen = Q_strlen( pchEncodedAscii );
	if ( nLen <= 16 ) { Assert( 0 ); return; }

	// If we are launched with new format requesting steam_ownerid and assetid then do async query
	if ( *pchEncodedAscii == 'S' )
	{
		uint64 uiParamS = Q_atoui64( pchEncodedAscii + 1 );
		uint64 uiParamA = 0;
		uint64 uiParamD = 0;
		if ( char const *pchParamA = strchr( pchEncodedAscii, 'A' ) )
		{
			uiParamA = Q_atoui64( pchParamA + 1 );
			if ( char const *pchParamD = strchr( pchEncodedAscii, 'D' ) )
			{
				uiParamD = Q_atoui64( pchParamD + 1 );
				Helper_RequestEconActionPreview( uiParamS, uiParamA, uiParamD, 0ull );
			}
		}
		return;
	}

	// Else if we are launched with new format requesting market listing id and assetid then do async query
	if ( *pchEncodedAscii == 'M' )
	{
		uint64 uiParamM = Q_atoui64( pchEncodedAscii + 1 );
		uint64 uiParamA = 0;
		uint64 uiParamD = 0;
		if ( char const *pchParamA = strchr( pchEncodedAscii, 'A' ) )
		{
			uiParamA = Q_atoui64( pchParamA + 1 );
			if ( char const *pchParamD = strchr( pchEncodedAscii, 'D' ) )
			{
				uiParamD = Q_atoui64( pchParamD + 1 );
				Helper_RequestEconActionPreview( 0ull, uiParamA, uiParamD, uiParamM );
			}
		}
		return;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFItemInspectionPanel::CTFItemInspectionPanel( Panel* pPanel, const char *pszName )
	: BaseClass( pPanel, pszName )
	, m_pModelInspectPanel( NULL )
	, m_pItemViewData( NULL )
	, m_pSOEconItemData( NULL )
{
	m_pModelInspectPanel = new CEmbeddedItemModelPanel( this, "ModelInspectionPanel" );
	m_pTeamColorNavPanel = new CNavigationPanel( this, "TeamNavPanel" );
	m_pItemNamePanel = new CItemModelPanel( this, "ItemName" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFItemInspectionPanel::ApplySchemeSettings( IScheme *pScheme )
{
	const char* pszFileName = "Resource/UI/econ/InspectionPanel.res";
#ifdef STAGING_ONLY
	if ( tf_use_debug_inspection_panel.GetBool() )
	{
		pszFileName = "Resource/UI/econ/InspectionPanel_Dev.res";
	}
#endif

	LoadControlSettings( pszFileName );

	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFItemInspectionPanel::PerformLayout()
{
	CEconItemView* pItem = m_pModelInspectPanel->GetItem();
	if ( pItem && pItem->IsValid() )
	{
		// Find out if we should show the team buttons if the item has team skins
		if ( m_pTeamColorNavPanel )
		{
			static CSchemaAttributeDefHandle pAttrTeamColoredPaintkit( "has team color paintkit" );
			uint32 iHasTeamColoredPaintkit = 0;

			// Find out if the item has the attribute
			bool bShowTeamButton = FindAttribute_UnsafeBitwiseCast<attrib_value_t>( pItem, pAttrTeamColoredPaintkit, &iHasTeamColoredPaintkit ) && iHasTeamColoredPaintkit > 0;

			m_pTeamColorNavPanel->SetVisible( bShowTeamButton );
		}

		// Show only the name if there's no rarity
		//m_pItemNamePanel->SetNameOnly( true );

		// Force the description to update right now, or else it might be caught up
		// in the queue of 50 panels in the backpack which want to load their crap first
		m_pItemNamePanel->UpdateDescription();	
	}

	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFItemInspectionPanel::OnCommand( const char *command )
{
	if ( FStrEq( command, "close" ) )
	{
		// Clean up after ourselves and set the item back to red
		CEconItemView* pItem = m_pModelInspectPanel->GetItem();
		if ( pItem )
		{
			pItem->SetTeamNumber( TF_TEAM_RED );
			// We need to recomposite again because we might have a blue version
			// in the cache that might get used in a tooltip.
			RecompositeItem();
		}

		SetVisible( false );
		return;
	}
#ifdef STAGING_ONLY
	else if ( FStrEq( command, "newseed" ) )
	{
		ConVarRef cv_seed( "tf_paint_kit_seed_override" );
		cv_seed.SetValue( RandomInt( 0, 10000000 ) );
		RecompositeItem();
		return;
	}
	else if ( FStrEq( command, "changeteam" ) )
	{
		ConVarRef cv_team( "tf_paint_kit_team_override" );
		cv_team.SetValue ( cv_team.GetInt() == 0 ? 1 : 0 );
		RecompositeItem();
		return;
	}
	else if ( V_strncmp( "wear", command, 4 ) == 0 )
	{
		int nWearLevel = atoi( command + 4 );
		ConVarRef cv_wear( "tf_paint_kit_force_wear" );
		cv_wear.SetValue( nWearLevel );
		RecompositeItem();
		return;
	}
	else if ( FStrEq( command, "refresh_skin" ) )
	{
		engine->ClientCmd_Unrestricted( "tf_paint_kit_override_refresh" );
		RecompositeItem();
		return;
	}
#endif

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFItemInspectionPanel::SetItem( CEconItemView *pItem )
{
	m_pItemNamePanel->SetItem( pItem );
	m_pModelInspectPanel->SetItem( pItem );
	RecompositeItem();
	InvalidateLayout();
	m_pTeamColorNavPanel->UpdateButtonSelectionStates( 0 );
}
//-----------------------------------------------------------------------------
void CTFItemInspectionPanel::SetSpecialAttributesOnly( bool bSpecialOnly ) 
{ 
	if ( m_pItemNamePanel )
	{
		if ( bSpecialOnly )
		{
			m_pItemNamePanel->SetNameOnly( false );
		}
		m_pItemNamePanel->SetSpecialAttributesOnly( bSpecialOnly ); 
	}
}
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFItemInspectionPanel::Reset()
{
	InvalidateLayout( true, true );
	SetItem( m_pItemViewData );
}

//-----------------------------------------------------------------------------
void CTFItemInspectionPanel::SetItemCopy( CEconItemView *pItem )
{
	// Make a copy of SO data since it comes from the market and will fall out of scope
	if ( m_pItemViewData )
	{
		delete m_pItemViewData;
		m_pItemViewData = NULL;
	}

	if ( m_pSOEconItemData )
	{
		delete m_pSOEconItemData;
		m_pSOEconItemData = NULL;
	}

	if ( pItem )
	{
		m_pItemViewData = new CEconItemView( *pItem );
		m_pSOEconItemData = new CEconItem( *pItem->GetSOCData() ); 
		m_pItemViewData->SetNonSOEconItem( m_pSOEconItemData );
		// always use high res for inspect
		m_pItemViewData->SetWeaponSkinUseHighRes( true );
	}
	SetItem( m_pItemViewData );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFItemInspectionPanel::OnRadioButtonChecked( vgui::Panel *panel )
{
	OnCommand( panel->GetName() );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFItemInspectionPanel::OnNavButtonSelected( KeyValues *pData )
{
	const int iTeam = pData->GetInt( "userdata", -1 );	AssertMsg( iTeam >= 0, "Bad filter" );
	if ( iTeam < 0 )
		return;

	CEconItemView* pItem = m_pModelInspectPanel->GetItem();
	if ( pItem )
	{
		pItem->SetTeamNumber( iTeam );

		RecompositeItem();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFItemInspectionPanel::RecompositeItem()
{
	// Force a reload of the skin
	CEconItemView* pItem = m_pModelInspectPanel->GetItem();
	if ( pItem )
	{
#ifdef STAGING_ONLY
		if ( !tf_paint_kit_force_regen.GetBool() )
#endif
		{
			pItem->CancelWeaponSkinComposite();
			pItem->SetWeaponSkinBase( NULL );
			pItem->SetWeaponSkinBaseCompositor( NULL );
		}
	}
}
