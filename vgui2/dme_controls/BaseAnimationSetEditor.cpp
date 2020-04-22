//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/BaseAnimationSetEditor.h"
#include "tier1/KeyValues.h"
#include "tier2/fileutils.h"
#include "vgui_controls/Splitter.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ToggleButton.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/perforcefilelistframe.h"
#include "studio.h"
#include "dme_controls/BaseAnimSetAttributeSliderPanel.h"
#include "dme_controls/BaseAnimSetPresetFaderPanel.h"
#include "dme_controls/BaseAnimSetControlGroupPanel.h"
#include "dme_controls/dmecontrols_utils.h"
#include "dme_controls/dmepicker.h"

#include "sfmobjects/exportfacialanimation.h"

#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmeanimationlist.h"
#include "movieobjects/dmegamemodel.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT 38
#define ANIMATION_SET_EDITOR_BUTTONTRAY_YPOS 12
#define ANIMATION_SET_BUTTON_INSET 0

struct AnimSetLayout_t
{
	CBaseAnimationSetEditor::EAnimSetLayout_t type;
	const char *shortname;
	const char *contextmenulabel;
};

static AnimSetLayout_t g_AnimSetLayout[] = 
{
	{	CBaseAnimationSetEditor::LAYOUT_SPLIT,			"split",		"#BxAnimSetSplitLayout" },
	{	CBaseAnimationSetEditor::LAYOUT_VERTICAL,		"vertical",		"#BxAnimSetVerticalLayout" },
	{	CBaseAnimationSetEditor::LAYOUT_HORIZONTAL,		"horizontal",	"#BxAnimSetHorizontalLayout" },
};

static const char *NameForLayout( CBaseAnimationSetEditor::EAnimSetLayout_t layout, bool menu )
{
	int c = ARRAYSIZE( g_AnimSetLayout );
	for ( int i = 0; i < c; ++i )
	{
		const AnimSetLayout_t& data = g_AnimSetLayout[ i ];
		if ( data.type == layout )
		{
			return menu ? data.contextmenulabel : data.shortname;
		}
	}
	Assert( 0 );
	return menu ? g_AnimSetLayout[ 0 ].contextmenulabel : g_AnimSetLayout[ 0 ].shortname;
}

static CBaseAnimationSetEditor::EAnimSetLayout_t LayoutForName( const char *name )
{
	int c = ARRAYSIZE( g_AnimSetLayout );
	for ( int i = 0; i < c; ++i )
	{
		const AnimSetLayout_t& data = g_AnimSetLayout[ i ];
		if ( !Q_stricmp( data.shortname, name ) )
		{
			return data.type;
		}
	}

	Assert( 0 );
	return CBaseAnimationSetEditor::LAYOUT_SPLIT;
}

CBaseAnimationSetEditor::CBaseAnimationSetEditor( vgui::Panel *parent, const char *className, bool bShowGroups ) :
	BaseClass( parent, className ),
	m_Layout( LAYOUT_SPLIT ),
	m_Images( false )
{
	const char *modes[] =
	{
		"AS_OFF",
		"AS_PREVIEW",
		"AS_RECORD",
		"AS_PLAYBACK",
	};

	const char *imagefiles[] =
	{
		"tools/ifm/icon_recordingmode_off",
		"tools/ifm/icon_recordingmode_preview",
		"tools/ifm/icon_recordingmode_record",
		"tools/ifm/icon_recordingmode_playback",
	};

	int i;
	for ( i = 0 ; i < NUM_AS_RECORDING_STATES; ++i )
	{
		m_pState[ i ] = new ToggleButton( this, modes[ i ], "" );
		m_pState[ i ]->SetContentAlignment( Label::a_center );
		m_pState[ i ]->AddActionSignalTarget( this );
		m_pState[ i ]->SetVisible( bShowGroups );
		m_pState[ i ]->SetKeyBoardInputEnabled( false );
	}

	m_pSelectionModeType = new ToggleButton( this, "AnimSetSelectionModeType", "" );
	m_pSelectionModeType->SetContentAlignment( Label::a_center );
	m_pSelectionModeType->AddActionSignalTarget( this );
	m_pSelectionModeType->SetSelected( false );
	m_pSelectionModeType->SetKeyBoardInputEnabled( false );

	m_pComboBox = new ComboBox( this, "AnimationSets", 10, false );

	//	m_Images.AddImage( scheme()->GetImage( "tools/ifm/icon_lock", false ) );
	//	m_Images.AddImage( scheme()->GetImage( "tools/ifm/icon_eyedropper", false ) );
	//	m_Images.AddImage( scheme()->GetImage( "tools/ifm/icon_selectionmodeactive", false ) );
	m_Images.AddImage( scheme()->GetImage( "tools/ifm/icon_selectionmodeattached", false ) );
	for ( i = 0; i < NUM_AS_RECORDING_STATES; ++i )
	{
		m_Images.AddImage( scheme()->GetImage( imagefiles[ i ], false ) );
	}

	int w, h;
	m_Images.GetImage( 1 )->GetContentSize( w, h );
	m_Images.GetImage( 1 )->SetSize( w, h );
	m_Images.GetImage( 2 )->GetContentSize( w, h );
	m_Images.GetImage( 2 )->SetSize( w, h );

	// SETUP_PANEL( this );

	PostMessage( GetVPanel(), new KeyValues( "OnChangeLayout", "value", m_Layout ) );
	PostMessage( GetVPanel(), new KeyValues( "PopulateAnimationSetsChoice" ) );

	m_pSelectionModeType->SetVisible( bShowGroups );
	m_pComboBox->SetVisible( bShowGroups );

	SetRecordingState( bShowGroups ? AS_PLAYBACK : AS_PREVIEW, true );

	m_hFileOpenStateMachine = new vgui::FileOpenStateMachine( this, this );
	m_hFileOpenStateMachine->AddActionSignalTarget( this );
}

