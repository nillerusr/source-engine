//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANEL2D_H
#define PANEL2D_H

#ifdef _WIN32
#pragma once
#endif


#include "../iuiengine.h"
#include "../iuipanel.h"
#include "../iuipanelclient.h"
#include "../iuipanelstyle.h"
#include "panorama/transformations.h"
#include "panorama/input/keycodes.h"
#include "panorama/layout/panel2dfactory.h"
#include "panorama/layout/stylefiletypes.h"
#include "panorama/input/mousecursors.h"
#include "panorama/input/iuiinput.h"
#include "panorama/uieventcodes.h"
#include "panorama/uievent.h"
#include "panorama/iuiwindow.h"
#include "panorama/layout/panel2dfactory.h"
#include "color.h"
#include "utlvector.h"
#include "utlstring.h"
#include "panelptr.h"
#include "steam/steamtypes.h"
#if defined( SOURCE2_PANORAMA )
#include "currencyamount.h"
#else
#include "rtime.h"
#include "amount.h"
#endif
#include "tier1/utlmap.h"
#include "panorama/layout/stylesymbol.h"

namespace panorama
{

#pragma warning(push)
// warning C4251: 'CPanel2D::symbol' : class 'CUtlSymbol' needs to have dll-interface to be used by clients of class 'CPanel2D'
#pragma warning( disable : 4251 ) 

class CLayoutFile;
class CTopLevelWindow;
class CVerticalScrollBar;
class CHorizontalScrollBar;
struct PanelDescription_t;
class CUIRenderEngine;
class CImageResourceManager;
class CPanelStyle;
class CBackgroundImageLayer;
class CPanel2D;
class CScrollBar;

// Typedefs for getter/setter methods that can be exposed via JS
typedef float(CPanel2D::*PanelFloatGetter_t)() const;
typedef void(CPanel2D::*PanelFloatSetter_t)(float);
typedef const char *(CPanel2D::*PanelStringGetter_t)() const;
typedef void(CPanel2D::*PanelStringSetter_t)(const char *);
typedef bool(CPanel2D::*PanelBoolGetter_t)() const;
typedef void(CPanel2D::*PanelBoolSetter_t)(bool);
typedef CPanoramaSymbol( CPanel2D::*PanelSymbolGetter_t )() const;
typedef void(CPanel2D::*PanelSymbolSetter_t)(CPanoramaSymbol);

inline CPanel2D * ToPanel2D( IUIPanel *pPanel )
{
	if( pPanel )
		return (CPanel2D*)(pPanel->ClientPtr());

	return NULL;
}

	
//-----------------------------------------------------------------------------
// Purpose: Struct used to perform hit tests
//-----------------------------------------------------------------------------
struct TransformContext_t
{
	float m_flPosX;
	float m_flPosY;
	float m_flPosZ;
	VMatrix m_TransformMatrix;
	float m_flWidth;
	float m_flHeight;

	float m_flPerspective;
	float m_flPerspectiveOriginX;
	float m_flPerspectiveOriginY;
};


//-----------------------------------------------------------------------------
// Purpose: Basic 2D UI panel.  These may be transformed in 3D space, but they are 
// at a base level 2D rectangular containers for other panels/paint operations.
//-----------------------------------------------------------------------------
class CPanel2D : public panorama::IUIPanelClient
{
	DECLARE_PANEL2D_NO_BASE( CPanel2D );

public:
	CPanel2D( IUIWindow *parent, const char * pchID );
	CPanel2D( CPanel2D *parent, const char * pchID );
	CPanel2D( CPanel2D *parent, const char *pchID, uint32 ePanelFlags );

	virtual ~CPanel2D();

	// Check if the panel has loaded layout
	bool IsLoaded() const { return m_pIUIPanel->IsLoaded(); }

	virtual void OnDeletePanel() OVERRIDE { delete this; }

	// Access the panorama side UI panel interface for the client panel
	virtual IUIPanel *UIPanel() const { return m_pIUIPanel; }

	void DeleteAsync( float flDelay = 0.0f );

	// Set the panel visible
	void SetVisible( bool bVisible ) { m_pIUIPanel->SetVisible( bVisible ); }
	bool BIsVisible() const { return m_pIUIPanel->BIsVisible(); }

	// Get the base position for the panel
	void GetPosition( CUILength &x, CUILength &y, CUILength &z, bool bIncludeUIScaleFactor = true );

	// Set the base position for the panel
	void SetPosition( CUILength x, CUILength y, CUILength z, bool bPreScaledByUIScaleFactor = false );
	void SetPositionWithoutTransition( CUILength x, CUILength y, CUILength z, bool bPreScaledByUIScaleFactor = false );
	void SetTransform3D( const CUtlVector<CTransform3D *> &vecTransforms );
	void SetOpacity( float flOpacity );
	bool BIsTransparent() { return m_pIUIPanel->BIsTransparent();  }
	void SetPreTransformScale2D( float flX, float flY );

	// Set the base width/height for the panel
	CUILength GetStyleWidth();
	CUILength GetStyleHeight();
	void SetSize( CUILength width, CUILength height );

	// The the bounds that should be used for placing a tooltip, returning false means "use the entire panel"
	virtual bool GetContextUIBounds( float *pflX, float *pflY, float *pflWidth, float *pflHeight ) { return false; }

	// Set the animation style for the panel
	void SetAnimation( const char *pchAnimationName, float flDuration, float flDelay, EAnimationTimingFunction eTimingFunc, EAnimationDirection eDirection, float flIterations ) { m_pIUIPanel->SetAnimation( pchAnimationName, flDuration, flDelay, eTimingFunc, eDirection, flIterations ); }

	// Returns the layout file for this panel
	CPanoramaSymbol GetLayoutFile() const { return m_pIUIPanel->GetLayoutFile(); }

	// Returns define from layout file for this panel
	char const * GetLayoutFileDefine( char const *szDefineName );
	int GetLayoutFileDefineInt( const char *szDefineName, int defaultValue );
	float GetLayoutFileDefineFloat( const char *szDefineName, float defaultValue );

	// Style access
	panorama::IUIPanelStyle *AccessStyle() const { return m_pIUIPanel->AccessIUIStyle(); }

	// Style access
	panorama::IUIPanelStyle *AccessStyleDirty() const { return m_pIUIPanel->AccessIUIStyleDirty(); }

	void ApplyStyles( bool bTraverse ) { return m_pIUIPanel->ApplyStyles( bTraverse ); }

	// Mark styles dirty for the panel
	void MarkStylesDirty( bool bIncludeChildren ) { m_pIUIPanel->MarkStylesDirty( bIncludeChildren ); }

	void ClearPropertyFromCode( panorama::CStyleSymbol symProperty );

	// Virtual called on scale factor for panel changing
	virtual void OnUIScaleFactorChanged( float flScaleFactor ) OVERRIDE { }

	// Paint the panel and it's children, called by the rendering layer when it's time to paint.
	void PaintTraverse() { m_pIUIPanel->PaintTraverse(); }

	// Paint the panels contents
	virtual void Paint() OVERRIDE;

	// Invalidates painting and tells the panel it must repaint next frame
	void SetRepaint( EPanelRepaint eRepaintNeeded );

	// sets & loads the layout file for this panel
	bool BLoadLayout( const char *pchFile, bool bOverrideExisting = false, bool bPartialLayout = false ) { return m_pIUIPanel->BLoadLayout( pchFile, bOverrideExisting, bPartialLayout ); }

	// sets & loads the layout for this panel
	bool BLoadLayoutFromString( const char *pchXMLString, bool bOverrideExisting = false, bool bPartialLayout = false ) { return m_pIUIPanel->BLoadLayoutFromString( pchXMLString, bOverrideExisting, bPartialLayout ); }

