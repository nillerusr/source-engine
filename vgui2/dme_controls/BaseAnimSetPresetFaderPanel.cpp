//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "dme_controls/BaseAnimSetPresetFaderPanel.h"
#include "dme_controls/DmePresetGroupEditorPanel.h"
#include "vgui_controls/InputDialog.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Slider.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/PanelListPanel.h"
#include "movieobjects/dmeanimationset.h"
#include "tier1/KeyValues.h"
#include "dme_controls/dmecontrols_utils.h"
#include "vstdlib/random.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "dme_controls/BaseAnimSetAttributeSliderPanel.h"
#include "dme_controls/BaseAnimationSetEditor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

float ifm_fader_timescale = 5.0f;

class CPresetSlider;


//-----------------------------------------------------------------------------
// Purpose: Utility dialog, used to let user type in some text
//-----------------------------------------------------------------------------
class CAddPresetDialog : public vgui::BaseInputDialog
{
	DECLARE_CLASS_SIMPLE( CAddPresetDialog, vgui::BaseInputDialog );

public:
	CAddPresetDialog( vgui::Panel *parent );

	void DoModal( CDmeAnimationSet *pAnimationSet, KeyValues *pContextKeyValues = NULL );

protected:
	// command buttons
	virtual void OnCommand(const char *command);

private:
	vgui::TextEntry		*m_pInput;
	vgui::ComboBox		*m_pPresetGroup;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAddPresetDialog::CAddPresetDialog( vgui::Panel *parent ) : BaseClass( parent, "Enter Preset Name" )
{
	m_pInput = new TextEntry( this, "PresetName" );
	m_pPresetGroup = new vgui::ComboBox( this, "PresetGroup", 8, true );
	SetDeleteSelfOnClose( false );

	LoadControlSettings( "resource/addpresetdialog.res" );
}


void CAddPresetDialog::DoModal( CDmeAnimationSet *pAnimationSet, KeyValues *pContextKeyValues )
{
	int nTextLength = m_pInput->GetTextLength() + 1;
	char* pCurrentGroupName = (char*)_alloca( nTextLength * sizeof(char) );
	m_pInput->GetText( pCurrentGroupName, nTextLength );

	m_pPresetGroup->DeleteAllItems();

	// Populate the combo box with preset group names
	CDmrElementArray< CDmePresetGroup > presetGroupList = pAnimationSet->GetPresetGroups();
	int nCount = presetGroupList.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmePresetGroup *pPresetGroup = presetGroupList[i];
		if ( pPresetGroup->m_bIsReadOnly )
			continue;

		KeyValues *kv = new KeyValues( "entry" );
		SetElementKeyValue( kv, "presetGroup", pPresetGroup );
		int nItemID = m_pPresetGroup->AddItem( pPresetGroup->GetName(), kv );
		if ( pCurrentGroupName && !Q_stricmp( pPresetGroup->GetName(), pCurrentGroupName ) )
		{
			m_pPresetGroup->ActivateItem( nItemID );
		}
	}

	BaseClass::DoModal( pContextKeyValues );

	m_pInput->SetText( "" );
	m_pInput->RequestFocus();

	PlaceUnderCursor( );
}


//-----------------------------------------------------------------------------
// command handler
//-----------------------------------------------------------------------------
void CAddPresetDialog::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "OK" ) )
	{
		int nTextLength = m_pInput->GetTextLength() + 1;
		char* txt = (char*)_alloca( nTextLength * sizeof(char) );
		m_pInput->GetText( txt, nTextLength );

		nTextLength = m_pPresetGroup->GetTextLength() + 1;
		char* pPresetGroupName = (char*)_alloca( nTextLength * sizeof(char) );
		m_pPresetGroup->GetText( pPresetGroupName, nTextLength );

		KeyValues *pCurrentGroup = m_pPresetGroup->GetActiveItemUserData();
		CDmePresetGroup *pPresetGroup = pCurrentGroup ? GetElementKeyValue<CDmePresetGroup>( pCurrentGroup, "presetGroup" ) : NULL;
		if ( pPresetGroup && Q_stricmp( pPresetGroup->GetName(), pPresetGroupName ) )
		{
			pPresetGroup = NULL;
		}
		KeyValues *kv = new KeyValues( "PresetNameSelected", "text", txt );
		kv->SetString( "presetGroupName", pPresetGroupName );
		SetElementKeyValue( kv, "presetGroup", pPresetGroup );

		if ( m_pContextKeyValues )
		{
			kv->AddSubKey( m_pContextKeyValues );
			m_pContextKeyValues = NULL;
		}
		PostActionSignal( kv );
		CloseModal();
		return;
	}

	if ( !Q_stricmp( command, "Cancel") )
	{
		CloseModal();
		return;
	}

	BaseClass::OnCommand( command );
}



//-----------------------------------------------------------------------------
//
// CPresetSliderEdgeButton: The buttons that lie on either side of the PresetSlider
//
//-----------------------------------------------------------------------------
class CPresetSliderEdgeButton : public Button
{
	DECLARE_CLASS_SIMPLE( CPresetSliderEdgeButton, Button );
public:
	CPresetSliderEdgeButton( CPresetSlider *parent, const char *panelName, const char *text );

private:
	virtual void OnCursorMoved(int x, int y);
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseReleased( vgui::MouseCode code );
	virtual void OnMouseDoublePressed( vgui::MouseCode code );

	CPresetSlider	*m_pSlider;
};


//-----------------------------------------------------------------------------
//
// CPresetSlider: The actual preset slider itself!
//
//-----------------------------------------------------------------------------
class CPresetSlider : public Slider
{
	DECLARE_CLASS_SIMPLE( CPresetSlider, Slider );

public:

	friend class CPresetSliderEdgeButton;

	CPresetSlider( CBaseAnimSetPresetFaderPanel *parent, const char *panelName, CDmePreset *pPreset );
	~CPresetSlider();

	void		SetControlValues( );

	void		SetGradientColor( const Color& clr );

	float		GetCurrent();
	void		SetPos( float frac );

	AttributeDict_t *GetAttributeDict();

	bool			IsPreviewSlider();
	bool			IsDragging();

	void		UpdateProceduralValues();