CBaseAnimationSetEditor::~CBaseAnimationSetEditor()
{
}

void CBaseAnimationSetEditor::CreateToolsSubPanels()
{
	m_hControlGroup = new CBaseAnimSetControlGroupPanel( (Panel *)NULL, "AnimSetControlGroup", this );
	m_hPresetFader = new CBaseAnimSetPresetFaderPanel( (Panel *)NULL, "AnimSetPresetFader", this );
	m_hAttributeSlider = new CBaseAnimSetAttributeSliderPanel( (Panel *)NULL, "AnimSetAttributeSliderPanel", this );
}

void CBaseAnimationSetEditor::OnButtonToggled( KeyValues *params )
{
	Panel *ptr = reinterpret_cast< Panel * >( params->GetPtr( "panel" ) );
	/*

	if ( ptr == m_pSelectionModeType )
	{
	// FIXME, could do this with MESSAGE_FUNC_PARAMS and look up "panel" ptr and compare to figure out which button was manipulated...
	g_pMovieMaker->SetTimeSelectionModeType( !m_pSelectionModeType->IsSelected() ? CIFMTool::MODE_DETACHED : CIFMTool::MODE_ATTACHED );
	}
	else
	*/
	{
		for ( int i = 0; i < NUM_AS_RECORDING_STATES; ++i )
		{
			if ( ptr == m_pState[ i ] )
			{
				SetRecordingState( (RecordingState_t)i, true );
				break;
			}
		}
	}
}

void CBaseAnimationSetEditor::ChangeLayout( EAnimSetLayout_t newLayout )
{
	int i;

	m_Layout = newLayout;

	// Make sure these don't get blown away...
	m_hControlGroup->SetParent( (Panel *)NULL );
	m_hPresetFader->SetParent( (Panel *)NULL );
	m_hAttributeSlider->SetParent( (Panel *)NULL );

	delete m_Splitter.Get();
	m_Splitter = NULL;

	CUtlVector< Panel * > list;
	list.AddToTail( m_hControlGroup.Get() );
	list.AddToTail( m_hPresetFader.Get() );
	list.AddToTail( m_hAttributeSlider.Get() );

	Splitter *sub = NULL;

	switch ( m_Layout )
	{
	default:
	case LAYOUT_SPLIT:
		{
			m_Splitter = new Splitter( this, "AnimSetEditorMainSplitter", SPLITTER_MODE_VERTICAL, 1 );
			m_Splitter->SetAutoResize
				( 
				Panel::PIN_TOPLEFT, 
				Panel::AUTORESIZE_DOWNANDRIGHT,
				0, ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT,
				0, 0
				);
			m_Splitter->SetBounds( 0, ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT, GetWide(), GetTall() - ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT );
			m_Splitter->SetSplitterColor( Color(32, 32, 32, 255) );

			// m_Splitter->EnableBorders( false );

			m_hControlGroup->SetParent( m_Splitter->GetChild( 0 ) );
			m_hControlGroup->SetAutoResize
				( 
				Panel::PIN_TOPLEFT, 
				Panel::AUTORESIZE_DOWNANDRIGHT,
				0, 0,
				0, 0
				);

			sub = new Splitter( m_Splitter->GetChild( 1 ), "AnimSetEditorSubSplitter", SPLITTER_MODE_HORIZONTAL, 1 );
			sub->SetAutoResize
				( 
				Panel::PIN_TOPLEFT, 
				Panel::AUTORESIZE_DOWNANDRIGHT,
				0, 0,
				0, 0
				);

			m_hPresetFader->SetParent( sub->GetChild( 0 ) );
			m_hPresetFader->SetAutoResize
				( 
				Panel::PIN_TOPLEFT, 
				Panel::AUTORESIZE_DOWNANDRIGHT,
				0, 0,
				0, 0
				);
			m_hAttributeSlider->SetParent( sub->GetChild( 1 ) );
			m_hAttributeSlider->SetAutoResize
				( 
				Panel::PIN_TOPLEFT, 
				Panel::AUTORESIZE_DOWNANDRIGHT,
				0, 0,
				0, 0
				);
		}
		break;
	case LAYOUT_VERTICAL:
		{
			m_Splitter = new Splitter( this, "AnimSetEditorMainSplitter", SPLITTER_MODE_VERTICAL, 2 );
			m_Splitter->SetSplitterColor( Color(32, 32, 32, 255) );
			m_Splitter->SetAutoResize
				( 
				Panel::PIN_TOPLEFT, 
				Panel::AUTORESIZE_DOWNANDRIGHT,
				0, ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT,
				0, 0
				);
			m_Splitter->SetBounds( 0, ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT, GetWide(), GetTall() - ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT );

			for ( i = 0; i < list.Count(); ++i )
			{
				list[ i ]->SetParent( m_Splitter->GetChild( i ) );
				list[ i ]->SetSize( m_Splitter->GetChild( i )->GetWide(), m_Splitter->GetChild( i )->GetTall() );
				list[ i ]->SetAutoResize
					( 
					Panel::PIN_TOPLEFT, 
					Panel::AUTORESIZE_DOWNANDRIGHT,
					0, 0,
					0, 0
					);
			}

			m_Splitter->EvenlyRespaceSplitters();
		}
		break;
	case LAYOUT_HORIZONTAL:
		{
			m_Splitter = new Splitter( this, "AnimSetEditorMainSplitter", SPLITTER_MODE_HORIZONTAL, 2 );
			m_Splitter->SetSplitterColor( Color(32, 32, 32, 255) );
			m_Splitter->SetAutoResize
				( 
				Panel::PIN_TOPLEFT, 
				Panel::AUTORESIZE_DOWNANDRIGHT,
				0, ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT,
				0, 0
				);

			m_Splitter->SetBounds( 0, ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT, GetWide(), GetTall() - ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT );

			for ( i = 0; i < list.Count(); ++i )
			{
				list[ i ]->SetParent( m_Splitter->GetChild( i ) );
				list[ i ]->SetSize( m_Splitter->GetChild( i )->GetWide(), m_Splitter->GetChild( i )->GetTall() );
				list[ i ]->SetAutoResize
					( 
					Panel::PIN_TOPLEFT, 
					Panel::AUTORESIZE_DOWNANDRIGHT,
					0, 0,
					0, 0
					);
			}

			m_Splitter->EvenlyRespaceSplitters();
		}
		break;
	}

	if ( sub )
	{
		sub->OnSizeChanged( sub->GetWide(), sub->GetTall() );
		sub->EvenlyRespaceSplitters();
	}
}

