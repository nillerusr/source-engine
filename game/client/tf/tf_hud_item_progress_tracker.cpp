//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "tf_hud_item_progress_tracker.h"
#include "iclientmode.h"
#include <vgui_controls/Label.h>
#include <vgui_controls/ImagePanel.h>
#include "gc_clientsystem.h"
#include "engine/IEngineSound.h"
#include "quest_log_panel.h"
#include "econ_controls.h"
#include "tf_item_inventory.h"
#include "c_tf_player.h"
#include "quest_objective_manager.h"
#include "tf_spectatorgui.h"
#include "econ_quests.h"
#include "inputsystem/iinputsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

const float ATTRIB_TRACK_GLOW_HOLD_TIME = 2.f;
const float ATTRIB_TRACK_BAR_GROW_RATE = 0.3f;
const float ATTRIB_TRACK_COMPLETE_PULSE_RATE = 2.f;
const float ATTRIB_TRACK_COMPLETE_PULSE_DIM_HOLD = 0.3f;
const float ATTRIB_TRACK_COMPLETE_PULSE_GLOW_HOLD = 0.9f;

enum EContractHUDVisibility
{
	CONTRACT_HUD_SHOW_NONE = 0,
	CONTRACT_HUD_SHOW_EVERYTHING,
	CONTRACT_HUD_SHOW_ACTIVE,
};

void cc_contract_progress_show_update( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	CHudItemAttributeTracker *pTrackerPanel = (CHudItemAttributeTracker *)GET_HUDELEMENT( CHudItemAttributeTracker );
	if ( pTrackerPanel )
	{
		pTrackerPanel->InvalidateLayout();
	}
}
ConVar tf_contract_progress_show( "tf_contract_progress_show", "1", FCVAR_CLIENTDLL | FCVAR_DONTRECORD | FCVAR_ARCHIVE, "Settings for the contract HUD element: 0 show nothing, 1 show everything, 2 show only active contracts.", cc_contract_progress_show_update );
ConVar tf_contract_competitive_show( "tf_contract_competitive_show", "2", FCVAR_CLIENTDLL | FCVAR_DONTRECORD | FCVAR_ARCHIVE, "Settings for the contract HUD element during competitive matches: 0 show nothing, 1 show everything, 2 show only active contracts.", cc_contract_progress_show_update );