	CDmePreset	*GetPreset();

protected:

	virtual void Paint();
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( IScheme *scheme );
	virtual void GetTrackRect( int &x, int &y, int &w, int &h );
	virtual void PerformLayout();
	virtual void OnMousePressed(MouseCode code);
	virtual void OnMouseDoublePressed(MouseCode code);
	virtual void OnMouseReleased(MouseCode code);
	virtual void OnKeyCodeTyped( KeyCode code );
	virtual void OnCursorMoved(int x, int y);

	MESSAGE_FUNC( OnShowContextMenu, "OnShowContextMenu" );
	MESSAGE_FUNC( OnRename, "OnRename" );
	MESSAGE_FUNC( OnDelete, "OnDelete" );

	MESSAGE_FUNC( OnOverwrite, "OnOverwrite" );

	MESSAGE_FUNC_PARAMS( OnInputCompleted, "InputCompleted", params );

	MESSAGE_FUNC( OnDeleteConfirmed, "OnDeleteConfirmed" );
	MESSAGE_FUNC( OnOverwriteConfirmed, "OnOverwriteConfirmed" );

private:
	void OnRenameCompleted( const char *pText, KeyValues *pContextKeyValues );

	void OnDragCompleted( float flValue );
	void UpdateTickPos( int x, int y );

	CBaseAnimSetPresetFaderPanel	*m_pParent;

	Color			m_GradientColor;
	Color			m_ZeroColor;
	Color			m_TextColor;
	Color			m_TextColorFocus;
	TextImage		*m_pName;
	float			m_flCurrent;

	bool			m_bSuppressCompletion;

	CPresetSliderEdgeButton			*m_pEdgeButtons[ 2 ];

	vgui::DHANDLE< vgui::Menu >	m_hContextMenu;
	vgui::DHANDLE< vgui::InputDialog >	m_hInputDialog;

	AttributeDict_t		m_AttributeLookup;

	vgui::DHANDLE< MessageBox >			m_hConfirm;
	CDmeHandle< CDmePreset >			m_hSelf;

	static bool s_bResetMousePosOnMouseUp;
	static int s_nMousePosX;
	static int s_nMousePosY;
};


//-----------------------------------------------------------------------------
//
// CPresetSliderEdgeButton: The buttons that lie on either side of the PresetSlider
//
//-----------------------------------------------------------------------------
CPresetSliderEdgeButton::CPresetSliderEdgeButton( CPresetSlider *parent, const char *panelName, const char *text ) :
	BaseClass( (Panel *)parent, panelName, text ), m_pSlider( parent )
{
	SetPaintBorderEnabled( false );
}

void CPresetSliderEdgeButton::OnCursorMoved(int x, int y)
{
	LocalToScreen( x, y );
	m_pSlider->ScreenToLocal( x, y );
	m_pSlider->OnCursorMoved( x, y );
}

void CPresetSliderEdgeButton::OnMousePressed( vgui::MouseCode code )
{
	BaseClass::OnMousePressed( code );
	PostMessage( m_pSlider->GetVPanel(), new KeyValues( "MousePressed", "code", code ) );
	PostMessage( m_pSlider->GetVPanel(), new KeyValues( "MouseReleased", "code", code ), 0.001f );
}

void CPresetSliderEdgeButton::OnMouseReleased( vgui::MouseCode code )
{
	BaseClass::OnMouseReleased( code );
	PostMessage( m_pSlider->GetVPanel(), new KeyValues( "MouseReleased", "code", code ) );
}

void CPresetSliderEdgeButton::OnMouseDoublePressed( vgui::MouseCode code )
{
	BaseClass::OnMouseDoublePressed( code );
	PostMessage( m_pSlider->GetVPanel(), new KeyValues( "MouseDoublePressed", "code", code ) );
	PostMessage( m_pSlider->GetVPanel(), new KeyValues( "MouseReleased", "code", code ), 0.001f );
}


//-----------------------------------------------------------------------------
//
// CPresetSlider: The actual preset slider itself!
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Static members
//-----------------------------------------------------------------------------
bool CPresetSlider::s_bResetMousePosOnMouseUp = false;
int CPresetSlider::s_nMousePosX;
int CPresetSlider::s_nMousePosY;


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CPresetSlider::CPresetSlider( CBaseAnimSetPresetFaderPanel *parent, const char *panelName, CDmePreset *preset ) :
	BaseClass( (Panel *)parent, panelName ), m_pParent( parent ), m_bSuppressCompletion( false )
{
	Assert( preset );
	m_hSelf = preset;

	SetRange( 0, 1000 );
	SetDragOnRepositionNob( true );

	SetPaintBackgroundEnabled( true );

	m_pName = new TextImage( panelName );

	m_pEdgeButtons[ 0 ] = new CPresetSliderEdgeButton( this, "PresetSliderLeftEdge", "" );
	m_pEdgeButtons[ 1 ] = new CPresetSliderEdgeButton( this, "PresetSliderRightEdge", "" );

	SetBgColor( Color( 128, 128, 128, 128 ) );

	m_ZeroColor = Color( 69, 69, 69, 255 );
	m_GradientColor = Color( 194, 120, 0, 255 );

	m_TextColor = Color( 200, 200, 200, 255 );
	m_TextColorFocus = Color( 208, 143, 40, 255 );
}

CPresetSlider::~CPresetSlider()
{
	delete m_pName;
}

CDmePreset *CPresetSlider::GetPreset()
{
	return m_hSelf;
}

// #define PRORCEDURAL_PRESET_TIMING