void CBaseAnimationSetEditor::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );

	int ypos = ANIMATION_SET_EDITOR_BUTTONTRAY_YPOS;

	int xpos = w - 25;
	m_pSelectionModeType->SetBounds( xpos, ypos, 20, 20 );
	for ( int i = NUM_AS_RECORDING_STATES - 1; i >= 0 ; --i  )
	{
		xpos -= 23;
		m_pState[ i ]->SetBounds( xpos, ypos, 20, 20 );
	}

	m_pComboBox->SetBounds( 10, ypos, xpos - 10- 5, 20 );
}

void CBaseAnimationSetEditor::OnChangeLayout( int value )
{
	ChangeLayout( ( EAnimSetLayout_t )value );
}


//-----------------------------------------------------------------------------
// Finds a channel in the animation set to overwrite with import data
//-----------------------------------------------------------------------------
CDmeChannel* CBaseAnimationSetEditor::FindImportChannel( CDmeChannel *pChannel, CDmeChannelsClip *pChannelsClip )
{
	CDmElement *pTargetElement = pChannel->GetToElement();
	if ( !pTargetElement )
		return NULL;

	CDmAttribute *pTargetAttribute = pChannel->GetToAttribute();
	if ( !pTargetAttribute )
		return NULL;

	const char *pTarget = pTargetAttribute->GetName();
	const char *pTargetName = pTargetElement->GetName();
	CDmeLog *pTargetLog = pChannel->GetLog();

	int nCount = pChannelsClip->m_Channels.Count();
	for ( int j = 0; j < nCount; ++j )
	{
		CDmeChannel *pImportChannel = pChannelsClip->m_Channels[j];
		if ( !pImportChannel )
			continue;

		CDmeLog *pImportLog = pImportChannel->GetLog();
		if ( !pImportLog )
			continue;

		if ( pTargetLog && ( pImportLog->GetType() != pTargetLog->GetType() ) )
			continue;

		if ( !pImportChannel->GetToAttribute() )
			continue;

		const char *pImportTarget = pImportChannel->GetToAttribute()->GetName();

		// Attribute to write into has to match exactly
		if ( Q_stricmp( pTarget, pImportTarget ) )
			continue;

		CDmElement *pImportTargetElement = pImportChannel->GetToElement();
		const char *pImportName = pImportTargetElement->GetName();
		
		// Element name has to match exactly or be of the form *(channel name)*
		if ( !Q_stricmp( pTargetName, pImportName ) )
			return pImportChannel;

		char pTemp[512];
		const char *pParen = strrchr( pTargetName, '(' );
		if ( !pParen )
			continue;
		Q_strncpy( pTemp, pParen+1, sizeof(pTemp) );
		char *pParen2 = strchr( pTemp, ')' );
		if ( !pParen2 )
			continue;
		*pParen2 = 0;
		if ( !Q_stricmp( pImportName, pTemp ) )
			return pImportChannel;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Transforms an imported channel, if necessary
//-----------------------------------------------------------------------------
void CBaseAnimationSetEditor::TransformImportedChannel( CDmeChannel *pChannel )
{
	CDmElement *pTarget = pChannel->GetToElement();
	const static UtlSymId_t symBones = g_pDataModel->GetSymbol( "bones" );
	CDmeGameModel *pGameModel = FindReferringElement<CDmeGameModel>( pTarget, symBones );
	if ( !pGameModel )
		return;

	int nBoneIndex = pGameModel->FindBone( CastElement< CDmeTransform >( pTarget ) );
	if ( nBoneIndex < 0 )
		return;

	// If we've got logs that have been imported, we need to compute our bounds.
	pGameModel->m_bComputeBounds = true;

	DmAttributeType_t logType = pChannel->GetLog()->GetDataType();
	int nLayerCount = pChannel->GetLog()->GetNumLayers();

	bool bHasPreTransform = false;
	bool bHasPostTransform = false;

	matrix3x4_t preTransform, postTransform;
	if ( pGameModel->GetSrcBoneTransforms( &preTransform, &postTransform, nBoneIndex ) )
	{
		bHasPreTransform = true;
		bHasPostTransform = true;
	}

	if ( pGameModel->IsRootTransform( nBoneIndex ) )
	{
		// NOTE: Root transforms require a pre-multiply of log data

		// Deal with the 'up axis' rotation
		matrix3x4_t rootTransform;
		RadianEuler angles( M_PI / 2.0f, 0.0f, M_PI / 2.0f );
		if ( bHasPreTransform )
		{
			AngleMatrix( angles, rootTransform );
			ConcatTransforms( rootTransform, preTransform, preTransform );
		}
		else
		{
			AngleMatrix( angles, preTransform );
		}
		bHasPreTransform = true;
	}

	if ( !bHasPreTransform && !bHasPostTransform )
		return;

	for ( int i = 0; i < nLayerCount; ++i )
	{
		if ( logType == AT_VECTOR3 )
		{
			CDmeVector3LogLayer *pPositionLog = CastElement< CDmeVector3LogLayer >( pChannel->GetLog()->GetLayer( i ) );
			if ( bHasPreTransform )
			{
				RotatePositionLog( pPositionLog, preTransform );
			}

#ifdef _DEBUG
			// At the moment, we don't support anything but prerotation.
			// This would be tricky because we'd need to read the quat logs
			// to figure out how to translate in local space.
			if ( bHasPostTransform )
			{
				Assert( fabs( postTransform[0][3] ) < 1e-3 && fabs( postTransform[1][3] ) < 1e-3 && fabs( postTransform[2][3] ) < 1e-3 );
			}
#endif
		}
		else
		{
			CDmeQuaternionLogLayer *pOrientationLog = CastElement< CDmeQuaternionLogLayer >( pChannel->GetLog()->GetLayer( i ) );
			if ( bHasPreTransform )
			{
				RotateOrientationLog( pOrientationLog, preTransform, true );
			}
			if ( bHasPostTransform )
			{
				RotateOrientationLog( pOrientationLog, postTransform, false );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Expands channels clip time to encompass log
//-----------------------------------------------------------------------------
void CBaseAnimationSetEditor::FixupChannelsClipTime( CDmeChannelsClip *pChannelsClip, CDmeLog *pLog )
{
	// Expand the channels clip to include the entire channel
	DmeTime_t st = pLog->GetBeginTime();
	DmeTime_t et = pLog->GetEndTime();
	st = pChannelsClip->FromChildMediaTime( st, false );
	et = pChannelsClip->FromChildMediaTime( et, false );
	if ( et < pChannelsClip->GetEndTime() )
	{
		et = pChannelsClip->GetEndTime();
	}
	if ( st < pChannelsClip->GetStartTime() )
	{
		DmeTime_t tDelta = pChannelsClip->GetStartTime() - st;
		DmeTime_t tOffset = pChannelsClip->GetTimeOffset();
		pChannelsClip->SetStartTime( st );
		pChannelsClip->SetTimeOffset( tOffset - tDelta );
	}
	else
	{
		st = pChannelsClip->GetStartTime();
	}
	DmeTime_t duration = et - st;
	if ( duration > pChannelsClip->GetDuration() )
	{
		pChannelsClip->SetDuration( duration );
	}
}


//-----------------------------------------------------------------------------
// Expands channels clip time to encompass log
//-----------------------------------------------------------------------------
void CBaseAnimationSetEditor::FixupChannelsClipTime( CDmeChannel *pChannel, CDmeLog *pLog )
{
	CUtlVector< CDmeChannelsClip* > clips;
	FindAncestorsReferencingElement( pChannel, clips );
	int nCount = clips.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		FixupChannelsClipTime( clips[i], pLog );
	}
}


//-----------------------------------------------------------------------------
// Imports a specific channels clip into the animation set
//-----------------------------------------------------------------------------
void CBaseAnimationSetEditor::OnImportConfirmed( KeyValues *pParams )
{
	KeyValues *pImportParams = pParams->FindKey( "context" );
	CDmeChannelsClip *pChannelsClip = GetElementKeyValue< CDmeChannelsClip >( pImportParams, "channelsClip" );
	if ( pParams->GetInt( "operationPerformed" ) == 0 )
	{
		CDisableUndoScopeGuard sg;
		g_pDataModel->RemoveFileId( pChannelsClip->GetFileId() );
		return;
	}

	bool bVisibleOnly = pImportParams->GetInt( "visibleOnly" ) != 0;

	CUtlVector< LogPreview_t > controls;
	int nCount = bVisibleOnly ? BuildVisibleControlList( controls ) : BuildFullControlList( controls );

	CUndoScopeGuard guard( "Import Animation" );
	for ( int i = 0; i < nCount; ++i )
	{
		for ( int k = 0; k < LOG_PREVIEW_MAX_CHANNEL_COUNT; ++k )
		{
			CDmeChannel *pChannel = controls[i].m_hChannels[k];
			if ( !pChannel )
				continue;

			CDmeChannel *pImportChannel = FindImportChannel( pChannel, pChannelsClip );
			if ( !pImportChannel )
				continue;

			// Switch the log over
			CDmeLog *pImportLog = pImportChannel->GetLog();
			pChannel->SetLog( pImportLog );
			pImportChannel->SetLog( NULL );
			pImportLog->SetFileId( pChannel->GetFileId(), TD_DEEP );

			TransformImportedChannel( pChannel );

			// Expand the channels clip to include the entire channel
			FixupChannelsClipTime( pChannel, pChannel->GetLog() );
		}
	}
	guard.Release();

	// Cleanup the file
	CDisableUndoScopeGuard sg;
	g_pDataModel->RemoveFileId( pChannelsClip->GetFileId() );
}


//-----------------------------------------------------------------------------
// Imports a specific channels clip into the animation set
//-----------------------------------------------------------------------------
void CBaseAnimationSetEditor::ImportAnimation( CDmeChannelsClip *pChannelsClip, bool bVisibleOnly )
{
	CUtlVector< LogPreview_t > controls;
	int nCount = bVisibleOnly ? BuildVisibleControlList( controls ) : BuildFullControlList( controls );

	COperationFileListFrame *pStatusFrame = new COperationFileListFrame( this, 
		"Import the Following Channels?", "Target Control", false );
	pStatusFrame->SetCloseButtonVisible( false );
	pStatusFrame->SetOperationColumnHeaderText( "Source Channel" );

	int nSrcCount = pChannelsClip->m_Channels.Count();
	CDmeChannel** ppFoundChannels = (CDmeChannel**)_alloca( nSrcCount * sizeof(CDmeChannel*) );
	int nFoundCount = 0;

	for ( int i = 0; i < nCount; ++i )
	{
		for ( int k = 0; k < LOG_PREVIEW_MAX_CHANNEL_COUNT; ++k )
		{
			CDmeChannel *pChannel = controls[i].m_hChannels[k];
			if ( !pChannel || pChannel->GetToElement() == NULL )
				continue;

			char pChannelInfo[512];
			Q_snprintf( pChannelInfo, sizeof(pChannelInfo), "\"%s\" : %s", 
				pChannel->GetToElement()->GetName(), pChannel->GetToAttribute()->GetName() );

			CDmeChannel *pImportChannel = FindImportChannel( pChannel, pChannelsClip );
			if ( !pImportChannel )
			{
				pStatusFrame->AddOperation( "No source channel", pChannelInfo, Color( 255, 0, 0, 255 ) ); 
				continue;
			}

			ppFoundChannels[nFoundCount++] = pImportChannel;

			char pImportInfo[512];
			Q_snprintf( pImportInfo, sizeof(pImportInfo), "\"%s\" : %s", 
				pImportChannel->GetToElement()->GetName(), pImportChannel->GetToAttribute()->GetName() );
			pStatusFrame->AddOperation( pImportInfo, pChannelInfo, Color( 0, 255, 0, 255 ) );
		}
	}

	for ( int i = 0; i < nSrcCount; ++i )
	{
		CDmeChannel *pMissingChannel  = pChannelsClip->m_Channels[i];

		int j;
		for ( j = 0; j < nFoundCount; ++j )
		{
			if ( ppFoundChannels[j] == pMissingChannel )
				break;
		}

		if ( j != nFoundCount )
			continue;

		char pImportInfo[512];
		Q_snprintf( pImportInfo, sizeof(pImportInfo), "\"%s\" : %s", 
			pMissingChannel->GetToElement()->GetName(), pMissingChannel->GetToAttribute()->GetName() );
		pStatusFrame->AddOperation( pImportInfo, "No destination control", Color( 255, 255, 0, 255 ) );
	}

	KeyValues *pContext = new KeyValues( "context" );
	SetElementKeyValue( pContext, "channelsClip", pChannelsClip );
	pContext->SetInt( "visibleOnly", bVisibleOnly );
	pStatusFrame->DoModal( pContext, "ImportConfirmed" );
}


//-----------------------------------------------------------------------------
// Called by CDmePickerFrame in SelectImportAnimation
//-----------------------------------------------------------------------------
void CBaseAnimationSetEditor::OnImportAnimationSelected( KeyValues *pParams )
{
	KeyValues *pContextKeyValues = pParams->FindKey( "context" );
	CDmeChannelsClip *pChannelsClip = GetElementKeyValue< CDmeChannelsClip >( pParams, "dme" );
	if ( pChannelsClip )
	{
		bool bVisibleOnly = pContextKeyValues->GetInt( "visibleOnly" ) != 0;
		ImportAnimation( pChannelsClip, bVisibleOnly );
	}
	else
	{
		OnImportAnimationCancelled( pParams );
	}
}


//-----------------------------------------------------------------------------
// Called by CDmePickerFrame in SelectImportAnimation
//-----------------------------------------------------------------------------
void CBaseAnimationSetEditor::OnImportAnimationCancelled( KeyValues *pParams )
{
	KeyValues *pContextKeyValues = pParams->FindKey( "context" );
	CDmElement *pAnimationList = GetElementKeyValue<CDmElement>( pContextKeyValues, "animationList" );
		
	// Cleanup the file
	if ( pAnimationList )
	{
		CDisableUndoScopeGuard sg;
		g_pDataModel->RemoveFileId( pAnimationList->GetFileId() );
	}
}


//-----------------------------------------------------------------------------
// Selects an animation to import
//-----------------------------------------------------------------------------
void CBaseAnimationSetEditor::SelectImportAnimation( CDmeAnimationList *pAnimationList, bool bVisibleOnly )
{
	KeyValues *pContextKeyValues = new KeyValues( "context" );
	SetElementKeyValue( pContextKeyValues, "animationList", pAnimationList );
	pContextKeyValues->SetInt( "visibleOnly", bVisibleOnly );

	int nCount = pAnimationList->GetAnimationCount();
	CUtlVector< DmePickerInfo_t > choices( 0, nCount );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeChannelsClip *pAnimation = pAnimationList->GetAnimation( i );
		if ( !pAnimation )
			continue;

		int j = choices.AddToTail();
		DmePickerInfo_t& info = choices[j];
		info.m_hElement = pAnimation->GetHandle();
		info.m_pChoiceString = pAnimation->GetName();
	}

	CDmePickerFrame *pAnimationPicker = new CDmePickerFrame( this, "Select Animation To Import" );
	pAnimationPicker->DoModal( choices, pContextKeyValues );
}


//-----------------------------------------------------------------------------
// Called by the FileOpenDialog in OnImportAnimation
//-----------------------------------------------------------------------------
void CBaseAnimationSetEditor::OnFileSelected( KeyValues *kv )
{
	KeyValues *pContextKeyValues = kv->FindKey( "ImportAnimation" );
	if ( !pContextKeyValues )
		return;

	bool bVisibleOnly = pContextKeyValues->GetInt( "visibleOnly" );
	if ( bVisibleOnly )
	{
		CUtlVector< LogPreview_t > controls;
		int nCount = BuildVisibleControlList( controls );
		if ( nCount == 0 )
		{
			vgui::MessageBox *pMessageBox = new vgui::MessageBox( "Error Importing Animations\n", 
				"Cannot import because there are no visible controls!\n", GetParent() );
			pMessageBox->DoModal( );
			return;
		}
	}

	const char *pFileName = kv->GetString( "fullpath", NULL );
	if ( !pFileName )
		return;

	CDmElement *pRoot;
	CDisableUndoScopeGuard guard;
	DmFileId_t fileId = g_pDataModel->RestoreFromFile( pFileName, NULL, NULL, &pRoot, CR_FORCE_COPY );
	guard.Release();

	if ( fileId == DMFILEID_INVALID )
		return;

	CDmeChannelsClip *pChannelsClip = CastElement< CDmeChannelsClip >( pRoot );
	if ( pChannelsClip )
	{
		ImportAnimation( pChannelsClip, bVisibleOnly );
		return;
	}

	CDmeAnimationList* pAnimationList = CastElement< CDmeAnimationList >( pRoot );
	if ( !pAnimationList )
	{
		pAnimationList = pRoot->GetValueElement< CDmeAnimationList >( "animationList" );
	}

	if ( !pAnimationList || pAnimationList->GetAnimationCount() == 0 )
	{
		char pBuf[1024];
		Q_snprintf( pBuf, sizeof(pBuf), "File \"%s\" contains no animations!\n", pFileName ); 
		vgui::MessageBox *pMessageBox = new vgui::MessageBox( "Error Importing Animations\n", pBuf, GetParent() );
		pMessageBox->DoModal( );

		CDisableUndoScopeGuard sg;
		g_pDataModel->RemoveFileId( pRoot->GetFileId() );
		sg.Release();
		return;
	}

	if ( pAnimationList->GetAnimationCount() == 1 )
	{
		ImportAnimation( pAnimationList->GetAnimation( 0 ), bVisibleOnly );
	}
	else
	{
		SelectImportAnimation( pAnimationList, bVisibleOnly );
	}
}


//-----------------------------------------------------------------------------
// Called by the context menu to import animation files
//-----------------------------------------------------------------------------
void CBaseAnimationSetEditor::OnImportAnimation( KeyValues *pParams )
{
	// Compute starting directory
	CDmeGameModel *pGameModel = m_AnimSet->GetValueElement< CDmeGameModel >( "gameModel" );

	char pStartingDir[ MAX_PATH ];
	if ( !pGameModel )
	{
		GetModContentSubdirectory( "models", pStartingDir, sizeof(pStartingDir) );
	}
	else
	{
		char pModelName[ MAX_PATH ];
		studiohdr_t *pStudioHdr = pGameModel->GetStudioHdr();
		Q_StripExtension( pStudioHdr->pszName(), pModelName, sizeof(pModelName) );

		char pRelativePath[ MAX_PATH ];
		Q_snprintf( pRelativePath, sizeof(pRelativePath), "models/%s/animations/dmx", pModelName );
		GetModContentSubdirectory( pRelativePath, pStartingDir, sizeof(pStartingDir) );
		if ( !g_pFullFileSystem->IsDirectory( pStartingDir ) )
		{
			Q_snprintf( pRelativePath, sizeof(pRelativePath), "models/%s", pModelName );
			GetModContentSubdirectory( pRelativePath, pStartingDir, sizeof(pStartingDir) );
			if ( !g_pFullFileSystem->IsDirectory( pStartingDir ) )
			{
				GetModContentSubdirectory( "models", pStartingDir, sizeof(pStartingDir) );
			}
		}
	}

	KeyValues *pContextKeyValues = new KeyValues( "ImportAnimation", "visibleOnly", pParams->GetInt( "visibleOnly" ) );
	FileOpenDialog *pDialog = new FileOpenDialog( this, "Select Animation File Name", true, pContextKeyValues );
	pDialog->SetStartDirectoryContext( "animation_set_import_animation", pStartingDir );
	pDialog->AddFilter( "*.*", "All Files (*.*)", false );
	pDialog->AddFilter( "*.dmx", "Animation file (*.dmx)", true );
	pDialog->SetDeleteSelfOnClose( true );
	pDialog->AddActionSignalTarget( this );
	pDialog->DoModal( );
}


//-----------------------------------------------------------------------------
// Main entry point for exporting facial animation
//-----------------------------------------------------------------------------
void CBaseAnimationSetEditor::SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, 
	bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	// Compute starting directory
	char pStartingDir[ MAX_PATH ];
	GetModSubdirectory( "scenes", pStartingDir, sizeof(pStartingDir) );

	Assert( !bOpenFile );
	pDialog->SetTitle( "Save Facial Animation As", true );

	Assert( !V_strcmp( pFileFormat, "facial_animation" ) );
	pDialog->SetStartDirectoryContext( "facial_animation_export", pStartingDir );
	pDialog->AddFilter( "*.*", "All Files (*.*)", false );
	pDialog->AddFilter( "*.dmx", "Facial animation file (*.dmx)", true, pFileFormat );
}

bool CBaseAnimationSetEditor::OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	Assert( 0 );
	return false;
}


//-----------------------------------------------------------------------------
// Called when it's time to write the file
//-----------------------------------------------------------------------------
bool CBaseAnimationSetEditor::OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	// Recompute relative paths for each source now that we know the file name
	// NOTE: This also updates the name of the fileID in the datamodel system
	CDisableUndoScopeGuard guard;
	bool bOk = ExportFacialAnimation( pFileName, GetRootClip(), GetAnimationSetClip(), m_AnimSet ); 
	return bOk;
}


//-----------------------------------------------------------------------------
// Main entry point for exporting facial animation
//-----------------------------------------------------------------------------
void CBaseAnimationSetEditor::OnExportFacialAnimation()
{
	KeyValues *pContextKeyValues = new KeyValues( "ExportFacialAnimation" );
	m_hFileOpenStateMachine->SaveFile( pContextKeyValues, NULL, "facial_animation", FOSM_SHOW_PERFORCE_DIALOGS );
}


void CBaseAnimationSetEditor::OnOpenContextMenu( KeyValues *params )
{
	if ( m_hContextMenu.Get() )
	{
		delete m_hContextMenu.Get();
		m_hContextMenu = NULL;
	}

	m_hContextMenu = new Menu( this, "ActionMenu" );

	int c = ARRAYSIZE( g_AnimSetLayout );
	for ( int i = 0; i < c; ++i )
	{
		const AnimSetLayout_t& data = g_AnimSetLayout[ i ];

		m_hContextMenu->AddMenuItem( data.contextmenulabel, new KeyValues( "OnChangeLayout", "value", (int)data.type ), this );
	}

	if ( m_AnimSet.Get() )
	{
		m_hContextMenu->AddSeparator( );
		m_hContextMenu->AddMenuItem( "#ImportAnimation", new KeyValues( "ImportAnimation" ), this );
		m_hContextMenu->AddMenuItem( "#ReattachToModel", new KeyValues( "ReattachToModel" ), this );
		m_hContextMenu->AddMenuItem( "#ExportFacialAnimation", new KeyValues( "ExportFacialAnimation" ), this );
	}

	Panel *rpanel = reinterpret_cast< Panel * >( params->GetPtr( "contextlabel" ) );
	if ( rpanel )
	{
		// force the menu to compute required width/height
		m_hContextMenu->PerformLayout();
		m_hContextMenu->PositionRelativeToPanel( rpanel, Menu::DOWN, 0, true );
	}
	else
	{
		Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
	}
}

void CBaseAnimationSetEditor::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// Have to manually apply settings here if they aren't attached in hierarchy
	if ( m_hControlGroup->GetParent() != this )
	{
		m_hControlGroup->ApplySchemeSettings( pScheme );
	}
	if ( m_hPresetFader->GetParent() != this )
	{
		m_hPresetFader->ApplySchemeSettings( pScheme );
	}
	if ( m_hAttributeSlider->GetParent() != this )
	{
		m_hAttributeSlider->ApplySchemeSettings( pScheme );
	}

	m_pSelectionModeType->ClearImages();
	m_pSelectionModeType->AddImage( m_Images.GetImage( 1 ), 0 );

	for ( int i = 0; i < NUM_AS_RECORDING_STATES; ++i )
	{
		m_pState[ i ]->ClearImages();
		m_pState[ i ]->AddImage( m_Images.GetImage( i + 2 ), 0 );
	}

	m_pComboBox->SetFont( pScheme->GetFont( "DefaultBold" ) );
}