	// sets loads the layout file for this panel, asynchronously supporting remote http:// paths
	void LoadLayoutAsync( const char *pchFile, bool bOverrideExisting = false, bool bPartialLayout = false ) { return m_pIUIPanel->LoadLayoutAsync( pchFile, bOverrideExisting, bPartialLayout ); }

	// loads the layout file for this panel, asynchronously supporting remote http:// paths in css within
	void LoadLayoutFromStringAsync( const char *pchXMLString, bool bOverrideExisting, bool bPartialLayout = false ) { return m_pIUIPanel->LoadLayoutFromStringAsync( pchXMLString, bOverrideExisting, bPartialLayout );  }

	// Measure self and children. First pass of layout
	void DesiredLayoutSizeTraverse( float flMaxWidth, float flMaxHeight ) { m_pIUIPanel->DesiredLayoutSizeTraverse( flMaxWidth, flMaxHeight ); }
	void DesiredLayoutSizeTraverse( float *pflDesiredWidth, float *pflDesiredHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions ) { m_pIUIPanel->DesiredLayoutSizeTraverse( pflDesiredWidth, pflDesiredHeight, flMaxWidth, flMaxHeight, bFinalDimensions );  }

	// Arrange children. Second pass of layout
	void LayoutTraverse( float x, float y, float flFinalWidth, float flFinalHeight ) { m_pIUIPanel->LayoutTraverse( x, y, flFinalWidth, flFinalHeight ); }

	// methods to invalid certain parts of layout
	void InvalidateSizeAndPosition() { m_pIUIPanel->InvalidateSizeAndPosition(); }
	void InvalidatePosition() { m_pIUIPanel->InvalidatePosition(); }
	bool IsSizeValid() { return  m_pIUIPanel->IsSizeValid(); }
	bool IsPositionValid() { return m_pIUIPanel->IsPositionValid(); }
	bool IsChildSizeValid() { return m_pIUIPanel->IsChildSizeValid(); }
	bool IsChildPositionValid() { return m_pIUIPanel->IsChildPositionValid(); }
	bool IsSizeTransitioning() { return m_pIUIPanel->IsSizeTransitioning(); }
	bool IsPositionTransitioning() { return m_pIUIPanel->IsPositionTransitioning(); }
	bool IsChildPositionTransitioning() { return m_pIUIPanel->IsChildPositionTransitioning(); }
	bool IsChildSizeTransitioning() { return m_pIUIPanel->IsChildSizeTransitioning(); }

	// size getters
	float GetDesiredLayoutWidth() const { return m_pIUIPanel->GetDesiredLayoutWidth(); }
	float GetDesiredLayoutHeight() const { return m_pIUIPanel->GetDesiredLayoutHeight(); }

	// Content size is what our contents actually take up, not accounting for fixed/relative
	// size set on us in styles which affect desired layout size
	float GetContentWidth() const { return m_pIUIPanel->GetContentWidth(); }
	float GetContentHeight() const { return m_pIUIPanel->GetContentHeight(); }

	// Actual size is the size given to the panel after layout, hopefully as big as its desired size.  
	// Actual size does NOT include margins (which are really in the parent).
	float GetActualLayoutWidth() const { return m_pIUIPanel->GetActualLayoutWidth(); }
	float GetActualLayoutHeight() const { return m_pIUIPanel->GetActualLayoutHeight(); }

	// Render size is the size of the content for rendering, this is either the actual layout size, or if
	// that is smaller than the content size + padding then it's the content size + padding.
	float GetActualRenderWidth() { return m_pIUIPanel->GetActualRenderWidth(); }
	float GetActualRenderHeight() { return m_pIUIPanel->GetActualRenderHeight(); }

	// Offset will include position, alignment, and margin adjustments
	float GetActualXOffset() const { return m_pIUIPanel->GetActualXOffset(); }
	float GetActualYOffset() const { return m_pIUIPanel->GetActualYOffset(); }

	// Offset to apply to contents for scrolling
	float GetContentsYScrollOffset() const { return m_pIUIPanel->GetContentsYScrollOffset(); }
	float GetContentsXScrollOffset() const { return m_pIUIPanel->GetContentsXScrollOffset(); }
	float GetContentsYScrollOffsetTarget() const { return m_pIUIPanel->GetContentsYScrollOffsetTarget(); }
	float GetContentsXScrollOffsetTarget() const { return m_pIUIPanel->GetContentsXScrollOffsetTarget(); }
	double GetContentsXScrollTransitionStart() const { return m_pIUIPanel->GetContentsXScrollTransitionStart(); }
	double GetContentsYScrollTransitionStart() const { return m_pIUIPanel->GetContentsYScrollTransitionStart(); }
	float GetInterpolatedXScrollOffset() { return m_pIUIPanel->GetInterpolatedXScrollOffset(); }
	float GetInterpolatedYScrollOffset() { return m_pIUIPanel->GetInterpolatedYScrollOffset(); }

	// Can the panel scroll further?
	bool BCanScrollUp() { return m_pIUIPanel->BCanScrollUp();  }
	bool BCanScrollDown() { return m_pIUIPanel->BCanScrollDown();  }
	bool BCanScrollLeft() { return m_pIUIPanel->BCanScrollLeft(); }
	bool BCanScrollRight() { return m_pIUIPanel->BCanScrollRight(); }

	// style class management
	void AddClass( const char *pchName ) { m_pIUIPanel->AddClass( pchName ); }
	void AddClass( CPanoramaSymbol symbol ) { m_pIUIPanel->AddClass( symbol ); }
	void AddClasses( const char *pchName ) { m_pIUIPanel->AddClasses( pchName ); }
	void AddClasses( CPanoramaSymbol *pSymbols, uint cSymbols ) { m_pIUIPanel->AddClasses( pSymbols, cSymbols ); }
	void RemoveClass( const char *pchName ) { m_pIUIPanel->RemoveClass( pchName ); }
	void RemoveClass( CPanoramaSymbol symName ) { m_pIUIPanel->RemoveClass( symName ); }
	void RemoveClasses( const CPanoramaSymbol *pSymbols, uint cSymbols ) { m_pIUIPanel->RemoveClasses( pSymbols, cSymbols ); }
	void RemoveClasses( const char *pchName ) { m_pIUIPanel->RemoveClasses( pchName ); }
	void RemoveAllClasses() { m_pIUIPanel->RemoveAllClasses(); }

	// bugbug jmccaskey - dangerous cross dll interface signature?  Is CUtlVector the same in debug/release?
	const CUtlVector< CPanoramaSymbol > &GetClasses() const { return m_pIUIPanel->GetClasses(); }

	bool BHasClass( const char *pchName ) const { return m_pIUIPanel->BHasClass( pchName ); }
	bool BHasClass( CPanoramaSymbol symName ) const { return m_pIUIPanel->BHasClass( symName ); }
	bool BAscendantHasClass( const char *pchName ) const { return m_pIUIPanel->BAscendantHasClass( pchName ); }
	bool BAscendantHasClass( CPanoramaSymbol symName ) const { return m_pIUIPanel->BAscendantHasClass( symName ); }
	void ToggleClass( const char *pchName ) { m_pIUIPanel->ToggleClass( pchName ); }
	void ToggleClass( CPanoramaSymbol symName ) { m_pIUIPanel->ToggleClass( symName ); }
	void SetHasClass( const char *pchName, bool bHasClass ) { m_pIUIPanel->SetHasClass( pchName, bHasClass ); }
	void SetHasClass( CPanoramaSymbol symName, bool bHasClass ) { m_pIUIPanel->SetHasClass( symName, bHasClass ); }
	void SwitchClass( const char *pchAttribute, const char *pchName ) { m_pIUIPanel->SwitchClass( pchAttribute, pchName ); }
	void SwitchClass( const char *pchAttribute, CPanoramaSymbol symName ) { m_pIUIPanel->SwitchClass( pchAttribute, symName ); }