void CPresetSlider::UpdateProceduralValues()
{
	if ( !m_hSelf->IsProcedural() )
		return;
#if defined( PRORCEDURAL_PRESET_TIMING )
	double st = Plat_FloatTime();
#endif
	// Figure out what we need to do
	int nPresetType = m_hSelf->GetProceduralPresetType();
	switch ( nPresetType )
	{
	default:
		Assert( 0 );
		break;
	case PROCEDURAL_PRESET_REVEAL:
	case PROCEDURAL_PRESET_PASTE:
	case PROCEDURAL_PRESET_JITTER:
	case PROCEDURAL_PRESET_SMOOTH:
	case PROCEDURAL_PRESET_SHARPEN:
	case PROCEDURAL_PRESET_SOFTEN:
	case PROCEDURAL_PRESET_STAGGER:
		// These are handled elsewhere right now... at some point we'll copy in values at head position in order to do ctrl-key preview mode
		break;
	case PROCEDURAL_PRESET_IN_CROSSFADE:
		{
			m_pParent->ProceduralPreset_UpdateCrossfade( m_hSelf, true );
		}
		break;
	case PROCEDURAL_PRESET_OUT_CROSSFADE:
		{
			m_pParent->ProceduralPreset_UpdateCrossfade( m_hSelf, false );
		}
		break;
	}
#if defined( PRORCEDURAL_PRESET_TIMING )
	double ed = Plat_FloatTime();
	Msg( "Update %.3f msec\n", 1000.0 * ( ed - st ) );
#endif
}

//-----------------------------------------------------------------------------
// Reads the sliders, sets control values into the attribute dictionary
//-----------------------------------------------------------------------------
void CPresetSlider::SetControlValues( )
{
	m_AttributeLookup.Purge();
	if ( !m_hSelf.Get() )
		return;

	CDmrElementArray< CDmElement > values = m_hSelf->GetControlValues();

	int nControlValueCount = values.Count();
	for ( int i = 0; i < nControlValueCount; ++i )
	{
		CDmElement *v = values[ i ];

		AnimationControlAttributes_t val;
		val.m_pAttribute[ANIM_CONTROL_VALUE] = v->GetAttribute( "value" );
		val.m_pAttribute[ANIM_CONTROL_BALANCE] = v->GetAttribute( "balance" );
		val.m_pAttribute[ANIM_CONTROL_MULTILEVEL] = v->GetAttribute( "multilevel" );
		val.m_pValue[ANIM_CONTROL_VALUE] = v->GetValue< float >( "value" ); 
		val.m_pValue[ANIM_CONTROL_BALANCE]	= v->GetValue< float >( "balance" );
		val.m_pValue[ANIM_CONTROL_MULTILEVEL] = v->GetValue< float >( "multilevel" );

		m_AttributeLookup.Insert( v->GetName(), val );
	}
}


AttributeDict_t *CPresetSlider::GetAttributeDict()
{
	return &m_AttributeLookup;
}

void CPresetSlider::OnMouseDoublePressed(MouseCode code)
{
	if ( code != MOUSE_LEFT )
	{
		BaseClass::OnMouseDoublePressed( code );
		return;
	}
}

void CPresetSlider::OnMousePressed(MouseCode code)
{
	if ( code == MOUSE_RIGHT )
		return;

	BaseClass::OnMousePressed( code );

	if ( !_dragging )
		return;

	SetCursor( dc_blank );
	int mx, my;
	input()->GetCursorPos( mx, my );
	int tx, ty, tw, th;
	GetTrackRect( tx, ty, tw, th );

	ScreenToLocal( mx, my );

	// Off right?
	bool offright = mx >= ( tx + tw ) ? true : false;

	bool ctrldown = input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL );
	if ( !ctrldown && !offright )
	{
		if ( mx >= tx )
		{
			Assert( !s_bResetMousePosOnMouseUp );
			s_bResetMousePosOnMouseUp = true;
			s_nMousePosX = mx;
			s_nMousePosY = my;
			LocalToScreen( s_nMousePosX, s_nMousePosY );

			int offset = mx - tx;
			mx -= offset;
			LocalToScreen( mx, my );
			input()->SetCursorPos( mx, my );
		}
		SetPos( 0 );
	}
}

void CPresetSlider::OnMouseReleased(MouseCode code)
{
	if ( code == MOUSE_RIGHT )
	{
		OnShowContextMenu();
		return;
	}

	float flLastValue = GetCurrent();
	bool bWasDragging = _dragging;
	BaseClass::OnMouseReleased( code );
	if ( bWasDragging )
	{
		OnDragCompleted( flLastValue );
		SetCursor( dc_arrow );
	}

	if( s_bResetMousePosOnMouseUp )
	{
		s_bResetMousePosOnMouseUp = false;
		input()->SetCursorPos( s_nMousePosX, s_nMousePosY );
	}
}

void CPresetSlider::OnKeyCodeTyped( KeyCode code )
{
	if ( code != KEY_ESCAPE || !_dragging )
	{
		BaseClass::OnKeyCodeTyped( code );
		return;
	}

	m_bSuppressCompletion = true;
	OnMouseReleased( MOUSE_LEFT );
	m_bSuppressCompletion = false;
	SetCursor( dc_arrow );
}

void CPresetSlider::OnDragCompleted( float flValue )
{
	if ( m_bSuppressCompletion )
		return;

	char sz[ 128 ];
	m_pName->GetText( sz, sizeof( sz ) );
	// Msg( "CPresetSlider slider drag completed %s [%.3f ]\n", sz, flValue );
	// Apply settings to attribute sliders

	m_pParent->ApplyPreset( flValue, m_AttributeLookup );
}

void CPresetSlider::OnRename()
{
	if ( m_hInputDialog.Get() )
	{
		delete m_hInputDialog.Get();
	}

	m_hInputDialog = new InputDialog( this, "Rename Preset", "Name:", GetName() );
	if ( m_hInputDialog.Get() )
	{
		KeyValues *pContextKeyValues = new KeyValues( "RenamePreset" );
		m_hInputDialog->SetSmallCaption( true );
		m_hInputDialog->SetMultiline( false );
		m_hInputDialog->DoModal( pContextKeyValues );
	}
	else
	{
		Assert( 0 );
	}
}

void CPresetSlider::OnRenameCompleted( const char *pText, KeyValues *pContextKeyValues )
{
	if ( !pText || !*pText )
	{
		Warning( "Can't rename preset for %s to an empty name\n", GetName() );
		return;
	}

	// No change( case sensitive)
	if ( !Q_strcmp( GetName(), pText ) )
		return;

	CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Rename Preset" );

	SetName( pText );
	m_pName->SetText( pText );
	m_pName->ResizeImageToContent();

	// 
	Assert( m_hSelf.Get() );
	if ( m_hSelf.Get() )
	{
		m_hSelf->SetName( pText );
	}
}