CBaseAnimSetControlGroupPanel *CBaseAnimationSetEditor::GetControlGroup()
{
	return m_hControlGroup.Get();
}

CBaseAnimSetPresetFaderPanel *CBaseAnimationSetEditor::GetPresetFader()
{
	return m_hPresetFader.Get();
}

CBaseAnimSetAttributeSliderPanel *CBaseAnimationSetEditor::GetAttributeSlider()
{
	return m_hAttributeSlider.Get();
}



void CBaseAnimationSetEditor::ChangeAnimationSet( CDmeAnimationSet *newAnimSet )
{
	m_AnimSet = newAnimSet;

	if ( newAnimSet )
	{
		CUndoScopeGuard guard( "Auto-create Procedural Presets" );
		newAnimSet->EnsureProceduralPresets();
	}

	// send to sub controls
	m_hControlGroup->ChangeAnimationSet( newAnimSet );
	m_hPresetFader->ChangeAnimationSet( newAnimSet );
	m_hAttributeSlider->ChangeAnimationSet( newAnimSet );
}

void CBaseAnimationSetEditor::OnDataChanged()
{
}

void CBaseAnimationSetEditor::OnTextChanged()
{
	KeyValues *kv = m_pComboBox->GetActiveItemUserData();
	if ( !kv )
		return;

	CDmeAnimationSet *set = GetElementKeyValue< CDmeAnimationSet >( kv, "handle" );
	if ( set )
	{
		ChangeAnimationSet( set );
	}
}