EContractHUDVisibility GetContractHUDVisibility()
{
	if ( TFGameRules() && TFGameRules()->IsMatchTypeCompetitive() )
		return (EContractHUDVisibility)tf_contract_competitive_show.GetInt();

	return (EContractHUDVisibility)tf_contract_progress_show.GetInt();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CItemAttributeProgressPanel::CItemAttributeProgressPanel( Panel* pParent, const char *pElementName, const CQuestObjectiveDefinition *pObjectiveDef, const char* pszResFileName  )
	: EditablePanel( pParent, pElementName )
	, m_nDefIndex( pObjectiveDef->GetDefinitionIndex() )
	, m_flUpdateTime( 0.f )
	, m_flLastThink( 0.f )
	, m_bAdvanced( pObjectiveDef->IsAdvanced() )
	, m_strResFileName( pszResFileName )
{
	Assert( !m_strResFileName.IsEmpty() );

	m_pAttribBlur = new Label( this, "AttribBlur", "" );
	m_pAttribGlow = new Label( this, "AttribGlow", "" );
	m_pAttribDesc = new Label( this, "AttribDesc", "" );

	REGISTER_COLOR_AS_OVERRIDABLE( m_enabledTextColor, "enabled_text_color_override" );
	REGISTER_COLOR_AS_OVERRIDABLE( m_disabledTextColor, "disabled_text_color_override" );

	// We're being set for the first time, instantly be progressed
	m_pAttribBlur->SetAlpha( 0 );
	m_pAttribGlow->SetAlpha( 0 );
	m_flUpdateTime = Plat_FloatTime() - ATTRIB_TRACK_GLOW_HOLD_TIME;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItemAttributeProgressPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	LoadControlSettings( m_strResFileName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItemAttributeProgressPanel::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	m_strNormalPointLocToken = inResourceData->GetString( "normal_token" );
	m_strAdvancedLocToken = inResourceData->GetString( "advanced_token" );

	const CQuestObjectiveDefinition *pObjectiveDef = GEconItemSchema().GetQuestObjectiveByDefIndex( m_nDefIndex );
	if ( pObjectiveDef )
	{
		const char *pszDescriptionToken = pObjectiveDef->GetDescriptionToken();

		locchar_t loc_ItemDescription[MAX_ITEM_NAME_LENGTH];
		const locchar_t *pLocalizedObjectiveName = GLocalizationProvider()->Find( pszDescriptionToken );
		if ( !pLocalizedObjectiveName || !pLocalizedObjectiveName[0] )
		{
			// Couldn't localize it, just use it raw
			GLocalizationProvider()->ConvertUTF8ToLocchar( pszDescriptionToken, loc_ItemDescription, ARRAYSIZE( loc_ItemDescription ) );
		}
		else
		{
			locchar_t loc_IntermediateName[ MAX_ITEM_NAME_LENGTH ];
			locchar_t locValue[ MAX_ITEM_NAME_LENGTH ];
			loc_sprintf_safe( locValue, LOCCHAR( "%d" ), pObjectiveDef->GetPoints() );

			loc_scpy_safe( loc_IntermediateName, CConstructLocalizedString( pLocalizedObjectiveName, locValue ) );

			locchar_t *pszLocString = GLocalizationProvider()->Find( pObjectiveDef->IsAdvanced() ? m_strAdvancedLocToken : m_strNormalPointLocToken );

			loc_scpy_safe( loc_ItemDescription, CConstructLocalizedString( pszLocString, loc_IntermediateName ) );
		}
	
		SetDialogVariable( "attr_desc", loc_ItemDescription );
	}

	//SetTall( GetContentTall() );
	//m_pAttribDesc->SetTall( GetTall() );

	InvalidateLayout();
}

void CItemAttributeProgressPanel::SetIsValid( bool bIsValid )
{
	if ( bIsValid )
	{
		m_pAttribDesc->SetFgColor( m_enabledTextColor );
	}
	else
	{
		m_pAttribDesc->SetFgColor( m_disabledTextColor );
	}
}

int CItemAttributeProgressPanel::GetContentTall() const
{
	// Find the bottom of the text
	int nTextWide = 0, nTextTall = 0;
	m_pAttribDesc->GetContentSize( nTextWide, nTextTall );
	int nTextYpos = m_pAttribDesc->GetYPos();
	return nTextYpos + nTextTall;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItemAttributeProgressPanel::SetProgress( Color glowColor )
{
	m_flUpdateTime = Plat_FloatTime();
	m_pAttribBlur->SetAlpha( 255 );
	m_pAttribGlow->SetAlpha( 255 );
	m_pAttribDesc->SetAlpha( 0 );
	m_pAttribBlur->SetFgColor( glowColor );
	m_pAttribGlow->SetFgColor( glowColor );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItemAttributeProgressPanel::OnThink()
{
	float flGlowTime = Plat_FloatTime() - m_flUpdateTime;
	if ( flGlowTime > ATTRIB_TRACK_GLOW_HOLD_TIME )
	{
		float flGlowAlpha = RemapValClamped( flGlowTime, ATTRIB_TRACK_GLOW_HOLD_TIME, ATTRIB_TRACK_GLOW_HOLD_TIME + 0.25f, 1.f, 0.f );
		m_pAttribBlur->SetAlpha( 255 * flGlowAlpha );
		m_pAttribGlow->SetAlpha( 255 * flGlowAlpha );
		m_pAttribDesc->SetAlpha( 255 * ( 1.f - flGlowAlpha ) );
	}

	m_flLastThink = Plat_FloatTime();
}

float CItemTrackerPanel::m_sflEventRecievedTime = 0.f;
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CItemTrackerPanel::CItemTrackerPanel( Panel* pParent, const char *pElementName, const CEconItem* pItem, const char* pszItemTrackerResFile )
	: EditablePanel( pParent, pElementName )
	, m_pItem( NULL )
	, m_pCompletedContainer( NULL )
	, m_pCompletedDescGlow( NULL )
	, m_pCompletedNameGlow( NULL )
	, m_flStandardTargetProgress( 0.f )
	, m_flStandardCurrentProgress( 0.f )
	, m_flBonusCurrentProgress( 0.f )
	, m_flBonusTargetProgress( 0.f )
	, m_flUpdateTime( 0.f )
	, m_flLastThink( 0.f )
	, m_nMaxStandardPoints( 0 )
	, m_nMaxBonusPoints( 0 )
	, m_eSoundToPlay( SOUND_NONE )
	, m_nContentTall( 0 )
	, m_bNoEffects( false )
	, m_strItemTrackerResFile( pszItemTrackerResFile )
{
	Assert( pszItemTrackerResFile );

	SetItem( pItem );
	ListenForGameEvent( "quest_objective_completed" );
	ListenForGameEvent( "player_spawn" );
	ListenForGameEvent( "inventory_updated" );
	ListenForGameEvent( "localplayer_changeclass" );
	ListenForGameEvent( "schema_updated" );

	m_pItemName = new Label( this, "ItemName", "" );
	m_pCompletedContainer = new EditablePanel( this, "CompletedContainer" );

	m_pProgressBarBackground = new EditablePanel( this, "ProgressBarBG" );
	m_pProgressBarStandard = new EditablePanel( m_pProgressBarBackground, "ProgressBarStandard" );
	m_pProgressBarBonus = new EditablePanel( m_pProgressBarBackground, "ProgressBarBonus" );
	m_pProgressBarStandardHighlight = new EditablePanel( m_pProgressBarBackground, "ProgressBarStandardHighlight" );
	m_pProgressBarBonusHighlight = new EditablePanel( m_pProgressBarBackground, "ProgressBarBonusHighlight" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CItemTrackerPanel::~CItemTrackerPanel()
{}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItemTrackerPanel::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	m_strItemAttributeResFile = inResourceData->GetString( "item_attribute_res_file" );
	m_strProgressBarStandardLocToken = inResourceData->GetString( "progress_bar_standard_loc_token" );
	m_strProgressBarAdvancedLocToken = inResourceData->GetString( "progress_bar_advanced_loc_token" );

	Assert( !m_strItemAttributeResFile.IsEmpty() );
	Assert( !m_strProgressBarStandardLocToken.IsEmpty() );
	Assert( !m_strProgressBarAdvancedLocToken.IsEmpty() );

	m_strStandardObjectiveTick		= inResourceData->GetString( "standard_objective_tick_sound", NULL );
	m_strStandardPointsComplete		= inResourceData->GetString( "standard_points_complete_sound", NULL );
	m_strAdvancedObjectiveComplete	= inResourceData->GetString( "advanced_objective_sound_complete", NULL );
	m_strAdvancedPointsComplete		= inResourceData->GetString( "advanced_points_complete_sound", NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItemTrackerPanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	LoadControlSettings( m_strItemTrackerResFile );

	m_pCompletedDescGlow = FindControl<Label>( "CompleteGlowText", true );
	m_pCompletedNameGlow = FindControl<Label>( "CompleteItemNameGlow", true );

	QuestObjectiveDefVec_t vecChosenObjectives;
	if ( m_pItem )
	{
		m_pItem->GetItemDefinition()->GetQuestDef()->GetRolledObjectivesForItem( vecChosenObjectives, m_pItem->GetSOCData() );
	}
	FOR_EACH_VEC( vecChosenObjectives, i )
	{
		auto pObjective = vecChosenObjectives[ i ];

		// Find or create the individual attribute panel
		CItemAttributeProgressPanel *pPanel = GetPanelForObjective( pObjective );
		pPanel->InvalidateLayout();
	}

	CaptureProgress();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int QuestSort_PointsAscending( CItemAttributeProgressPanel* const* p1, CItemAttributeProgressPanel* const* p2 )
{
	const CQuestObjectiveDefinition* pObj1 = GEconItemSchema().GetQuestObjectiveByDefIndex( (*p1)->m_nDefIndex );
	const CQuestObjectiveDefinition* pObj2 = GEconItemSchema().GetQuestObjectiveByDefIndex( (*p2)->m_nDefIndex );

	if ( pObj1->GetPoints() != pObj2->GetPoints() )
	{
		// Smallest point value on the bottom
		return pObj1->GetPoints() - pObj2->GetPoints();
	}

	return pObj1->GetDefinitionIndex() - pObj2->GetDefinitionIndex();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItemTrackerPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( !m_pItem )
		return;

	// Set the name into dialog variables
	const wchar_t *pszLocToken = g_pVGuiLocalize->Find( m_pItem->GetItemDefinition()->GetQuestDef()->GetRolledNameForItem( m_pItem->GetSOCData() ) );
	SetDialogVariable( "itemname", pszLocToken );
	if ( m_pCompletedContainer )
	{
		m_pCompletedContainer->SetDialogVariable( "itemname", pszLocToken );
	}

	const CQuestItemTracker* pItemTracker = QuestObjectiveManager()->GetTypedTracker< CQuestItemTracker* >( m_pItem->GetItemID() );
	
	if ( pItemTracker )
	{
		bool bStandardPointsCompleteAndBonusPossible = m_flStandardCurrentProgress == m_flStandardTargetProgress && m_flStandardTargetProgress == 1.f && m_nMaxBonusPoints > 0;
		uint32 nCurrentPoints = bStandardPointsCompleteAndBonusPossible ? pItemTracker->GetEarnedBonusPoints() : pItemTracker->GetEarnedStandardPoints();
		uint32 nTargetPoints = bStandardPointsCompleteAndBonusPossible ? m_nMaxBonusPoints : m_nMaxStandardPoints;
			
		locchar_t locValue[ MAX_ITEM_NAME_LENGTH ];
		const locchar_t *pPointsToken = GLocalizationProvider()->Find( bStandardPointsCompleteAndBonusPossible ? m_strProgressBarAdvancedLocToken : m_strProgressBarStandardLocToken );
		loc_scpy_safe( locValue, CConstructLocalizedString( pPointsToken, nCurrentPoints, nTargetPoints ) );

		m_pProgressBarBackground->SetDialogVariable( "points", locValue );
		m_pProgressBarStandard->SetDialogVariable( "points" , locValue );
		m_pProgressBarStandardHighlight->SetDialogVariable( "points" , locValue );
		m_pProgressBarBonus->SetDialogVariable( "points", locValue );
		m_pProgressBarBonusHighlight->SetDialogVariable( "points" , locValue );
	}
	else
	{
		m_pProgressBarBackground->SetDialogVariable( "points", "" );
		m_pProgressBarStandard->SetDialogVariable( "points" , "" );
		m_pProgressBarStandardHighlight->SetDialogVariable( "points" , "" );
		m_pProgressBarBonus->SetDialogVariable( "points", "" );
		m_pProgressBarBonusHighlight->SetDialogVariable( "points" , "" );
	}

	m_pProgressBarStandardHighlight->SetVisible( !m_bNoEffects );
	m_pProgressBarBonusHighlight->SetVisible( !m_bNoEffects );

	int nWide = 0, nTall = 0;
	if ( m_pItemName->IsVisible() && !m_bNoEffects )
	{
		m_pItemName->GetContentSize( nWide, nTall );
	}

	int nX = m_nAttribXOffset;
	int nY = nTall + m_nAttribYStartOffset;

	bool bCompleted = true;

	m_vecAttribPanels.Sort( &QuestSort_PointsAscending );

	if ( !IsStandardCompleted() )
	{
		bCompleted = false;
	}

	m_pCompletedContainer->SetVisible( bCompleted );

	if ( bCompleted && !m_bNoEffects )
	{
		m_pCompletedContainer->SetVisible( true );
		CExLabel* pCompleted = m_pCompletedContainer->FindControl< CExLabel >( "CompleteDesc", true );
		CExLabel* pCompletedGlow = m_pCompletedContainer->FindControl< CExLabel > ( "CompleteGlowText", true );
		if ( pCompleted && pCompletedGlow )
		{
			const wchar_t *pszText = NULL;
			const char *pszTextKey = "#QuestTracker_Complete";
			if ( pszTextKey )
			{
				pszText = g_pVGuiLocalize->Find( pszTextKey );
			}
			if ( pszText )
			{					
				wchar_t wzFinal[512] = L"";
				UTIL_ReplaceKeyBindings( pszText, 0, wzFinal, sizeof( wzFinal ), GAME_ACTION_SET_FPSCONTROLS );
				pCompleted->SetText( wzFinal );
				pCompletedGlow->SetText( wzFinal );
			}
		}

		nY = m_pCompletedContainer->GetTall();
	}
	else
	{
		m_pCompletedContainer->SetVisible( false );
	}

	// Place the bars at the bottom of the text, not the bottom of the text panel
	m_pProgressBarBackground->SetPos( m_pProgressBarBackground->GetXPos(), nY );
	nY += m_pProgressBarBackground->GetTall() + m_nBarGap;

	UpdateBars();

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	FOR_EACH_VEC( m_vecAttribPanels, i )
	{
		CItemAttributeProgressPanel* pPanel = m_vecAttribPanels[i];

		// Hide all objectives if everything is completed, or just hide standard objectives is standard points re completed
		if ( !m_bNoEffects && ( IsEverythingCompleted() || ( IsStandardCompleted() && !pPanel->IsAdvanced() ) ) )
		{
			pPanel->SetVisible( false );
		}
		else
		{
			pPanel->SetPos( nX, nY );
			nY += pPanel->GetContentTall();
			pPanel->SetVisible( true );

			InvalidReasonsContainer_t invalidReasons;
			if ( pItemTracker && pLocalPlayer )
			{
				// Fixup validity, which changes the color of the 
				const CBaseQuestObjectiveTracker* pObjectiveTracker = pItemTracker->FindTrackerForDefIndex( pPanel->m_nDefIndex );
				if ( pObjectiveTracker )
				{
					pObjectiveTracker->IsValidForPlayer( pLocalPlayer, invalidReasons );
				}
			}

			pPanel->SetIsValid( invalidReasons.IsValid() );

			nY += m_nAttribYStep;
		}
	}

	m_nContentTall = nY;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CItemTrackerPanel::GetContentTall() const
{
	return m_nContentTall;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItemTrackerPanel::OnThink()
{
	BaseClass::OnThink();

	if ( !m_pItem || m_bNoEffects )
		return;

	static double s_flLastSoundTime = 0;
	// Give a little time in case other messaes are in flight
	if ( ( Plat_FloatTime() - m_sflEventRecievedTime ) > 0.1f )
	{
		// Dont play sounds very frequently
		if ( m_eSoundToPlay != SOUND_NONE && ( ( Plat_FloatTime() - s_flLastSoundTime ) > 0.1f ) )
		{
			const char *pszSoundName = NULL;

			// Figure out which sound to play
			switch( m_eSoundToPlay )
			{
			case SOUND_STANDARD_OBJECTIVE_TICK:
				pszSoundName = m_strStandardObjectiveTick;
				break;
			case SOUND_ADVANCED_OBJECTIVE_TICK:
				pszSoundName = m_strAdvancedObjectiveComplete;
				break;
			case SOUND_QUEST_STANDARD_COMPLETE:
				pszSoundName = m_strStandardPointsComplete;
				break;
			case SOUND_QUEST_ADVANCED_COMPLETE:
				pszSoundName = m_strAdvancedPointsComplete;
				break;
			};

			s_flLastSoundTime = Plat_FloatTime();
			CLocalPlayerFilter filter;
			
			C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, pszSoundName );
		}

		m_eSoundToPlay = SOUND_NONE;
	}

	// If the highlight is done, and the quest is ready to turn in, pulse a glow behind
	// the title and the completion instruction
	if ( IsDoneProgressing() && IsStandardCompleted() )
	{
		if ( m_pCompletedDescGlow && m_pCompletedNameGlow )
		{
			float flPeriod = ( sin( gpGlobals->curtime * ATTRIB_TRACK_COMPLETE_PULSE_RATE ) * 0.5f ) + 0.5f;
			float flGlowAlpha = RemapValClamped( flPeriod, ATTRIB_TRACK_COMPLETE_PULSE_DIM_HOLD, ATTRIB_TRACK_COMPLETE_PULSE_GLOW_HOLD, 0.f, 1.f );
			m_pCompletedDescGlow->SetAlpha( 255 * flGlowAlpha );
			m_pCompletedNameGlow->SetAlpha( 255 * flGlowAlpha );
		}
	}

	float flGlowTime = Plat_FloatTime() - m_flUpdateTime;
	if ( flGlowTime > ATTRIB_TRACK_GLOW_HOLD_TIME && ( m_flStandardCurrentProgress != m_flStandardTargetProgress || m_flBonusCurrentProgress != m_flBonusTargetProgress ) )
	{
		float flDelta = Plat_FloatTime() - m_flLastThink;
		m_flStandardCurrentProgress = Approach( m_flStandardTargetProgress, m_flStandardCurrentProgress, flDelta * ATTRIB_TRACK_BAR_GROW_RATE );
		m_flBonusCurrentProgress = Approach( m_flBonusTargetProgress, m_flBonusCurrentProgress, flDelta * ATTRIB_TRACK_BAR_GROW_RATE );

		// Resize/position the bars
		UpdateBars();

		// This happens when all the bars are all caught up
		if ( IsDoneProgressing() )
		{
			InvalidateLayout();
		}
	}

	if ( IsDoneProgressing() )
	{
		m_pProgressBarStandardHighlight->SetVisible( false );
		m_pProgressBarBonusHighlight->SetVisible( false );
	}

	m_flLastThink = Plat_FloatTime();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItemTrackerPanel::FireGameEvent( IGameEvent *pEvent )
{
	if ( FStrEq( pEvent->GetName(), "quest_objective_completed" ) )
	{
		itemid_t nIDLow = 0x00000000FFFFFFFF & (itemid_t)pEvent->GetInt( "quest_item_id_low" );
		itemid_t nIDHi =  0xFFFFFFFF00000000 & (itemid_t)pEvent->GetInt( "quest_item_id_hi" ) << 32;
		itemid_t nID = nIDLow | nIDHi;
		if ( !m_pItem || m_pItem->GetItemID() != nID )
			return;

		// Capture whatever progress has happened
		CaptureProgress();

		uint32 nObjectiveDefIndex = pEvent->GetInt( "quest_objective_id" );

		// Don't do sounds if there's no objective that made this progress
		if ( nObjectiveDefIndex == (uint32)-1 )
			return;

		bool bAdvanced = false;
		const CQuestObjectiveDefinition* pObjective = GEconItemSchema().GetQuestObjectiveByDefIndex( nObjectiveDefIndex );
		if ( !pObjective )
			return;

		bAdvanced = pObjective->IsAdvanced();

		const CQuestItemTracker* pItemTracker = QuestObjectiveManager()->GetTypedTracker< CQuestItemTracker* >( m_pItem->GetItemID() );

		if ( pItemTracker )
		{
			const Color& glowColor = pItemTracker->GetEarnedStandardPoints() < m_nMaxStandardPoints ? m_clrStandardHighlight
																									: m_clrBonusHighlight;
			FOR_EACH_VEC( m_vecAttribPanels, i )
			{
				if ( m_vecAttribPanels[i]->m_nDefIndex == nObjectiveDefIndex )
				{
					m_vecAttribPanels[i]->SetProgress( glowColor );
					break;
				}
			}

			// Check if it's time to play a sound
			ESoundToPlay eNewSound = SOUND_NONE;

			if ( m_flStandardCurrentProgress < m_flStandardTargetProgress && pItemTracker->GetEarnedStandardPoints() == m_nMaxStandardPoints )
			{
				eNewSound = SOUND_QUEST_STANDARD_COMPLETE;
			}
			else if ( m_flBonusCurrentProgress < m_flBonusTargetProgress && pItemTracker->GetEarnedBonusPoints() == m_nMaxBonusPoints )
			{
				eNewSound = SOUND_QUEST_ADVANCED_COMPLETE;
			}
			else if ( bAdvanced )
			{
				eNewSound = SOUND_ADVANCED_OBJECTIVE_TICK;
			}
			else
			{
				eNewSound = SOUND_STANDARD_OBJECTIVE_TICK;
			}

			// Priority will handle this for us
			if ( eNewSound > m_eSoundToPlay )
			{
				m_eSoundToPlay = eNewSound;
				if ( m_sflEventRecievedTime < Plat_FloatTime() )
				{
					m_sflEventRecievedTime = Plat_FloatTime();
				}
			}
		}
	}
	else if ( FStrEq( pEvent->GetName(), "player_spawn" ) 
		   || FStrEq( pEvent->GetName(), "inventory_updated" ) 
		   || FStrEq( pEvent->GetName(), "localplayer_changeclass" ) )
	{
		InvalidateLayout();
	}
	else if ( FStrEq( pEvent->GetName(), "schema_updated" ) )
	{
		FOR_EACH_VEC( m_vecAttribPanels, i )
		{
			m_vecAttribPanels[ i ]->InvalidateLayout();
		}

		InvalidateLayout();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItemTrackerPanel::UpdateBars()
{
	// Everything is relative to the background bar
	int nWide = m_pProgressBarBackground->GetWide();

	// Resize standard bar
	m_pProgressBarStandard->SetWide( floor(m_flStandardCurrentProgress * nWide ) );
	// Resize bonus bar
	m_pProgressBarBonus->SetWide( floor(m_flBonusCurrentProgress * nWide ) );

	// Highlight bars snap to the target width
	m_pProgressBarStandardHighlight->SetWide( floor(m_flStandardTargetProgress * nWide) );
	m_pProgressBarBonusHighlight->SetWide( floor( m_flBonusTargetProgress * nWide ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItemTrackerPanel::CaptureProgress()
{
	if ( !m_pItem )
		return;

	const CQuestItemTracker* pItemTracker = QuestObjectiveManager()->GetTypedTracker< CQuestItemTracker* >( m_pItem->GetItemID() );

	// Fixup bar bounds
	float flStandardProgress = 0.f;
	float flBonusProgress = 0.f;

	if ( pItemTracker )
	{
		// We use the item trackers for quest progress since they're the most up-to-date
		flStandardProgress = (float)pItemTracker->GetEarnedStandardPoints() / (float)( m_nMaxStandardPoints);
		flBonusProgress = (float)pItemTracker->GetEarnedBonusPoints() / (float)( m_nMaxBonusPoints );
	}

	// Standard progress
	bool bChange = flStandardProgress != m_flStandardTargetProgress;
	m_flStandardTargetProgress = flStandardProgress;

	// Bonus progress
	bChange |= flBonusProgress != m_flBonusTargetProgress;
	m_flBonusTargetProgress = flBonusProgress;

	// We're being set for the first time, instantly be progressed
	if ( m_flUpdateTime == 0.f || m_bNoEffects )
	{
		m_flStandardCurrentProgress = m_flStandardTargetProgress;
		m_flBonusCurrentProgress = m_flBonusTargetProgress;

		m_flUpdateTime = Plat_FloatTime() - ATTRIB_TRACK_GLOW_HOLD_TIME;
		InvalidateLayout();
	}
	else if ( bChange ) // If this is a change, play effects
	{
		m_flUpdateTime = Plat_FloatTime();
		InvalidateLayout();
	}

	m_pProgressBarStandardHighlight->SetVisible( !m_bNoEffects );
	m_pProgressBarBonusHighlight->SetVisible( !m_bNoEffects );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CItemTrackerPanel::IsStandardCompleted() const
{
	const CQuestItemTracker* pItemTracker = QuestObjectiveManager()->GetTypedTracker< CQuestItemTracker* >( m_pItem->GetItemID() );
	if ( pItemTracker )
	{
		return pItemTracker->GetEarnedStandardPoints() >= m_nMaxStandardPoints;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CItemTrackerPanel::IsEverythingCompleted() const
{
	const CQuestItemTracker* pItemTracker = QuestObjectiveManager()->GetTypedTracker< CQuestItemTracker* >( m_pItem->GetItemID() );
	if ( pItemTracker )
	{
		return ( pItemTracker->GetEarnedBonusPoints() + pItemTracker->GetEarnedStandardPoints() ) >= ( m_nMaxBonusPoints + m_nMaxStandardPoints );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItemTrackerPanel::SetItem( const CEconItem* pItem )
{
	m_pItem.SetItem( TFInventoryManager()->GetLocalTFInventory()->GetInventoryItemByItemID( pItem->GetItemID() ) );

	m_nMaxStandardPoints = pItem->GetItemDefinition()->GetQuestDef()->GetMaxStandardPoints();
	m_nMaxBonusPoints = pItem->GetItemDefinition()->GetQuestDef()->GetMaxBonusPoints();

	InvalidateLayout();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CItemAttributeProgressPanel* CItemTrackerPanel::GetPanelForObjective( const CQuestObjectiveDefinition* pObjective )
{
	FOR_EACH_VEC( m_vecAttribPanels, i )
	{
		if ( m_vecAttribPanels[ i ]->m_nDefIndex == pObjective->GetDefinitionIndex() )
			return m_vecAttribPanels[ i ];
	}

	CItemAttributeProgressPanel *pPanel = new CItemAttributeProgressPanel( this, "ItemAttributeProgressPanel", pObjective, m_strItemAttributeResFile );
	SETUP_PANEL( pPanel );

	m_vecAttribPanels.AddToTail( pPanel );

	return pPanel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CItemTrackerPanel::IsValidForLocalPlayer() const
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	CSteamID steamID;
	if ( !pLocalPlayer || !pLocalPlayer->GetSteamID( &steamID ) )
		return false;

	Assert( m_pItem );

	// Safeguard.  There's a crash in public from this.
	if ( !m_pItem )
		return false;

	const CQuestItemTracker* pItemTracker = QuestObjectiveManager()->GetTypedTracker< CQuestItemTracker* >( m_pItem->GetItemID() );
	InvalidReasonsContainer_t invalidReasons;
	if ( pItemTracker )
	{
		int nNumInvalid = pItemTracker->IsValidForPlayer( pLocalPlayer, invalidReasons );
		if ( nNumInvalid < pItemTracker->GetTrackers().Count() )
		{
			return true;
		}
	}

	// Check for completed quests.  They are always visible.
	CEconItemView *pItem = TFInventoryManager()->GetLocalTFInventory()->GetInventoryItemByItemID( m_pItem->GetItemID() );
	if ( pItem && IsQuestItemReadyToTurnIn( pItem ) && GetContractHUDVisibility() == CONTRACT_HUD_SHOW_EVERYTHING )
	{
		return true;
	}

	return false;
}


DECLARE_HUDELEMENT( CHudItemAttributeTracker );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudItemAttributeTracker::CHudItemAttributeTracker( const char *pElementName )
	: CHudElement( pElementName )
	, EditablePanel( NULL, "ItemAttributeTracker" )
	, m_mapTrackers( DefLessFunc( itemid_t ) )
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	ListenForGameEvent( "player_spawn" );
	ListenForGameEvent( "inventory_updated" );
	ListenForGameEvent( "client_disconnect" );
	ListenForGameEvent( "localplayer_changeclass" );
	ListenForGameEvent( "quest_objective_completed" );

	RegisterForRenderGroup( "weapon_selection" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudItemAttributeTracker::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/HudItemAttributeTracker.res" );

	m_pStatusContainer = FindControl< EditablePanel >( "QuestsStatusContainer" );
	m_pStatusHeaderLabel = m_pStatusContainer->FindControl< Label >( "Header" );
	m_pCallToActionLabel = m_pStatusContainer->FindControl< Label >( "CallToAction" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudItemAttributeTracker::PerformLayout()
{
	BaseClass::PerformLayout();

	CUtlVector<CEconItemView*> vecQuestItems;
	TFInventoryManager()->GetAllQuestItems( &vecQuestItems );
	int nNumCompleted = 0;
	int nNumUnidentified = 0;
	int nNumInactive = 0;
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	FOR_EACH_VEC( vecQuestItems, i )
	{
		const CEconItemView* pItem = vecQuestItems[i];
		if ( IsQuestItemReadyToTurnIn( pItem ) )
		{
			++nNumCompleted;
		}
		else if ( IsQuestItemUnidentified( pItem->GetSOCData() ) )
		{
			++nNumUnidentified;
		}
		else if ( pLocalPlayer )
		{
			const CQuestItemTracker* pItemTracker = QuestObjectiveManager()->GetTypedTracker< CQuestItemTracker* >( pItem->GetItemID() );
			InvalidReasonsContainer_t invalidReasons;
			if ( pItemTracker )
			{
				if ( pItemTracker->IsValidForPlayer( pLocalPlayer, invalidReasons ) == pItemTracker->GetTrackers().Count() )
				{
					++nNumInactive;
				}
			}
		}
	}
	
	const char* pszHeaderString = NULL;
	const char* pszCallToActionString = NULL;
	int nNumToShow = 0;
	if ( nNumCompleted > 0 )
	{
		nNumToShow = nNumCompleted;
		pszHeaderString = nNumToShow == 1 ? "#QuestTracker_Complete_Single" : "#QuestTracker_Complete_Multiple";
		pszCallToActionString = "QuestTracker_Complete";
	}
	else if ( nNumUnidentified > 0 )
	{
		nNumToShow = nNumUnidentified;
		pszHeaderString = nNumToShow == 1 ? "#QuestTracker_New_Single" : "#QuestTracker_New_Multiple";
		pszCallToActionString = "QuestTracker_New_CallToAction";
	}
	else if ( nNumInactive > 0 )
	{
		nNumToShow = nNumInactive;
		pszHeaderString = nNumToShow == 1 ? "#QuestTracker_Inactive_Single" : "#QuestTracker_Inactive_Multiple";
		pszCallToActionString = "QuestTracker_New_CallToAction";
	}

	bool bAnyStatusToShow = pszHeaderString != NULL;

	bool bShowExtras = bAnyStatusToShow && ( GetContractHUDVisibility() != CONTRACT_HUD_SHOW_ACTIVE );
	if ( bShowExtras )
	{
		// Build the "X New Contracts" string
		locchar_t locNumNewQuests[ 256 ];
		const locchar_t *pLocalizedString = GLocalizationProvider()->Find( pszHeaderString );
		if ( pLocalizedString && pLocalizedString[0] )
		{
			wchar_t wszCounter[ 256 ];
			loc_sprintf_safe( wszCounter, LOCCHAR( "%d" ), nNumToShow);
			loc_scpy_safe( locNumNewQuests,
							CConstructLocalizedString( pLocalizedString, wszCounter ) );
		}
	
		m_pStatusContainer->SetDialogVariable( "header", locNumNewQuests );

		// Build the "Press [ F2 ] to view" string.
		const wchar_t *pszText = NULL;
		if ( pszCallToActionString )
		{
			pszText = g_pVGuiLocalize->Find( pszCallToActionString );
		}
		if ( pszText )
		{					
			wchar_t wzFinal[512] = L"";
			UTIL_ReplaceKeyBindings( pszText, 0, wzFinal, sizeof( wzFinal ), GAME_ACTION_SET_FPSCONTROLS );
			m_pStatusContainer->SetDialogVariable( "call_to_action", wzFinal );
		}
	}
	
	int nY = 0;

	if ( m_pStatusContainer )
	{
		nY = m_pStatusContainer->GetYPos();

		m_pStatusContainer->SetVisible( bShowExtras );
		if ( m_pStatusContainer->IsVisible() )
		{
			nY += m_pStatusContainer->GetTall();
			int nLabelWide, nLabelTall;
			m_pStatusHeaderLabel->GetContentSize( nLabelWide, nLabelTall );
			
			m_pStatusContainer->SetWide( m_nStatusBufferWidth + nLabelWide );
			
			m_pStatusHeaderLabel->SetPos( m_pStatusContainer->GetWide() - m_pStatusHeaderLabel->GetWide(), m_pStatusHeaderLabel->GetYPos() );
			m_pCallToActionLabel->SetPos( m_pStatusContainer->GetWide() - m_pCallToActionLabel->GetWide(), m_pCallToActionLabel->GetYPos() );
			m_pStatusContainer->SetPos( m_pStatusContainer->GetParent()->GetWide() - m_pStatusContainer->GetWide(), m_pStatusContainer->GetYPos() );
		}
	}

	FOR_EACH_MAP( m_mapTrackers, i )
	{
		CItemTrackerPanel* pPanel = m_mapTrackers[ i ];
		if ( pPanel )
		{
			if ( pPanel->IsValidForLocalPlayer() )
			{
				int nTall = pPanel->GetContentTall();
				pPanel->SetPos( GetWide() - pPanel->GetWide(), nY );
				nY += nTall;
				pPanel->SetVisible( true );
			}
			else
			{
				pPanel->SetVisible( false );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudItemAttributeTracker::OnThink()
{
	BaseClass::OnThink();

	// If the specator GUI is up, we need to drop down a bit so that we're
	// not on top of the death notices
	int nY = g_pSpectatorGUI && g_pSpectatorGUI->IsVisible() ? g_pSpectatorGUI->GetTopBarHeight() : 0;
	int nCurrentX, nCurrentY;
	GetPos( nCurrentX, nCurrentY );
	if ( nCurrentY != nY )
	{
		SetPos( nCurrentX, nY );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudItemAttributeTracker::ShouldDraw( void )
{
	if ( engine->IsPlayingDemo() )
		return false;

	if ( GetContractHUDVisibility() == CONTRACT_HUD_SHOW_NONE )
		return false;

	if ( GetLocalPlayerTeam() < FIRST_GAME_TEAM )
		return false;

	if ( TFGameRules() )
	{
		if ( TFGameRules()->ShowMatchSummary() )
			return false;

		if ( TFGameRules()->GetRoundRestartTime() > -1.f )
		{
			float flTime = TFGameRules()->GetRoundRestartTime() - gpGlobals->curtime;
			if ( flTime <= 10.f )
				return false;
		}
	}

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudItemAttributeTracker::FireGameEvent( IGameEvent *pEvent )
{
	if ( FStrEq( pEvent->GetName(), "player_spawn" ) 
	  || FStrEq( pEvent->GetName(), "inventory_updated" ) 
	  || FStrEq( pEvent->GetName(), "client_disconnect" ) 
	  || FStrEq( pEvent->GetName(), "localplayer_changeclass" )
	  || FStrEq( pEvent->GetName(), "quest_objective_completed" ) )
	{
		InvalidateLayout();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudItemAttributeTracker::HandleSOEvent( const CSteamID & steamIDOwner, const CSharedObject *pObject, ETrackerHandling_t eHandling )
{
	// We only care about items!
	if( pObject->GetTypeID() != CEconItem::k_nTypeID )
		return;

	const CEconItem *pItem = (CEconItem *)pObject;

	// We only care about quests
	if ( pItem->GetItemDefinition()->GetQuestDef() == NULL )
		return;

	// We dont do anything with trackers for unidentified quests
	if ( IsQuestItemUnidentified( pItem ) )
		return;

	CItemTrackerPanel* pTracker = NULL;
	switch ( eHandling )
	{
	case TRACKER_CREATE:
	case TRACKER_UPDATE:
		FindTrackerForItem( pItem, &pTracker, true );
		if ( pTracker )
		{
			pTracker->SetItem( pItem );
		}
		break;
	case TRACKER_REMOVE:
		FindTrackerForItem( pItem, &pTracker, false );
		if ( pTracker )
		{
			m_mapTrackers.Remove( pItem->GetItemID() );
			pTracker->MarkForDeletion();
		}

		break;
	}

}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudItemAttributeTracker::FindTrackerForItem( const CEconItem* pItem, CItemTrackerPanel** ppTracker, bool bCreateIfNotFound )
{
	(*ppTracker) = NULL;
	bool bCreatedNew = false;

	auto idx = m_mapTrackers.Find( pItem->GetItemID() );
	if ( idx == m_mapTrackers.InvalidIndex() && bCreateIfNotFound )
	{
		(*ppTracker) = new CItemTrackerPanel( this
											, "ItemTrackerPanel"
											, pItem
											, pItem->GetItemDefinition()->GetQuestDef()->GetQuestTheme()->GetInGameTrackerResFile() );
		(*ppTracker)->InvalidateLayout( true, true );
		m_mapTrackers.Insert( pItem->GetItemID(), (*ppTracker) );
		bCreatedNew = true;
	}
	else if ( idx != m_mapTrackers.InvalidIndex() )
	{
		(*ppTracker) = m_mapTrackers[ idx ];
	}

	return bCreatedNew;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudItemAttributeTracker::LevelInit( void )
{
	// We want to listen for our player's SO cache updates
	if ( steamapicontext && steamapicontext->SteamUser() )
	{
		CSteamID steamID = steamapicontext->SteamUser()->GetSteamID();
		GCClientSystem()->GetGCClient()->AddSOCacheListener( steamID, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudItemAttributeTracker::LevelShutdown( void )
{
	if ( steamapicontext && steamapicontext->SteamUser() )
	{
		CSteamID steamID = steamapicontext->SteamUser()->GetSteamID();
		GCClientSystem()->GetGCClient()->RemoveSOCacheListener( steamID, this );
	}
}