void CPresetSlider::OnInputCompleted( KeyValues *pParams )
{
	const char *pText = pParams->GetString( "text", NULL );

	KeyValues *pContextKeyValues = pParams->FindKey( "RenamePreset" );
	if ( pContextKeyValues )
	{
		OnRenameCompleted( pText, pContextKeyValues );
		return;
	}

	Assert( 0 );
}

void CPresetSlider::OnDelete()
{
	if ( m_hConfirm.Get() )
		delete m_hConfirm.Get();

	char sz[ 256 ];
	Q_snprintf( sz, sizeof( sz ), "Delete '%s'?", GetName() );

	m_hConfirm = new MessageBox( "Delete Preset", sz, this );
	Assert( m_hConfirm.Get() );
	if ( m_hConfirm )
	{
		m_hConfirm->SetCancelButtonVisible( true );
		m_hConfirm->SetCancelButtonText( "#VGui_Cancel" );
		m_hConfirm->SetCommand( new KeyValues( "OnDeleteConfirmed" ) );
		m_hConfirm->AddActionSignalTarget( this );
		m_hConfirm->DoModal();
	}
}

void CPresetSlider::OnDeleteConfirmed()
{
	m_pParent->OnDeletePreset( m_hSelf.Get() );
}

void CPresetSlider::OnOverwrite()
{
	if ( m_hConfirm.Get() )
		delete m_hConfirm.Get();

	char sz[ 256 ];
	Q_snprintf( sz, sizeof( sz ), "Overwrite '%s'?", GetName() );

	m_hConfirm = new MessageBox( "Overwrite Preset", sz, this );
	Assert( m_hConfirm.Get() );
	if ( m_hConfirm )
	{
		m_hConfirm->ShowMessageBoxOverCursor( true );
		m_hConfirm->SetCancelButtonVisible( true );
		m_hConfirm->SetCancelButtonText( "#VGui_Cancel" );
		m_hConfirm->SetCommand( new KeyValues( "OnOverwriteConfirmed" ) );
		m_hConfirm->AddActionSignalTarget( this );
		m_hConfirm->DoModal();
	}
}

void CPresetSlider::OnOverwriteConfirmed()
{
	m_pParent->OnOverwritePreset( m_hSelf.Get() );
}

void CPresetSlider::OnShowContextMenu()
{
	if ( m_hContextMenu.Get() )
	{
		delete m_hContextMenu.Get();
		m_hContextMenu = NULL;
	}

	m_hContextMenu = new Menu( this, "ActionMenu" );

	bool bIsReadOnly = m_hSelf.Get() ? m_hSelf->IsReadOnly() : false;
	bool bCanOverwrite = !bIsReadOnly;
	if ( m_hSelf->IsProcedural() )
	{
		switch ( m_hSelf->GetProceduralPresetType() )
		{
		default:
			break;
		case PROCEDURAL_PRESET_REVEAL:
			bCanOverwrite = true;
			break;
		}
	}

	if ( bCanOverwrite )
	{
		m_hContextMenu->AddMenuItem( "Overwrite", new KeyValues( "OnOverwrite" ), this );
		m_hContextMenu->AddSeparator();
	}
	if ( !bIsReadOnly )
	{
		m_hContextMenu->AddMenuItem( "Rename...", new KeyValues( "OnRename" ), this );
		if ( Q_stricmp( GetName(), "Default" ) )
		{
			m_hContextMenu->AddMenuItem( "Delete...", new KeyValues( "OnDelete" ), this );
		}

		m_hContextMenu->AddSeparator();
	}

	m_hContextMenu->AddMenuItem( "Add...", new KeyValues( "AddPreset" ), m_pParent );
	m_hContextMenu->AddMenuItem( "Change Crossfade Speed...", new KeyValues( "SetPresetCrossfadeSpeed" ), m_pParent );

	m_hContextMenu->AddSeparator();

	m_hContextMenu->AddMenuItem( "Manage...", new KeyValues( "ManagePresets" ), m_pParent );

	Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}

void CPresetSlider::UpdateTickPos( int x, int y )
{
	int tx, ty, tw, th;
	GetTrackRect( tx, ty, tw, th );
	 
	bool bIsCtrlKeyDown = vgui::input()->IsKeyDown( KEY_LCONTROL ) || vgui::input()->IsKeyDown( KEY_RCONTROL );
	bool bIsAltKeyDown = vgui::input()->IsKeyDown( KEY_LALT ) || vgui::input()->IsKeyDown( KEY_RALT );
	if ( bIsCtrlKeyDown && bIsAltKeyDown && !_dragging )
	{
		x = tx + tw;
	}

	float previewValue = 0.0f;
	if ( x > tx )
	{
		if ( x >= ( tx + tw ) || tw <= 0 )
		{
			previewValue = 1.0f;
		}
		else
		{
			previewValue = (float)( x - tx ) / (float)tw;
		}
	}

	SetPos( previewValue );
}

void CPresetSlider::OnCursorMoved(int x, int y)
{
	UpdateTickPos( x, y );
}

bool CPresetSlider::IsDragging()
{
	return _dragging;
}

bool CPresetSlider::IsPreviewSlider()
{
	VPANEL capturePanel = input()->GetMouseCapture();
	if ( capturePanel )
	{
		if ( capturePanel != GetVPanel() )
			return false;
		return true;
	}

	VPANEL appModal = input()->GetAppModalSurface();
	if ( appModal )
		return false;

	int mx, my;
	input()->GetCursorPos( mx, my );

	/*
	VPANEL topMost = IsWithinTraverse( mx, my, true );
	if ( topMost && topMost != GetVPanel() )
	{
	const char *name = ipanel()->GetName( topMost );
	return false;
	}
	*/

	return IsWithin( mx, my );
}

float CPresetSlider::GetCurrent()
{
	return GetValue() * 0.001f;
}

void CPresetSlider::SetPos( float frac )
{
	SetValue( (int)( frac * 1000.0f + 0.5f ), false );
}