	const char *GetID() const { return m_pIUIPanel->GetID(); }
	bool BHasID() const { return m_pIUIPanel->GetID()[0] != '0'; }

	void SetTabIndex( float flTabIndex ) { m_pIUIPanel->SetTabIndex( flTabIndex ); }
	void SetSelectionPosition( float flXPos, float flYPos ) { m_pIUIPanel->SetSelectionPosition( flXPos, flYPos ); }
	void SetSelectionPositionX( float flXPos ) { m_pIUIPanel->SetSelectionPositionX( flXPos ); }
	void SetSelectionPositionY( float flYPos ) { m_pIUIPanel->SetSelectionPositionY( flYPos ); }

	float GetSelectionPositionX() const { return m_pIUIPanel->GetSelectionPositionX(); }
	float GetSelectionPositionY() const { return m_pIUIPanel->GetSelectionPositionY(); }
	float GetTabIndex() const { return m_pIUIPanel->GetTabIndex(); }

	//
	// Implementation of functions that panorama calls back on UI control classes
	//

	// kb/mouse management
	virtual bool OnKeyDown( const KeyData_t &code ) OVERRIDE;
	virtual bool OnKeyUp( const KeyData_t & code ) OVERRIDE;
	virtual bool OnKeyTyped( const KeyData_t &unichar ) OVERRIDE;
	virtual bool OnGamePadDown( const GamePadData_t &code ) OVERRIDE;
	virtual bool OnGamePadUp( const GamePadData_t &code ) OVERRIDE;
	virtual bool OnGamePadAnalog( const GamePadData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonDown( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonUp( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonDoubleClick( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonTripleClick( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseWheel( const MouseData_t &code ) OVERRIDE;
	virtual void OnMouseMove( float flMouseX, float flMouseY ) OVERRIDE;
	virtual bool OnClick( IUIPanel *pPanel, const MouseData_t &code ) OVERRIDE;

	// events
	bool OnScrollDirection( IUIScrollBar *pScrollBar, bool bIncreasePosition, float flDelta );
	bool OnScrollUp();
	bool OnScrollDown();
	bool OnScrollLeft();
	bool OnScrollRight();

	bool OnPageDirection( IUIScrollBar *pScrollBar, bool bIncreasePosition );
	bool OnPageUp();
	bool OnPageDown();
	bool OnPageLeft();
	bool OnPageRight();

	// Scrolling
	void ScrollVertically( float flScrollDelta, bool bImmediateMove = false );
	void ScrollHorizontally( float flScrollDelta, bool bImmediateMove = false );
	void ScrollToShowVerticalRange( float flRangeStart, float flRangeEnd );
	virtual void ScrollToXPercent( float flXPercent );
	virtual void ScrollToYPercent( float flXPercent );

	// navigation events, override these in children if you want special navigation handling,
	// shouldn't be needed often though, use tabindex/selectionpos in xml layout files!
	virtual bool OnMoveUp( int nRepeats );
	virtual bool OnMoveDown( int nRepeats );
	virtual bool OnMoveLeft( int nRepeats );
	virtual bool OnMoveRight( int nRepeats );
	virtual bool OnTabForward( int nRepeats );
	virtual bool OnTabBackward( int nRepeats );


	// child iteration
	int GetChildCount() const { return m_pIUIPanel->GetChildCount(); }
	CPanel2D *GetChild( int i ) const { return ToPanel2D( m_pIUIPanel->GetChild( i ) ); }
	CPanel2D *GetFirstChild() const { return ToPanel2D( m_pIUIPanel->GetFirstChild() ); }
	CPanel2D *GetLastChild() const { return ToPanel2D( m_pIUIPanel->GetLastChild() ); }
	// Return index of child in creation/panel vector order (also default tab order)
	int GetChildIndex( const CPanel2D *pChild ) const { if( !pChild ) return -1; else return m_pIUIPanel->GetChildIndex( pChild->UIPanel() ); }

	int GetHiddenChildCount() const { return m_pIUIPanel->GetHiddenChildCount(); }
	CPanel2D *GetHiddenChild( int i ) const { return ToPanel2D( m_pIUIPanel->GetHiddenChild( i ) ); }

	// Gets immediate ancestor of this panel in panel hierarchy
	CPanel2D *GetParent() const { return ToPanel2D( m_pIUIPanel->GetParent() ); }
	
	// searches only immediate children
	CPanel2D *FindChild( const char *pchID ) { return ToPanel2D( m_pIUIPanel->FindChild( pchID ) ); }

	// searches all children even outside layout file scope
	CPanel2D *FindChildTraverse( const char *pchID ) { return ToPanel2D( m_pIUIPanel->FindChildTraverse( pchID ) ); }

	// searches any children created from our layout file
	CPanel2D *FindChildInLayoutFile( const char *pchID ) { return ToPanel2D( m_pIUIPanel->FindChildInLayoutFile( pchID ) ); }

	// searches any panel created from our layout file (so parents or children!)
	CPanel2D *FindPanelInLayoutFile( const char *pchID ) { return ToPanel2D( m_pIUIPanel->FindPanelInLayoutFile( pchID ) ); }

	void MoveChildAfter( CPanel2D *pChildToMove, CPanel2D *pBefore ) { return m_pIUIPanel->MoveChildAfter( pChildToMove->UIPanel(), pBefore->UIPanel() ); }
	void MoveChildBefore( CPanel2D *pChildToMove, CPanel2D *pAfter ) { return m_pIUIPanel->MoveChildBefore( pChildToMove->UIPanel(), pAfter->UIPanel() ); }

	// window management
	IUIWindow *GetParentWindow() const { return m_pIUIPanel->GetParentWindow(); }


	// input & focus
	bool BAcceptsInput() { return m_pIUIPanel->BAcceptsInput();  }
	void SetAcceptsInput( bool bAllowInput ) { m_pIUIPanel->SetAcceptsInput( bAllowInput );  }
	bool BAcceptsFocus() const { return m_pIUIPanel->BAcceptsFocus(); }
	void SetAcceptsFocus( bool bAllowFocus ) { m_pIUIPanel->SetAcceptsFocus( bAllowFocus ); }
	bool BCanAcceptInput() { return m_pIUIPanel->BCanAcceptInput(); }
	void SetDefaultFocus( const char *pchChildID ) { m_pIUIPanel->SetDefaultFocus( pchChildID ); }
	const char *GetDefaultFocus() const { return m_pIUIPanel->GetDefaultFocus();  }
	void SetDisableFocusOnMouseDown( bool bDisable ) { m_pIUIPanel->SetDisableFocusOnMouseDown( bDisable ); }
	bool BFocusOnMouseDown() { return m_pIUIPanel->BFocusOnMouseDown(); }
	virtual bool BRequiresFocus() { return false; } // Override if your control requires taking focus in order to operate (e.g. TextEntry)

	// Should this panel be the top of an input hierarchy and keep track of focus within itself, not losing focus when a panel in some
	// other hierarchy changes focus?  Use this for panels that are peers like friends vs browser vs mainmenu in tenfoot
	bool BTopOfInputContext() {	return m_pIUIPanel->BTopOfInputContext(); }
	void SetTopOfInputContext( bool bIsTopOfInputContext ) { return m_pIUIPanel->SetTopOfInputContext( bIsTopOfInputContext ); }

	CPanel2D *GetParentInputContext() { return ToPanel2D( m_pIUIPanel->GetParentInputContext() ); }

	// Get the default input focus child within this panel, may be null
	CPanel2D *GetDefaultInputFocus() { return ToPanel2D( m_pIUIPanel->GetDefaultInputFocus() ); }

	// Override behavior for getting default input focus, callback from framework
	virtual IUIPanel *OnGetDefaultInputFocus() OVERRIDE { return NULL;  }

	// Set focus to this panel, which will auto-scroll it into full view as well if parent has overflow: scroll
	bool SetFocus() { return m_pIUIPanel->SetFocus(); }

	// Set the focus to this panel in it's input context, but do not make the context change if some other context currently
	// has focus
	bool UpdateFocusInContext() { return m_pIUIPanel->UpdateFocusInContext(); }

	// Set the focus in response to receiving hover (on panels that a parent sets childfocusonhover), this will
	// never scroll the parent.
	bool SetFocusDueToHover() { return m_pIUIPanel->SetFocusDueToHover(); }

	void SetInputContextFocus() { m_pIUIPanel->SetInputContextFocus(); }

	// retrieve the style flags (map to CSS psuedo-classes) for this panel
	uint GetStyleFlags() const { return m_pIUIPanel->GetStyleFlags(); }
	void AddStyleFlag( EStyleFlags eStyleFlag ) { m_pIUIPanel->AddStyleFlag( eStyleFlag ); }
	void RemoveStyleFlag( EStyleFlags eStyleFlag ) { m_pIUIPanel->RemoveStyleFlag( eStyleFlag ); }
	bool IsInspected() const { return m_pIUIPanel->IsInspected(); }
	bool BHasHoverStyle() const { return m_pIUIPanel->BHasHoverStyle(); }
	void SetSelected( bool bSelected ) { m_pIUIPanel->SetSelected( bSelected ); }
	bool IsSelected() const { return m_pIUIPanel->IsSelected(); }
	bool BHasKeyFocus() const { return m_pIUIPanel->BHasKeyFocus(); }
	bool BHasDescendantKeyFocus() const { return m_pIUIPanel->BHasDescendantKeyFocus(); }
	bool IsLayoutLoading() const { return m_pIUIPanel->IsLayoutLoading(); }

	// enable/disable
	void SetEnabled( bool bEnabled ) { m_pIUIPanel->SetEnabled( bEnabled ); }
	bool IsEnabled() const { return m_pIUIPanel->IsEnabled(); }

	bool IsActivationEnabled() { return m_pIUIPanel->IsActivationEnabled(); }

	// Set activation disabled on this panel, input/focus still generally work, but Activate events won't be handled, useful to prevent a button
	// being clicked when out of focus, but leave it able to be focused for later activation or such
	void SetActivationEnabled( bool bEnabled ) { m_pIUIPanel->SetActivationEnabled( bEnabled ); }

	// Set all our immediate children enabled/disabled
	void SetAllChildrenActivationEnabled( bool bEnabled ) { m_pIUIPanel->SetAllChildrenActivationEnabled( bEnabled ); }

	// Enable/disable hit testing of this panel, you may want a parent that is never hit test that has a large region, but clicks
	// just pass through to other things behind it.  Children may still hit test.
	void SetHitTestEnabled( bool bEnabled ) { m_pIUIPanel->SetHitTestEnabled( bEnabled ); }
	bool BHitTestEnabled() const { return m_pIUIPanel->BHitTestEnabled(); }
	void SetHitTestEnabledTraverse( bool bEnabled ) { m_pIUIPanel->SetHitTestEnabledTraverse( bEnabled ); }

	void SetOnActivateEvent( IUIEvent *pEvent );
	void SetOnActivateEvent( const char *pchEventString );
	void SetOnFocusEvent( IUIEvent *pEvent );
	void SetOnCancelEvent( IUIEvent *pEvent );
	void SetOnContextMenuEvent( IUIEvent *pEvent );
	void SetOnLoadEvent( IUIEvent *pEvent );
	void SetOnMouseActivateEvent( IUIEvent *pEvent );
	void SetOnMouseOverEvent( IUIEvent *pEvent );
	void SetOnMouseOutEvent( IUIEvent *pEvent );
	void SetOnDblClickEvent( IUIEvent *pEvent );
	void SetOnTabForwardEvent( IUIEvent *pEvent );
	void SetOnTabBackwardEvent( IUIEvent *pEvent );

	// bugbug jmccaskey - DELETE ME	
	// bugbug jmccaskey - both of the next two functions need to be deleted, we should
	// not expose panel event internals like this, but parental button needs some refactoring
	// first.  That refactoring must happen to work at all with javascript panel events anyway.
	VecUIEvents_t * GetPanelEvents( CPanoramaSymbol symEvent ) { return m_pIUIPanel->GetPanelEvents( symEvent ); }
	virtual void OnPanelEventSet( CPanoramaSymbol symEvent ) OVERRIDE { }

	bool BHasOnActivateEvent() { return m_pIUIPanel->BHasOnActivateEvent(); }
	bool BHasOnMouseActivateEvent() { return m_pIUIPanel->BHasOnMouseActivateEvent(); }

	void ClearOnActivateEvent();

	// Dialog variables
	void SetDialogVariable( const char *pchKey, const char *pchValue );
	void SetDialogVariable( const char *pchKey, int iVal );
	// We do NOT have a uint32 type here by design, to prevent you accidenatlly using a RTime32
	// and getting a number value. Either cast to int for a number or construct a CRTime
#if defined (SOURCE2_PANORAMA )
	void SetDialogVariable( const char *pchKey, time_t timeVal );
	void SetDialogVariable( const char *pchKey, CCurrencyAmount amount );
#else
	void SetDialogVariable( const char *pchKey, CAmount amount );
	void SetDialogVariable( const char *pchKey, CRTime timeVal );
#endif
	void SetDialogVariable( const char *varName, const CUtlString &value );
	void SetDialogVariableLocString( const char *varName, const char *pchValue );



	// Scrolling controls
	void ScrollToTop() { m_pIUIPanel->ScrollToTop(); }
	void ScrollToBottom() { m_pIUIPanel->ScrollToBottom(); }
	void ScrollToLeftEdge() { m_pIUIPanel->ScrollToLeftEdge(); }
	void ScrollToRightEdge() { m_pIUIPanel->ScrollToRightEdge(); }
	void ScrollParentToMakePanelFit( ScrollBehavior_t behavior = SCROLL_BEHAVIOR_DEFAULT, bool bImmediateScroll = false ) { m_pIUIPanel->ScrollParentToMakePanelFit( behavior, bImmediateScroll ); }
	void ScrollToFitRegion( float x0, float x1, float y0, float y1, ScrollBehavior_t behavior = SCROLL_BEHAVIOR_DEFAULT, bool bDirectParentScrollOnly = false, bool bImmediateScroll = false ) { m_pIUIPanel->ScrollToFitRegion( x0, x1, y0, y1, behavior, bDirectParentScrollOnly, bImmediateScroll ); }
	bool BCanSeeInParentScroll() { return m_pIUIPanel->BCanSeeInParentScroll(); }
	
	void OnScrollPositionChanged() { m_pIUIPanel->OnScrollPositionChanged(); }
	void SetSendChildScrolledIntoViewEvents( bool bSendChildScrolledIntoViewEvents ) { m_pIUIPanel->SetSendChildScrolledIntoViewEvents( bSendChildScrolledIntoViewEvents ); }	// this must be enabled on your parent for ScrolledIntoView events to fire and IsScrolledIntoView state to be set
	void FireScrolledIntoViewEvent() { m_pIUIPanel->FireScrolledIntoViewEvent(); }
	void FireScrolledOutOfViewEvent() { m_pIUIPanel->FireScrolledOutOfViewEvent(); }
	bool IsScrolledIntoView() const { return m_pIUIPanel->IsScrolledIntoView(); }

	bool BSelectionPosVerticalBoundary() { return m_pIUIPanel->BSelectionPosVerticalBoundary(); }
	bool BSelectionPosHorizontalBoundary() { return m_pIUIPanel->BSelectionPosHorizontalBoundary(); }

	// Tell panel to set focus to next panel in specified movement direction/type
	bool SetFocusToNextPanel( int nRepeats, EFocusMoveDirection moveType, bool bAllowWrap, float flTabIndexCurrent, float flXPosCurrent, float flYPosCurrent, float flXStart, float flYStart ) { return m_pIUIPanel->SetFocusToNextPanel( nRepeats, moveType, bAllowWrap, flTabIndexCurrent, flXPosCurrent, flYPosCurrent, flXStart, flYStart ); }
	virtual bool OnSetFocusToNextPanel( int nRepeats, EFocusMoveDirection moveType, bool bAllowWrap, float flTabIndexCurrent, float flXPosCurrent, float flYPosCurrent, float flXStart, float flYStart ) OVERRIDE{ return false; }
	bool SetInputFocusToFirstOrLastChildInFocusOrder( EFocusMoveDirection moveType, float flXStart, float flYStart ) { return m_pIUIPanel->SetInputFocusToFirstOrLastChildInFocusOrder( moveType, flXStart, flYStart ); }

	// hierarchy
	void SetParent( CPanel2D *pParent ) { m_pIUIPanel->SetParent( pParent ? pParent->UIPanel() : NULL ); }
	void RemoveAndDeleteChildren() { m_pIUIPanel->RemoveAndDeleteChildren(); }
	void RemoveAndDeleteChildrenOfType( CPanoramaSymbol symPanelType ) { m_pIUIPanel->RemoveAndDeleteChildrenOfType( symPanelType ); }
	uint32 GetChildCountOfType( CPanoramaSymbol symPanelType ) { return m_pIUIPanel->GetChildCountOfType( symPanelType ); }
	bool IsDescendantOf( const CPanel2D *pPanel ) const { return m_pIUIPanel->IsDescendantOf( pPanel ? pPanel->m_pIUIPanel : NULL ); }
	CPanel2D *FindAncestor( const char *pchID ) { return ToPanel2D( m_pIUIPanel->FindAncestor( pchID ) );  }

	// layout file
	CPanoramaSymbol GetLayoutFileLoadedFrom() const { return m_pIUIPanel->GetLayoutFileLoadedFrom(); }

	// Override this and return true if you know your panel will never draw outside it's bounds,
	// thus allowing an optimization to skip pushing clipping layers.
	virtual bool BRequiresContentClipLayer() OVERRIDE { return false; }

	// debug
	virtual void GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties );

	void SetTooltip( CPanel2D *pPanel );

	// Panel events
	bool DispatchPanelEvent( CPanoramaSymbol symPanelEvent ) { return m_pIUIPanel->DispatchPanelEvent( symPanelEvent ); }
	bool BParsePanelEvent( CPanoramaSymbol symPanelEvent, const char *pchValue ) { return m_pIUIPanel->BParsePanelEvent( symPanelEvent, pchValue ); }
	bool BIsPanelEventSet( CPanoramaSymbol symPanelEvent ) { return m_pIUIPanel->BIsPanelEventSet( symPanelEvent ); }
	bool BIsPanelEvent( CPanoramaSymbol symProperty ) { return m_pIUIPanel->BIsPanelEvent( symProperty ); }

	// the input namespace to use for this panel
	const char *GetInputNamespace() const {	return m_pIUIPanel->GetInputNamespace(); }

	// the mouse cursor to display when hovered
	virtual EMouseCursors GetMouseCursor() OVERRIDE { return eMouseCursor_Arrow; }

	// controls if clicking on an unfocused panel should set focus
	void SetMouseCanActivate( EMouseCanActivate eMouseCanActivate, const char *pchOptionalParent = NULL ) { m_pIUIPanel->SetMouseCanActivate( eMouseCanActivate, pchOptionalParent ); }
	EMouseCanActivate GetMouseCanActivate() { return m_pIUIPanel->GetMouseCanActivate(); }
	CPanel2D *FindParentForMouseCanActivate() { return ToPanel2D( m_pIUIPanel->FindParentForMouseCanActivate() ); }

	// controls if clicking on an unfocused panel should set focus
	void SetChildFocusOnHover( bool bEnable ) { m_pIUIPanel->SetChildFocusOnHover( bEnable ); }
	bool GetChildFocusOnHover() { return m_pIUIPanel->GetChildFocusOnHover(); }

	// Set background images for the panel
	void SetBackgroundImages( const CUtlVector< CBackgroundImageLayer * > &vecLayers );
	CUtlVector< CBackgroundImageLayer * > *GetBackgroundImages();

	// called once after registering with UIEngine::CallBeforeStyleAndLayout()
	virtual void OnCallBeforeStyleAndLayout() OVERRIDE {}

	// clone
	virtual bool IsClonable();
	virtual CPanel2D *Clone();

	// sort children
	void SortChildren( int( __cdecl *pfnCompare )(const ClientPanelPtr_t *, const ClientPanelPtr_t *) ) { m_pIUIPanel->SortChildren( pfnCompare ); }

	// set the namespace to use for input
	void SetInputNamespace( const char *pchNamespace ) { m_pIUIPanel->SetInputNamespace( pchNamespace ); }

	// override this if you need to have the loc system consider an alternate panel hierarchy for resolving dialog variables
	virtual IUIPanel *GetLocalizationParent() const OVERRIDE { return GetParent() ? GetParent()->m_pIUIPanel : NULL; }

	// child management, use with caution! normally always managed internally.
	void AddChild( CPanel2D *pChild ) { m_pIUIPanel->AddChild( pChild->UIPanel() ); }

	// child management, use with caution! normally always managed internally.  Returns child index we inserted at.
	int AddChildSorted( bool( __cdecl *pfnLessFunc )(ClientPanelPtr_t const &p1, ClientPanelPtr_t const &p2), CPanel2D *pChild ) { return m_pIUIPanel->AddChildSorted( pfnLessFunc, pChild->UIPanel() ); }

	// child management, use with caution! normally always managed internally.
	void RemoveChild( CPanel2D *pChild ) { m_pIUIPanel->RemoveChild( pChild->UIPanel() ); }

	// Check if styles are dirty for the panel
	bool BStylesDirty() const { return m_pIUIPanel->BStylesDirty(); }
	
	// Check if styles are possibly dirty for any of our children
	bool BChildStylesDirty() { return m_pIUIPanel->BChildStylesDirty(); }

	// Set that we need an on styles changed call when styles become non-dirty, even if there is no actual change.
	void SetOnStylesChangedNeeded() { m_pIUIPanel->SetOnStylesChangedNeeded(); }


	// Getter for panel attributes
	int GetAttribute( const char *pchAttrName, int nDefaultValue ) { return m_pIUIPanel->GetAttribute( pchAttrName, nDefaultValue ); }

	// Getter for panel attributes
	const char *GetAttribute( const char *pchAttrName, const char * pchDefaultValue ) { return m_pIUIPanel->GetAttribute( pchAttrName, pchDefaultValue ); }

	// Getter for panel attributes
	uint32 GetAttribute( const char *pchAttrName, uint32 unDefaultValue ) { return m_pIUIPanel->GetAttribute( pchAttrName, unDefaultValue ); }

	// Getter for panel attributes
	uint64 GetAttribute( const char *pchAttrName, uint64 unDefaultValue ) { return m_pIUIPanel->GetAttribute( pchAttrName, unDefaultValue ); }

	// Setter for panel attributes
	void SetAttribute( const char *pchAttrName, int nValue ) { m_pIUIPanel->SetAttribute( pchAttrName, nValue ); }

	// Setter for panel attributes
	void SetAttribute( const char *pchAttrName, const char * pchValue ) { m_pIUIPanel->SetAttribute( pchAttrName, pchValue ); }

	// Setter for panel attributes
	void SetAttribute( const char *pchAttrName, uint32 unValue ) { m_pIUIPanel->SetAttribute( pchAttrName, unValue ); }

	// Setter for panel attributes
	void SetAttribute( const char *pchAttrName, uint64 unValue ) { m_pIUIPanel->SetAttribute( pchAttrName, unValue ); }

	// walks parents calculating the top left corner relative to window space
	void GetPositionWithinWindow( float *pflX, float *pflY );

	// walks parents calculating the top left corner relative to the ancestor's space. If
	// the passed in panel is NULL or not an ancestor, this will end up being relative to the window space 
	virtual void GetPositionWithinAncestor( CPanel2D *pAncestor, float *pflX, float *pflY ) OVERRIDE;

	bool BHasAnyActiveTransitions();

	// Get the nearest parent that establishes a javascript context, or return ourself if we ourselves create one
	panorama::CPanel2D *GetJavaScriptContextParent() const { return (CPanel2D*)(m_pIUIPanel->GetJavaScriptContextParent()->ClientPtr()); }

	// Called to ask us to setup object template for Javascript, you can implement this in a child class and then call
	// the base method (so all the normal panel2d stuff gets exposed), plus call the various RegisterJS helpers yourself
	// to expose additional panel type specific data/methods.
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// Callback to client panel to create a scrollbar
	virtual IUIScrollBar *CreateNewVerticalScrollBar( float flInitialScrollPos ) OVERRIDE;

	// Callback to client panel to create a scrollbar
	virtual IUIScrollBar *CreateNewHorizontalScrollBar( float flInitialScrollPos ) OVERRIDE;

	// Callback to hide tooltip if it's visible
	virtual void HideTooltip() OVERRIDE;

	// Has this panel ever been layed out
	virtual bool BHasBeenLayedOut() const { return m_pIUIPanel->BHasBeenLayedOut(); }

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
	void Validate( CValidator &validator, const tchar *pchName );
	static void ValidateStatics( CValidator &validator, const char *pchName );
#endif

	void SetLayoutLoadedFromParent( CPanel2D *pParent ) { m_pIUIPanel->SetLayoutLoadedFromParent( pParent ? pParent->UIPanel() : NULL ); }

	IUIImageManager *UIImageManager() { return m_pIUIPanel->UIImageManager(); }

protected:
	friend class CPanelStyle;
	friend class CStyleFileSet;
	friend class CVerticalScrollBar;
	friend class CHorizontalScrollBar;

	virtual IUIRenderEngine *AccessRenderEngine() { return m_pIUIPanel->GetParentWindow()->UIRenderEngine(); }

	void FirePanelLoadedEvent() { m_pIUIPanel->FirePanelLoadedEvent();  }

	// override to change how this panel is measured
	virtual void OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions ) OVERRIDE
	{ 
		m_pIUIPanel->OnContentSizeTraverse( pflContentWidth, pflContentHeight, flMaxWidth, flMaxHeight, bFinalDimensions );  
	}

	// override to add additional panel events
	virtual bool BIsClientPanelEvent( CPanoramaSymbol symProperty ) OVERRIDE;

	// override to change how this panel arranges its children
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE { m_pIUIPanel->OnLayoutTraverse( flFinalWidth, flFinalHeight ); }
	
	virtual void OnStylesChanged() OVERRIDE { if ( m_pIUIPanel->GetParent() ) { m_pIUIPanel->GetParent()->ClientPtr()->OnChildStylesChanged(); } }
	virtual void OnVisibilityChanged() OVERRIDE {}

	virtual void OnChildStylesChanged() OVERRIDE { }

	// methods for setting properties from a layout file. Default BSetProperties calls BSetProperty on each element.
	virtual bool BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties ) OVERRIDE;
	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;


	virtual void OnBeforeChildrenChanged() OVERRIDE {}
	virtual void OnAfterChildrenChanged() OVERRIDE {}
	virtual void OnRemoveChild( IUIPanel *pChild ) OVERRIDE {}

	virtual void OnInitializedFromLayout() OVERRIDE {}

	void SetLayoutFile( CPanoramaSymbol symLayoutFile ) { m_pIUIPanel->SetLayoutFile( symLayoutFile ); }
	void SetLayoutFileTraverse( CPanoramaSymbol symLayoutFile );

	void SetMouseTracking( bool bState ) { m_pIUIPanel->SetMouseTracking( bState ); }
	
	// for cloning
	bool AreChildrenClonable();
	virtual void InitClonedPanel( CPanel2D *pPanel );

	// for setting parent disabled style flag	
	//bugbug jmccaskey - these are broken... would like to fix so they are uneeded
	virtual void AddDisabledFlagToChildren() { }
	virtual void RemoveDisabledFlagFromChildren() { }

	// Pointer to basic IUIPanel interface from panorama
	IUIPanel *m_pIUIPanel;

private:
	// we by design don't have a uint32 overload of this, use this function here to enforce it
	void SetDialogVariable( const char *pchKey, uint32 nInvalid ) { Assert( !"Invalid call" ); }

	CUtlVector<IUIPanel *> const &AccessChildren() { return m_pIUIPanel->AccessChildren(); }
	CUtlVector<IUIPanel *> const &JSFindChildrenWithClassTraverse( const char *pchClass );

	
	// event handler functions, these CANNOT be virtual, if you need to override then
	// have this function call into another helper that is virtual to override behavior
	bool EventAppendChildrenFromLayoutFileAsync( const CPanelPtr< IUIPanel > &pPanel, const char *pchLayoutFile );
	bool EventAddStyleClass( const CPanelPtr< IUIPanel > &pPanel, const char *pchName );
	bool EventRemoveStyleClass( const CPanelPtr< IUIPanel > &pPanel, const char *pchName );
	bool EventToggleStyleClass( const CPanelPtr< IUIPanel > &pPanel, const char *pchName );
	bool EventAddStyleClassToEachChild( const CPanelPtr< IUIPanel > &pPanel, const char *pchName );
	bool EventRemoveStyleClassFromEachChild( const CPanelPtr< IUIPanel > &pPanel, const char *pchName );
	bool EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventPanelCancelled( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventPanelContextMenu( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventPanelLoaded( const CPanelPtr< IUIPanel > &pPanel );
	bool EventShowTooltip( const CPanelPtr< IUIPanel > &pPanel );
	bool EventScrollUp()		{ return OnScrollUp(); }
	bool EventScrollDown()		{ return OnScrollDown(); }
	bool EventScrollLeft()		{ return OnScrollLeft(); }
	bool EventScrollRight()		{ return OnScrollRight(); }
	bool EventScrollPanelUp( const CPanelPtr< IUIPanel > &pPanel )		{ return OnScrollUp(); }
	bool EventScrollPanelDown( const CPanelPtr< IUIPanel > &pPanel )	{ return OnScrollDown(); }
	bool EventScrollPanelLeft( const CPanelPtr< IUIPanel > &pPanel )	{ return OnScrollLeft(); }
	bool EventScrollPanelRight( const CPanelPtr< IUIPanel > &pPanel )	{ return OnScrollRight(); }
	bool EventPageUp()			{ return OnPageUp(); }
	bool EventPageDown()		{ return OnPageDown(); }
	bool EventPageLeft()		{ return OnPageLeft(); }
	bool EventPageRight()		{ return OnPageRight(); }
	bool EventPagePanelUp( const CPanelPtr< IUIPanel > &pPanel )	{ return OnPageUp(); }
	bool EventPagePanelDown( const CPanelPtr< IUIPanel > &pPanel )	{ return OnPageDown(); }
	bool EventPagePanelLeft( const CPanelPtr< IUIPanel > &pPanel )	{ return OnPageLeft(); }
	bool EventPagePanelRight( const CPanelPtr< IUIPanel > &pPanel )	{ return OnPageRight(); }
	bool EventScrollToTop( const CPanelPtr< IUIPanel > &pPanel );
	bool EventScrollToBottom( const CPanelPtr< IUIPanel > &pPanel );
	bool EventMoveUp( int nRepeats )			{ return OnMoveUp( nRepeats ); }
	bool EventMoveDown( int nRepeats )		{ return OnMoveDown( nRepeats ); } 
	bool EventMoveLeft( int nRepeats )		{ return OnMoveLeft( nRepeats ); }
	bool EventMoveRight( int nRepeats )		{ return OnMoveRight( nRepeats ); }
	bool EventTabForward( int nRepeats )		{ return OnTabForward( nRepeats ); }
	bool EventTabBackward( int nRepeats )		{ return OnTabBackward( nRepeats ); }	
	bool EventImageLoaded( const CPanelPtr< IUIPanel > &pPanel, IImageSource *pImage );
	bool EventImageFailedLoad( const CPanelPtr< IUIPanel > &pPanel, IImageSource *pImage );
	bool EventSetPanelEvent( const CPanelPtr< IUIPanel > &pPanel, const char *pchPanelEventName, const char *pchPanelEventAction );
	bool EventClearPanelEvent( const CPanelPtr< IUIPanel > &pPanel, const char *pchPanelEventName );
	bool EventIfHasClassEvent( const CPanelPtr< IUIPanel > &pPanel, const char * pchClassName, IUIEvent * pEventToFire );
	bool EventIfNotHasClassEvent( const CPanelPtr< IUIPanel > &pPanel, const char * pchClassName, IUIEvent * pEventToFire );
	bool EventIfHoverOtherEvent( const CPanelPtr< IUIPanel > &pPanel, const char *pchOtherPanelID, IUIEvent * pEventToFire );
	bool EventIfNotHoverOtherEvent( const CPanelPtr< IUIPanel > &pPanel, const char *pchOtherPanelID, IUIEvent * pEventToFire );
	bool EventIfHoverOverEventInternal( const CPanelPtr< IUIPanel > &pPanel, const char *pchOtherPanelID, IUIEvent * pEventToFire, bool bFireIfHovered );
	bool EventCheckChildrenScrolledIntoView( const CPanelPtr< IUIPanel > &pPanel ) { return m_pIUIPanel->OnCheckChildrenScrolledIntoView(); }
	bool EventScrollPanelIntoView( const CPanelPtr< IUIPanel > &pPanel, ScrollBehavior_t behavior, bool bImmediate );

	void Initialize( IUIWindow *window, CPanel2D *parent, const char *pchID, uint32 ePanelFlags );
	bool BAppyLayoutFile( CLayoutFile *pLayoutFile, CUtlVector< CPanel2D * > *pvecExistingPanels );
	void DeletePanelsForReloadTraverse( CPanoramaSymbol symPath, CUtlVector< CPanel2D * > *pvecPanelsWithID );
	
	// Is this a property we must create our children before applying during layout file application?
	virtual bool BIsDelayedProperty( CPanoramaSymbol symProperty ) OVERRIDE;

	void ClearPanelEventJS( CPanoramaSymbol symPanelEvent ) { m_pIUIPanel->ClearPanelEvents( symPanelEvent ); }

	// Private helper for JS attribute registration
	void RegisterJSFloatTabIndexSelectionPos( const char *pchjsMemberName, PanelFloatGetter_t pGetFunc, PanelFloatSetter_t pSetFunc );

	void SetDefaultFocusOnMouseDownBehavior();

	// tooltip for panel. Need to keep a safe pointer as the tooltip is a top level window and will be deleted at shutdown automatically
	CPanelPtr< CPanel2D > m_pTooltip;

	static CUtlVector<IUIPanel *> s_vecMatchingChildren;
};


class CUIScrollBar;


//-----------------------------------------------------------------------------
// Purpose: Base class for all types of scroll bars
//-----------------------------------------------------------------------------
class CBaseScrollBar : public CPanel2D
{
	DECLARE_PANEL2D( CBaseScrollBar, CPanel2D );

public:
	CBaseScrollBar( CPanel2D *parent, const char * pchPanelID );

	virtual ~CBaseScrollBar();

	IUIScrollBar *Interface();

	// Normalizes the position to be within range min/max 
	void Normalize( bool bImmediateThumbUpdate = false )
	{
		if ( m_flWindowStart < m_flRangeMin )
			m_flWindowStart = m_flRangeMin;

		if ( m_flWindowStart + m_flWindowSize > m_flRangeMax )
		{
			m_flWindowStart = m_flRangeMax - m_flWindowSize;
			m_flWindowStart = RoundFloatToInt( m_flWindowStart );
		}
		else
		{
			m_flWindowStart = RoundFloatToInt( m_flWindowStart );
		}

		UpdateLayout( bImmediateThumbUpdate );
	}

	// Set the scroll range 
	void SetRangeMinMax( float flRangeMin, float flRangeMax )
	{
		if ( m_flRangeMin != flRangeMin || m_flRangeMax != flRangeMax )
		{
			m_flRangeMin = flRangeMin;
			m_flRangeMax = flRangeMax;

			if ( GetParent() )
				GetParent()->InvalidatePosition();

			Normalize( false );
		}
	}

	// return the size of the range we have
	float GetRangeSize() const { return m_flRangeMax - m_flRangeMin; }
	float GetRangeMin() const { return m_flRangeMin; }
	float GetRangeMax() const { return m_flRangeMax; }

	// Set the window size
	void SetScrollWindowSize( float flWindowSize )
	{
		if ( m_flWindowSize != flWindowSize )
		{
			m_flWindowSize = flWindowSize;

			if ( GetParent() )
				GetParent()->InvalidatePosition();

			Normalize( false );
		}
	}

	// Get scroll window size
	float GetScrollWindowSize() { return m_flWindowSize; }

	// Set the current window position
	void SetScrollWindowPosition( float flWindowPos, bool bImmediateMove = false )
	{
		m_flWindowStart = flWindowPos;
		m_flLastScrollTime = UIEngine()->GetCurrentFrameTime();


		if ( GetParent() )
		{
			if ( GetParent()->IsChildPositionValid() && GetParent()->IsPositionValid() )
				Normalize( bImmediateMove );

			GetParent()->InvalidatePosition();
			GetParent()->OnScrollPositionChanged();
		}
	}

	double GetLastScrollTime() { return m_flLastScrollTime; }

	// Get scroll window position
	float GetScrollWindowPosition() { return m_flWindowStart; }

	bool BLastMoveImmediate() { return m_bLastMoveImmediate; }

protected:

	friend class CUIScrollBar;

	CUIScrollBar *m_pScrollBarInterface;

	virtual void UpdateLayout( bool bImmediateMove ) = 0;

	bool m_bLastMoveImmediate;

	double m_flLastScrollTime;
	float m_flRangeMin;
	float m_flRangeMax;
	float m_flWindowSize;
	float m_flWindowStart;
};

//-----------------------------------------------------------------------------
// Purpose: Default Scrollbar
//-----------------------------------------------------------------------------
class CScrollBar : public CBaseScrollBar
{
	DECLARE_PANEL2D( CScrollBar, CBaseScrollBar );

public:
	CScrollBar( CPanel2D *parent, const char * pchPanelID );
	virtual ~CScrollBar();

	virtual void ScrollToMousePos() = 0;

	virtual void OnMouseMove( float flMouseX, float flMouseY )
	{
		// If there's a transform set on the parent, adjust the input as appropriate.
		Vector vMousePosition( flMouseX, flMouseY, 0.0f );
		if ( GetParent() )
		{
			VMatrix matParentTransform = GetParent()->AccessStyle()->GetTransform3DMatrix();
			vMousePosition = matParentTransform * vMousePosition;
		}

		m_flMouseX = vMousePosition.x;
		m_flMouseY = vMousePosition.y;

		if ( m_bMouseDown )
			ScrollToMousePos();
	}

	virtual bool OnMouseButtonDown( const MouseData_t &code )
	{
		// Only interested in left clicks
		if ( code.m_MouseCode != MOUSE_LEFT )
			return BaseClass::OnMouseButtonDown( code );

		AddClass( "MouseDown" );
		
		if ( m_pScrollThumb->BHasHoverStyle() )
			m_bMouseWentDownOnThumb = true;
		else
			m_bMouseWentDownOnThumb = false;
		m_bMouseDown = true;
		m_flMouseStartX = m_flMouseX;
		m_flMouseStartY = m_flMouseY;
		m_flScrollStartPosition = GetScrollWindowPosition();
		ScrollToMousePos();

		return true;
	}

	virtual bool OnMouseButtonUp( const MouseData_t &code )
	{
		// Only interested in left clicks
		if ( code.m_MouseCode != MOUSE_LEFT )
			return BaseClass::OnMouseButtonDown( code );

		m_bMouseDown = false;
		ScrollToMousePos();

		RemoveClass( "MouseDown" );

		return true;
	}

protected:
	CPanel2D *m_pScrollThumb;

	bool m_bMouseDown;
	bool m_bMouseWentDownOnThumb;

	float m_flMouseX;
	float m_flMouseY;
	float m_flMouseStartX;
	float m_flMouseStartY;
	float m_flScrollStartPosition;
};


class CUIScrollBar : public IUIScrollBar
{
public:
	CUIScrollBar( CBaseScrollBar *pParent ) { m_pParent = pParent; }

	virtual IUIPanel* UIPanel() OVERRIDE { return m_pParent->UIPanel(); }
	virtual IUIPanelClient* ClientPtr() OVERRIDE { return m_pParent; }

	virtual void Normalize( bool bImmediateThumbUpdate ) OVERRIDE { return m_pParent->Normalize( bImmediateThumbUpdate ); }

	virtual void SetRangeMinMax( float flRangeMin, float flRangeMax ) OVERRIDE { return m_pParent->SetRangeMinMax( flRangeMin, flRangeMax ); }

	virtual float GetRangeSize() const OVERRIDE { return m_pParent->GetRangeSize(); }
	virtual float GetRangeMin() const OVERRIDE { return m_pParent->GetRangeMin(); }
	virtual float GetRangeMax() const OVERRIDE { return m_pParent->GetRangeMax(); }

		// Set the window size
	virtual void SetScrollWindowSize( float flWindowSize ) OVERRIDE { return m_pParent->SetScrollWindowSize( flWindowSize ); }

		// Get scroll window size
	virtual float GetScrollWindowSize() OVERRIDE { return m_pParent->GetScrollWindowSize(); }

		// Set the current window position
	virtual void SetScrollWindowPosition( float flWindowPos, bool bImmediateMove = false ) OVERRIDE { return m_pParent->SetScrollWindowPosition( flWindowPos, bImmediateMove ); }

	virtual float GetLastScrollTime() OVERRIDE { return m_pParent->GetLastScrollTime(); }

	// Get scroll window position
	virtual float GetScrollWindowPosition() OVERRIDE { return m_pParent->GetScrollWindowPosition(); }

	// Return true if the user is manually dragging the scrollbar with the mouse
	virtual bool BLastMoveImmediate() OVERRIDE { return m_pParent->BLastMoveImmediate(); }

private:
	CBaseScrollBar *m_pParent;
};


//-----------------------------------------------------------------------------
// Purpose: Vertical scroll bar
//-----------------------------------------------------------------------------
class CVerticalScrollBar : public CScrollBar
{
	DECLARE_PANEL2D( CVerticalScrollBar, CScrollBar );

public:
	CVerticalScrollBar( CPanel2D *parent, const char * pchPanelID ) : CScrollBar( parent, pchPanelID ) 
	{
		m_pScrollThumb->AddClass( "VerticalScrollThumb" );
	}
	
	void ScrollToMousePos()
	{
		CPanel2D *pPanel = GetParent();
		if ( pPanel )
		{
			float flHeight = GetActualLayoutHeight();
			if ( flHeight > 0.00001f )
			{
				if ( m_bMouseWentDownOnThumb )
				{
					float flPercentDiff = (m_flMouseY - m_flMouseStartY) / flHeight;
					float flPositionOffset = flPercentDiff * pPanel->GetContentHeight();
					float flPosition = m_flScrollStartPosition + flPositionOffset;
					SetScrollWindowPosition( clamp( flPosition, 0.0f, pPanel->GetContentHeight() - GetScrollWindowSize() ), true );
				}
				else
				{
					float flPercent = m_flMouseY / flHeight;
					float flPos = pPanel->GetContentHeight() * flPercent;
					SetScrollWindowPosition( clamp( flPos, 0.0f, pPanel->GetContentHeight() - GetScrollWindowSize() ), true );
				}
			}
		}
	}


	virtual ~CVerticalScrollBar() {}

protected:
	virtual void UpdateLayout( bool bImmediateMove );
	
};


//-----------------------------------------------------------------------------
// Purpose: Horizontal scroll bar
//-----------------------------------------------------------------------------
class CHorizontalScrollBar : public CScrollBar
{
	DECLARE_PANEL2D( CHorizontalScrollBar, CScrollBar );

public:
	CHorizontalScrollBar( CPanel2D *parent, const char * pchPanelID ) : CScrollBar( parent, pchPanelID ) 
	{
		m_pScrollThumb->AddClass( "HorizontalScrollThumb" );
	}

	void ScrollToMousePos()
	{
		CPanel2D *pPanel = GetParent();
		if ( pPanel )
		{
			float flWidth = GetActualLayoutWidth();
			if ( flWidth > 0.00001f )
			{
				if ( m_bMouseWentDownOnThumb )
				{
					float flPercentDiff = ( m_flMouseX - m_flMouseStartX ) / flWidth;
					float flPositionOffset = flPercentDiff * pPanel->GetContentWidth();
					float flPosition = m_flScrollStartPosition + flPositionOffset;
					SetScrollWindowPosition( clamp( flPosition, 0.0f, pPanel->GetContentWidth() - GetScrollWindowSize() ), true );
				}
				else
				{
					float flPercent = m_flMouseX / flWidth;
					float flPos = pPanel->GetContentWidth() * flPercent;
					SetScrollWindowPosition( clamp( flPos, 0.0f, pPanel->GetContentWidth() - GetScrollWindowSize() ), true );
				}
			}
		}
	}

	virtual ~CHorizontalScrollBar() {}

protected:

	virtual void UpdateLayout( bool bImmediateMove );
};

#pragma warning(pop)


} // namespace panorama

#endif // PANEL2D_H