void CBaseAnimationSetEditor::SetRecordingState( RecordingState_t state, bool /*updateSettings*/ )
{
	m_RecordingState = state;

	// Reset buttons as needed
	for ( int i = 0; i < NUM_AS_RECORDING_STATES; ++i )
	{
		if ( (RecordingState_t)i == state )
		{
			m_pState[ i ]->SetSelected( true );
			m_pState[ i ]->ForceDepressed( true );
		}
		else
		{
			m_pState[ i ]->SetSelected( false );
			m_pState[ i ]->ForceDepressed( false );
		}
	}
}

RecordingState_t CBaseAnimationSetEditor::GetRecordingState() const
{
	return m_RecordingState;
}

CDmeAnimationSet *CBaseAnimationSetEditor::GetAnimationSet()
{
	return m_AnimSet;
}

int CBaseAnimationSetEditor::BuildVisibleControlList( CUtlVector< LogPreview_t >& list )
{
	return m_hAttributeSlider->BuildVisibleControlList( list );
}

int CBaseAnimationSetEditor::BuildFullControlList( CUtlVector< LogPreview_t >& list )
{
	return m_hAttributeSlider->BuildFullControlList( list );
}

void CBaseAnimationSetEditor::RecomputePreview()
{
	m_hAttributeSlider->RecomputePreview();
}