void CPresetSlider::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_pName->SetFont( scheme->GetFont( "DefaultBold" ) );
	m_pName->SetColor( m_TextColor );
	m_pName->ResizeImageToContent();

	SetFgColor( Color( 194, 120, 0, 255 ) );
	SetThumbWidth( 3 );

	Color fullColor1( Color( 118, 71, 41, 255 ) );
	Color fullColor2( Color( 194, 120, 0, 255 ) );
	m_pEdgeButtons[ 0 ]->SetDefaultColor( m_ZeroColor, m_ZeroColor );
	m_pEdgeButtons[ 1 ]->SetDefaultColor( fullColor1, fullColor1 );
	m_pEdgeButtons[ 0 ]->SetDepressedColor( m_ZeroColor, m_ZeroColor );
	m_pEdgeButtons[ 1 ]->SetDepressedColor( fullColor2, fullColor2 );
	m_pEdgeButtons[ 0 ]->SetArmedColor( m_ZeroColor, m_ZeroColor );
	m_pEdgeButtons[ 1 ]->SetArmedColor( fullColor1, fullColor1 );
	m_pEdgeButtons[ 0 ]->SetButtonActivationType( Button::ACTIVATE_ONPRESSED );
	m_pEdgeButtons[ 1 ]->SetButtonActivationType( Button::ACTIVATE_ONPRESSED );
}

void CPresetSlider::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );

	int btnSize = 9;
	m_pEdgeButtons[ 0 ]->SetBounds( 3, ( h - btnSize ) / 2, btnSize, btnSize );
	m_pEdgeButtons[ 1 ]->SetBounds( w - 12, ( h - btnSize ) / 2, btnSize, btnSize );
}

void CPresetSlider::GetTrackRect( int &x, int &y, int &w, int &h )
{
	GetSize( w, h );
	x = 15;
	y = 2;
	w -= 30;
	h -= 4;
}


void CPresetSlider::SetGradientColor( const Color& clr )
{
	m_GradientColor = clr;
}

void CPresetSlider::Paint()
{
	if ( !IsPreviewSlider() )
		return;

	bool bIsCtrlKeyDown = vgui::input()->IsKeyDown( KEY_LCONTROL ) || vgui::input()->IsKeyDown( KEY_RCONTROL );
	if ( !IsDragging() && !bIsCtrlKeyDown )
		return;

	int mx, my;
	input()->GetCursorPos( mx, my );
	ScreenToLocal( mx, my );
	UpdateTickPos( mx, my );

	// horizontal nob
	int x, y;
	int wide,tall;
	GetTrackRect( x, y, wide, tall );

	Color col = GetFgColor();
	surface()->DrawSetColor( col );
	surface()->DrawFilledRect( _nobPos[0], 1, _nobPos[1], GetTall() - 1 );
	surface()->DrawSetColor( m_ZeroColor );
	surface()->DrawFilledRect( _nobPos[0] - 1, y + 1,  _nobPos[0], y + tall - 1 );
}

void CPresetSlider::PaintBackground()
{
	int w, h;
	GetSize( w, h );

	bool hasFocus = IsPreviewSlider();
	bool bIsCtrlKeyDown = vgui::input()->IsKeyDown( KEY_LCONTROL ) || vgui::input()->IsKeyDown( KEY_RCONTROL );
	bool bIsAltKeyDown = vgui::input()->IsKeyDown( KEY_LALT ) || vgui::input()->IsKeyDown( KEY_RALT );
	if ( hasFocus && ( IsDragging() || bIsCtrlKeyDown ) )
	{   
		int tx, ty, tw, th;
		GetTrackRect( tx, ty, tw, th );

		surface()->DrawSetColor( Color( 0, 0, 0,  255 ) );
		surface()->DrawOutlinedRect( tx, ty, tx + tw, ty + th );
		surface()->DrawSetColor( m_GradientColor );

		++tx;
		++ty;
		tw -= 2;
		th -= 2;

		// Gradient fill rectangle
		int fillw = (int)( (float)tw * GetCurrent() + 0.5f );

		int minAlpha = 15;
		float alphaTarget = 255.0f;

		int curAlpha = max( (int)(GetCurrent() * alphaTarget), minAlpha );
		 
		if ( _dragging )
		{
			surface()->DrawFilledRectFade( tx, ty, tx + fillw, ty + th, minAlpha, curAlpha, true );
			surface()->DrawSetColor( m_ZeroColor );
			surface()->DrawFilledRect( tx + fillw + 1, ty, tx + tw, ty + th );
		}
		else
		{												
			surface()->DrawSetColor( bIsAltKeyDown ? m_GradientColor : m_ZeroColor );
			surface()->DrawFilledRect( tx, ty, tx + tw, ty + th );
		}
	}

	int cw, ch;
	m_pName->SetColor( hasFocus ? m_TextColorFocus : m_TextColor );
	m_pName->GetContentSize( cw, ch );
	m_pName->SetPos( ( w - cw ) * 0.5f, ( h - ch ) * 0.5f );
	m_pName->Paint();
}


//-----------------------------------------------------------------------------
// Slider list panel
//-----------------------------------------------------------------------------
class CSliderListPanel : public PanelListPanel
{
	DECLARE_CLASS_SIMPLE( CSliderListPanel, vgui::PanelListPanel );

public:
	CSliderListPanel( CBaseAnimSetPresetFaderPanel *parent, vgui::Panel *pParent, const char *panelName );

	virtual void OnMouseReleased( vgui::MouseCode code );
	virtual void OnMousePressed( vgui::MouseCode code );

private:
	MESSAGE_FUNC( OnShowContextMenu, "OnShowContextMenu" );
	vgui::DHANDLE< vgui::Menu >	m_hContextMenu;
	CBaseAnimSetPresetFaderPanel *m_pParent;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSliderListPanel::CSliderListPanel( CBaseAnimSetPresetFaderPanel *pFader, vgui::Panel *pParent, const char *panelName ) :
	BaseClass( pParent, panelName )
{
	m_pParent = pFader;
}


//-----------------------------------------------------------------------------
// Context menu!
//-----------------------------------------------------------------------------
void CSliderListPanel::OnMousePressed( MouseCode code )
{
	if ( code == MOUSE_RIGHT )
		return;

	BaseClass::OnMousePressed( code );
}

void CSliderListPanel::OnMouseReleased( MouseCode code )
{
	if ( code == MOUSE_RIGHT )
	{
		OnShowContextMenu();
		return;
	}

	BaseClass::OnMouseReleased( code );
}


//-----------------------------------------------------------------------------
// Shows the slider context menu
//-----------------------------------------------------------------------------
void CSliderListPanel::OnShowContextMenu()
{
	if ( m_hContextMenu.Get() )
	{
		delete m_hContextMenu.Get();
		m_hContextMenu = NULL;
	}

	m_hContextMenu = new Menu( this, "ActionMenu" );

	m_hContextMenu->AddMenuItem( "Add...", new KeyValues( "AddPreset" ), m_pParent );
	m_hContextMenu->AddMenuItem( "Change Crossfade Speed...", new KeyValues( "SetPresetCrossfadeSpeed" ), m_pParent );

	m_hContextMenu->AddSeparator();

	m_hContextMenu->AddMenuItem( "Manage...", new KeyValues( "ManagePresets" ), m_pParent );

	Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CBaseAnimSetPresetFaderPanel::CBaseAnimSetPresetFaderPanel( vgui::Panel *parent, const char *className, CBaseAnimationSetEditor *editor ) :
	BaseClass( parent, className ),
	m_flLastFrameTime( 0.0f ),
	m_pSliders( NULL ),
	m_pWorkspace( NULL )
{
	m_hEditor = editor;

	m_pWorkspace = new EditablePanel( this, "PresetWorkspace" );
	m_pWorkspace->SetPaintBackgroundEnabled( false );
	m_pWorkspace->SetPaintEnabled( false );
	m_pWorkspace->SetPaintBorderEnabled( false );

	m_pSliders = new CSliderListPanel( this, m_pWorkspace, "PresetSliders" );
	m_pSliders->SetFirstColumnWidth( 0 );
	m_pSliders->SetAutoResize
		( 
		Panel::PIN_TOPLEFT, 
		Panel::AUTORESIZE_DOWNANDRIGHT,
		0, 24,
		0, 0
		);
	m_pSliders->SetPos( 0, 24 );
	m_pSliders->SetVerticalBufferPixels( 0 );

	m_pFilter = new TextEntry( m_pWorkspace, "PresetFilter" );
	m_pFilter->AddActionSignalTarget( this );
	m_pFilter->SetAutoResize
		( 
		Panel::PIN_TOPLEFT, 
		Panel::AUTORESIZE_RIGHT,
		2, 2,
		2, 22
		);
	m_pFilter->SetPos( 0, 0 );

	m_pWorkspace->SetAutoResize
		( 
		Panel::PIN_TOPLEFT, 
		Panel::AUTORESIZE_DOWNANDRIGHT,
		0, 0,
		0, 0
		);
}


//-----------------------------------------------------------------------------
// Purpose: refreshes dialog on text changing
//-----------------------------------------------------------------------------
void CBaseAnimSetPresetFaderPanel::OnTextChanged( )
{
	int nLength = m_pFilter->GetTextLength();
	m_Filter.SetLength( nLength );
	if ( nLength > 0 )
	{
		m_pFilter->GetText( m_Filter.GetForModify(), nLength+1 );
	}
	PopulateList( true );
}


//-----------------------------------------------------------------------------
// Called from the input dialogs used by this class
//-----------------------------------------------------------------------------
void CBaseAnimSetPresetFaderPanel::OnInputCompleted( KeyValues *pParams )
{
	const char *pText = pParams->GetString( "text", NULL );
	KeyValues *pContextKeyValues = pParams->FindKey( "CrossfadeSpeed" );
	if ( pContextKeyValues )
	{
		float f = Q_atof( pText );
		if ( f > 0.0f )
		{
			ifm_fader_timescale = f;
		}
		else
		{
			Warning( "Crossfade [%f] invalid, must be a postive number\n", f );
		}
		return;
	}

	Assert( 0 );
}


//-----------------------------------------------------------------------------
// Called from the add preset name dialogs used by this class
//-----------------------------------------------------------------------------
void CBaseAnimSetPresetFaderPanel::OnPresetNameSelected( KeyValues *pParams )
{
	const char *pText = pParams->GetString( "text", NULL );
	if ( !pText || !*pText )
	{
		vgui::MessageBox *pError = new MessageBox( "Add Preset Error", "Can't add preset with an empty name\n", this );
		pError->SetDeleteSelfOnClose( true );
		pError->DoModal();
		return;
	}

	CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Add Preset" );

	CDmePresetGroup *pPresetGroup = GetElementKeyValue<CDmePresetGroup>( pParams, "presetGroup" );
	if ( !pPresetGroup )
	{
		const char *pGroupName = pParams->GetString( "presetGroupName" );
		if ( !pGroupName )
			return;

		pPresetGroup = m_AnimSet->FindOrAddPresetGroup( pGroupName );
	}

	if ( pPresetGroup->m_bIsReadOnly )
	{
		vgui::MessageBox *pError = new MessageBox( "Add Preset Error", "Can't add preset to a read-only preset group!\n", this );
		pError->SetDeleteSelfOnClose( true );
		pError->DoModal();
		return;
	}

	if ( pPresetGroup->FindPreset( pText ) )
	{
		vgui::MessageBox *pError = new MessageBox( "Add Preset Error", "A preset with that name already exists!\n", this );
		pError->SetDeleteSelfOnClose( true );
		pError->DoModal();
		return;
	}

	CDmePreset *pPreset = pPresetGroup->FindOrAddPreset( pText );
	AddNewPreset( pPreset );
	guard.Release();

	ChangeAnimationSet( m_AnimSet );
}


//-----------------------------------------------------------------------------
// The 'set crossfade speed' context menu option
//-----------------------------------------------------------------------------
void CBaseAnimSetPresetFaderPanel::OnSetCrossfadeSpeed()
{
	if ( m_hInputDialog.Get() )
	{
		delete m_hInputDialog.Get();
	}

	char sz[32 ];
	Q_snprintf( sz, sizeof( sz ), "%f", ifm_fader_timescale );
	m_hInputDialog = new InputDialog( this, "Crossfade Speed", "Fader Crossfade Rate:", sz );
	if ( m_hInputDialog.Get() )
	{
		KeyValues *pContextKeyValues = new KeyValues( "CrossfadeSpeed" );
		m_hInputDialog->SetSmallCaption( true );
		m_hInputDialog->SetMultiline( false );
		m_hInputDialog->DoModal( pContextKeyValues );
	}
}


//-----------------------------------------------------------------------------
// The 'add preset' context menu option
//-----------------------------------------------------------------------------
void CBaseAnimSetPresetFaderPanel::OnAddPreset()
{
	if ( !m_AnimSet.Get() )
		return;

	if ( !m_hAddPresetDialog.Get() )
	{
		m_hAddPresetDialog = new CAddPresetDialog( this );
		m_hAddPresetDialog->AddActionSignalTarget( this );
	}

	m_hAddPresetDialog->DoModal( m_AnimSet, NULL );
}


//-----------------------------------------------------------------------------
// Called by the preset group editor panel when it changes presets
//-----------------------------------------------------------------------------
void CBaseAnimSetPresetFaderPanel::OnPresetsChanged()
{
	ChangeAnimationSet( m_AnimSet );
}


//-----------------------------------------------------------------------------
// Brings up the preset manager
//-----------------------------------------------------------------------------
void CBaseAnimSetPresetFaderPanel::OnManagePresets()
{
	if ( !m_hPresetEditor.Get() )
	{
		m_hPresetEditor = new CDmePresetGroupEditorFrame( this, "Manage Presets" );
		m_hPresetEditor->AddActionSignalTarget( this );
		m_hPresetEditor->SetVisible( false );
		m_hPresetEditor->SetDeleteSelfOnClose( false );
		m_hPresetEditor->MoveToCenterOfScreen();
	}

	m_hPresetEditor->SetAnimationSet( m_AnimSet );
	m_hPresetEditor->DoModal( );
}

void CBaseAnimSetPresetFaderPanel::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	m_pSliders->SetBgColor( Color( 42, 42, 42, 255 ) );
}

void CBaseAnimSetPresetFaderPanel::GetPreviewFader( FaderPreview_t& fader )
{
	Q_memset( &fader, 0, sizeof( fader ) );

	fader.isbeingdragged = false;

	fader.holdingctrl = input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL );
	int mx, my;
	input()->GetCursorPos( mx, my );
	if ( !IsWithin( mx, my ) )
	{
		fader.holdingctrl = false;
	}

	// Walk through sliders and figure out which is under the mouse
	CPresetSlider *mouseOver = NULL;

	for ( int i = m_pSliders->FirstItem(); i != m_pSliders->InvalidItemID(); i = m_pSliders->NextItem(i) )
	{
		CPresetSlider *slider = static_cast< CPresetSlider * >( m_pSliders->GetItemPanel( i ) );
		if ( !slider || !slider->IsPreviewSlider() )
			continue;

		mouseOver = slider;
		fader.isbeingdragged = slider->IsDragging();
		break;
	}

	if ( mouseOver )
	{
		// Deal with procedural presets here
		if ( fader.holdingctrl || 
			 fader.isbeingdragged )
		{
			mouseOver->UpdateProceduralValues();
		}

		fader.name	 = mouseOver->GetName();
		fader.amount = mouseOver->GetCurrent();
		fader.values = mouseOver->GetAttributeDict();
		fader.preset = mouseOver->GetPreset();
	}
}

void CBaseAnimSetPresetFaderPanel::PopulateList( bool bChanged )
{
	if ( !m_AnimSet.Get() )
	{
		m_CurrentPresetList.RemoveAll();
		m_pSliders->DeleteAllItems();
		return;
	}

	CDmrElementArray< CDmePresetGroup > presetGroups = m_AnimSet->GetPresetGroups();
	Assert( presetGroups.IsValid() );

	int c = presetGroups.Count();

	bool bNeedRebuild = false;
	int slot = 0;
	for ( int i = 0 ; i < c; ++i )
	{
		CDmePresetGroup *pPresetGroup = presetGroups[ i ];
		Assert( pPresetGroup );
		if ( !pPresetGroup || !pPresetGroup->m_bIsVisible )
			continue;

		CDmrElementArray< CDmePreset > presets = pPresetGroup->GetPresets();

		int cp = presets.Count();
		for ( int j = 0; j < cp; ++j )
		{
			CDmePreset *pPreset = presets[ j ];
			Assert( pPreset );

			const char *pElementName = pPreset->GetName();
			if ( Q_stricmp( pElementName, "Default" ) )
			{
				if ( m_Filter.Length() && !Q_stristr( pElementName, m_Filter.Get() ) )
					continue;
			}

			if ( slot >= m_CurrentPresetList.Count() )
			{
				bNeedRebuild = true;
				break;
			}

			if ( pPreset != m_CurrentPresetList[ slot ] )
			{
				bNeedRebuild = true;
				break;
			}

			++slot;
		}
	}

	if ( slot != m_CurrentPresetList.Count() )
	{
		bNeedRebuild = true;
	}

	if ( bNeedRebuild )
	{
		m_CurrentPresetList.RemoveAll();
		m_pSliders->DeleteAllItems();

		for ( int i = 0 ; i < c; ++i )
		{
			CDmePresetGroup *pPresetGroup = presetGroups[ i ];
			Assert( pPresetGroup );
			if ( !pPresetGroup || !pPresetGroup->m_bIsVisible )
				continue;

			CDmrElementArray< CDmePreset > presets = pPresetGroup->GetPresets();

			int cp = presets.Count();
			for ( int j = 0; j < cp; ++j )
			{
				CDmePreset *pPreset = presets[ j ];
				Assert( pPreset );

				const char *pElementName = pPreset->GetName();
				if ( Q_stricmp( pElementName, "Default" ) )
				{
					if ( m_Filter.Length() && !Q_stristr( pElementName, m_Filter.Get() ) )
						continue;
				}

				CPresetSlider *pSlider = new CPresetSlider( this, pPreset->GetName(), pPreset );
				pSlider->SetPos( 0 );
				pSlider->SetSize( 100, 20 );
				pSlider->SetGradientColor( Color( 194, 120, 0, 255 ) );

				pSlider->SetControlValues( );

				m_pSliders->AddItem( NULL, pSlider );

				m_CurrentPresetList.AddToTail( pPreset->GetHandle() );
			}
		}
	}
	else
	{
		UpdateControlValues();
	}
}

void CBaseAnimSetPresetFaderPanel::ChangeAnimationSet( CDmeAnimationSet *newAnimSet )
{
	bool bChanged = m_AnimSet != newAnimSet;
	m_AnimSet = newAnimSet;
	PopulateList( bChanged );
}

void CBaseAnimSetPresetFaderPanel::UpdateControlValues()
{
	for ( int i = m_pSliders->FirstItem(); i != m_pSliders->InvalidItemID(); i = m_pSliders->NextItem(i) )
	{
		CPresetSlider *pSlider = static_cast< CPresetSlider * >( m_pSliders->GetItemPanel( i ) );
		if ( pSlider )
		{
			pSlider->SetControlValues( );
		}
	}
}

void CBaseAnimSetPresetFaderPanel::ApplyPreset( float flScale, AttributeDict_t& dict )
{
	CBaseAnimSetAttributeSliderPanel *sliderPanel = m_hEditor->GetAttributeSlider();
	if ( sliderPanel )
	{
		sliderPanel->ApplyPreset( flScale, dict );
	}
}


//-----------------------------------------------------------------------------
// Reads the current animation set control values, creates presets
//-----------------------------------------------------------------------------
void CBaseAnimSetPresetFaderPanel::SetPresetFromSliders( CDmePreset *pPreset )
{
	if ( !m_AnimSet.Get() )
		return;

	CBaseAnimSetAttributeSliderPanel *pSliderPanel = m_hEditor->GetAttributeSlider();
	if ( !pSliderPanel )
		return;

	CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Set Preset" );

	const CDmrElementArray< CDmElement > controlList = m_AnimSet->GetControls();

	// Now set values for each known control
	int numControls = controlList.Count();
	for ( int i = 0; i < numControls; ++i )
	{
		CDmElement *pControl = controlList[ i ];

		if ( pControl->GetValue<bool>( "transform" ) )
			continue;

		// Facial sliders below
		AttributeValue_t value;
		bool bIsDefault = !pSliderPanel->GetAttributeSliderValue( &value, pControl->GetName() );
		if ( !bIsDefault )
		{
			float flDefaultValue = pControl->GetValue< float >( "defaultValue" );
			float flDefaultBalance = pControl->GetValue< float >( "defaultBalance" );
			float flDefaultMultilevel = pControl->GetValue< float >( "defaultMultilevel" );
			bIsDefault = ( value.m_pValue[ANIM_CONTROL_VALUE] == flDefaultValue ) && 
				( value.m_pValue[ANIM_CONTROL_BALANCE] == flDefaultBalance ) && 
				( value.m_pValue[ANIM_CONTROL_MULTILEVEL] == flDefaultMultilevel );
		}

		// Blow away preset values for controls that contain the default values
		if ( bIsDefault )
		{
			pPreset->RemoveControlValue( pControl->GetName() );
			continue;
		}

		bool bIsCombo = pControl->GetValue< bool >( "combo" );
		bool bIsMulti = pControl->GetValue< bool >( "multi" );

		// Stamp the control value
		CDmElement *pControlValue = pPreset->FindOrAddControlValue( pControl->GetName() );
		pControlValue->SetValue< float >( "value", value.m_pValue[ANIM_CONTROL_VALUE] );
		if ( bIsCombo )
		{
			pControlValue->SetValue< float >( "balance", value.m_pValue[ANIM_CONTROL_BALANCE] );
		}
		else
		{
			pControlValue->RemoveAttribute( "balance" );
		}

		if ( bIsMulti )
		{
			pControlValue->SetValue< float >( "multilevel", value.m_pValue[ANIM_CONTROL_MULTILEVEL] );
		}
		else
		{
			pControlValue->RemoveAttribute( "multilevel" );
		}
	}
}

void CBaseAnimSetPresetFaderPanel::AddNewPreset( CDmePreset *pPreset )
{
	if ( !pPreset )
		return;

	SetPresetFromSliders( pPreset );

	CPresetSlider *pSlider = new CPresetSlider( this, pPreset->GetName(), pPreset );
	if ( pSlider )
	{
		pSlider->SetPos( 0 );
		pSlider->SetSize( 100, 20 );
		pSlider->SetGradientColor( Color( 194, 120, 0, 255 ) );
		pSlider->SetControlValues( );
		m_pSliders->AddItem( NULL, pSlider );
	}
}

void CBaseAnimSetPresetFaderPanel::OnAddNewPreset( KeyValues *pKeyValues )
{
	CDmePreset *pPreset = GetElementKeyValue<CDmePreset>( pKeyValues, "preset" );

	CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Overwrite Preset" );
	AddNewPreset( pPreset );
	guard.Release();

	ChangeAnimationSet( m_AnimSet );
}

void CBaseAnimSetPresetFaderPanel::AddNewPreset( const char *pGroupName, const char *pName )
{
	if ( !m_AnimSet.Get() )
		return;

	CBaseAnimSetAttributeSliderPanel *sliderPanel = m_hEditor->GetAttributeSlider();
	if ( !sliderPanel )
		return;

	CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Add Preset" );

	CDmePresetGroup *pPresetGroup = m_AnimSet->FindOrAddPresetGroup( pGroupName );
	CDmePreset *pPreset = pPresetGroup->FindOrAddPreset( pName );
	AddNewPreset( pPreset );
	guard.Release();

	ChangeAnimationSet( m_AnimSet );
}

void CBaseAnimSetPresetFaderPanel::OnOverwritePreset( CDmePreset *pPreset )
{
	SetPresetFromSliders( pPreset );
	UpdateControlValues();
}

void CBaseAnimSetPresetFaderPanel::OnDeletePreset( CDmePreset *pPreset )
{
	{
		// Delete it from various things
		CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Delete Preset" );
		m_AnimSet->RemovePreset( pPreset );
	}

	ChangeAnimationSet( m_AnimSet );
}

void CBaseAnimSetPresetFaderPanel::ProceduralPreset_UpdateCrossfade( CDmePreset *pPreset, bool bFadeIn )
{
	// Handled by derived class in SFM
}
