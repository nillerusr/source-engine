//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUIPANEL_H
#define IUIPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "panoramatypes.h"
#include "panoramasymbol.h"
#include "iuirenderengine.h"
#include "layout/stylefiletypes.h"
#include "layout/uilength.h"
#include "tier1/utlstring.h"

namespace panorama
{

class CLayoutFile;
class IUIPanelStyle;
class IUIPanelClient;
class IUIWindow;
class IUIImageManager;
class IUI3DSurface;
class IUIEvent;
class IUIScrollBar;
typedef panorama::IUIPanelClient * ClientPanelPtr_t;
typedef CUtlVector< panorama::IUIEvent * > VecUIEvents_t;

//-----------------------------------------------------------------------------
// Purpose: Used to provide property info to debugger for output
//-----------------------------------------------------------------------------
struct DebugPropertyOutput_t
{
	DebugPropertyOutput_t() {}
	DebugPropertyOutput_t( const char *pchName, const char *pchValue ) : m_strName( pchName ), m_strValue( pchValue ) {}

	CUtlString m_strName;
	CUtlString m_strValue;
};


enum ScrollBehavior_t
{
	SCROLL_BEHAVIOR_SCROLL_MINIMUM_DISTANCE = 0,
	SCROLL_BEHAVIOR_SCROLL_TO_TOPLEFT_EDGE,
	SCROLL_BEHAVIOR_SCROLL_TO_BOTTOMRIGHT_EDGE,
	SCROLL_BEHAVIOR_SCROLL_TO_CENTER,

	SCROLL_BEHAVIOR_DEFAULT = SCROLL_BEHAVIOR_SCROLL_MINIMUM_DISTANCE,
};

//-----------------------------------------------------------------------------
// Purpose: Basic panel interface exposing operations used inside of panorama, rather
// than operations that are part of building/laying out controls in the panorama_client module
//-----------------------------------------------------------------------------
class IUIPanel
{
public:
	virtual ~IUIPanel() {}

	// Initialize panel
	virtual void Initialize( IUIWindow *window, IUIPanel *parent, const char *pchID, uint32 ePanelFlags ) = 0;

	// Initialize a cloned panel from ourself
	virtual void InitClonedPanel( IUIPanel *pClone ) = 0;

	// Do panel2d event type registrations 
	virtual void RegisterEventHandlersOnPanel2DType( CPanoramaSymbol symPanelType ) = 0;

	// Shutdown panel, should only happen right before actual deletion
	virtual void Shutdown() = 0;

	// Fire panel loaded event and mark us as now loaded, images and other panels may call this late
	virtual void FirePanelLoadedEvent() = 0;

	// Gets client type pointer value
	virtual void SetClientPtr( panorama::IUIPanelClient *pPtr ) = 0;
	virtual panorama::IUIPanelClient * ClientPtr() const = 0;
	
	// Set panel id
	virtual void SetID( const char *pchID ) = 0;

	// Get panel id
	virtual const char *GetID() const = 0;

	// Get panel type, which really calls back into client panel object
	virtual CPanoramaSymbol GetPanelType() const = 0;

	// Check for valid panel id
	virtual bool BHasID() const = 0;

	// sets & loads the layout file for this panel
	virtual bool BLoadLayout( const char *pchFile, bool bOverrideExisting = false, bool bPartialLayout = false ) = 0;

	// sets & loads the layout for this panel
	virtual bool BLoadLayoutFromString( const char *pchXMLString, bool bOverrideExisting = false, bool bPartialLayout = false ) = 0;

	// sets loads the layout file for this panel, asynchronously supporting remote http:// paths
	virtual void LoadLayoutAsync( const char *pchFile, bool bOverrideExisting = false, bool bPartialLayout = false ) = 0;

	// loads the layout file for this panel, asynchronously supporting remote http:// paths in css within
	virtual void LoadLayoutFromStringAsync( const char *pchXMLString, bool bOverrideExisting, bool bPartialLayout = false ) = 0;

	// Check if the panel has loaded layout
	virtual bool IsLoaded() const = 0;

	// Set panel's immediate parent
	virtual void SetParent( panorama::IUIPanel *pParent ) = 0;

	// Get panel's immediate parent
	virtual panorama::IUIPanel *GetParent() const = 0;

	// Get panel's parent window
	virtual panorama::IUIWindow *GetParentWindow() const = 0;

	// Set panel visibility
	virtual void SetVisible( bool bVisible ) = 0;

	// Is the panel visible (not accounting for opacity)
	virtual bool BIsVisible() const = 0;

	// Is the panel transparent (may be 'visible' and affect layout, but fully transparent)
	virtual bool BIsTransparent() const = 0;

	// Set the layout file this panel is loaded from based on a parent, shouldn't need this directly normally
	virtual void SetLayoutLoadedFromParent( panorama::IUIPanel *pParent ) = 0;

	// Returns the layout file for this panel (ie, the one it loaded if any)
	virtual panorama::CPanoramaSymbol GetLayoutFile() const = 0;

	// Returns layout file that loaded us which may have been loaded by an ancestor rather than direct parent
	virtual panorama::CPanoramaSymbol GetLayoutFileLoadedFrom() const = 0;

	// Get the current reload count for the panel's layout file - can be used to detect when a reload has occurred
	// Returns -1 if no layout file.
	virtual int GetLayoutFileReloadCount() const = 0;

	// searches only immediate children
	virtual panorama::IUIPanel *FindChild( const char *pchID ) = 0;

	// searches all children even outside layout file scope
	virtual panorama::IUIPanel *FindChildTraverse( const char *pchID ) = 0;

	// searches any children created from our layout file
	virtual IUIPanel *FindChildInLayoutFile( const char *pchID ) = 0;

	// searches any panel created from our layout file (so parents or children!)
	virtual IUIPanel *FindPanelInLayoutFile( const char *pchID ) = 0;

	// Check if this panel is a descendant of the passed panel
	virtual bool IsDescendantOf( const panorama::IUIPanel *pPanel ) const = 0;

	// Remove and delete all children from panel
	virtual void RemoveAndDeleteChildren() = 0;

	// Remove and delete all children matching type
	virtual void RemoveAndDeleteChildrenOfType( CPanoramaSymbol symPanelType ) = 0;

	// Child access
	virtual int GetChildCount() const = 0;
	virtual IUIPanel *GetChild( int i ) const = 0;
	virtual IUIPanel *GetFirstChild() const = 0;
	virtual IUIPanel *GetLastChild() const = 0;

	// Return index of child in creation/panel vector order (also default tab order)
	virtual int GetChildIndex( const IUIPanel *pChild ) const = 0;

	// Get child count of specific type
	virtual uint32 GetChildCountOfType( CPanoramaSymbol symPanelType ) = 0;

	// For special children to be rendered in debugger
	virtual int GetHiddenChildCount() const = 0;
	virtual IUIPanel *GetHiddenChild( int i ) = 0;

	// Find ancestor with matching id
	virtual IUIPanel *FindAncestor( const char *pchID ) = 0;

	// Set the panels repaint state
	virtual void SetRepaint( panorama::EPanelRepaint eRepaintNeeded ) = 0;

	// Check if we should draw this child
	virtual bool BShouldDrawChild( panorama::IUIPanel *pChild ) = 0;

	// Apply layout file to panel
	virtual bool BAppyLayoutFile( panorama::CLayoutFile *pLayoutFile, CUtlVector< panorama::IUIPanel * > *pvecExistingPanels ) = 0;

	// Enable or disable background movies on panel 
	virtual void EnableBackgroundMovies( bool bEnabled ) = 0;

	// Access panel's style information
	virtual panorama::IUIPanelStyle * AccessIUIStyle() = 0;

	// Returns potentially dirty styles data, use for setting during initial construction/setup - or if you want to explicitly ignore transitions, but never for getting and generally not for setting!
	virtual panorama::IUIPanelStyle *AccessIUIStyleDirty() = 0;
	
	// Apply styles on the panel, resolving them all fully and updating actual panel style
	virtual void ApplyStyles( bool bTraverse ) = 0;

	// Set that we want an on styles changed event when styles are applied even if styles aren't actually dirty
	virtual void SetOnStylesChangedNeeded() = 0;

	// Access all children directly
	virtual CUtlVector<panorama::IUIPanel *> const &AccessChildren() = 0;

	// Measure self and children. First pass of layout
	virtual void DesiredLayoutSizeTraverse( float flMaxWidth, float flMaxHeight ) = 0;
	virtual void DesiredLayoutSizeTraverse( float *pflDesiredWidth, float *pflDesiredHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions ) = 0;

	// Tell us what your content size is
	virtual void OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions ) = 0;

	// Arrange children. Second pass of layout
	virtual void LayoutTraverse( float x, float y, float flFinalWidth, float flFinalHeight ) = 0;

	// Tell us how to actually layout children
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) = 0;

	// called by flowing and custom layout passes to set position. Will properly handle transition being applied with styles
	virtual void SetPositionFromLayoutTraverse( CUILength x, CUILength y, CUILength z ) = 0;

	// methods to invalid certain parts of layout
	virtual void InvalidateSizeAndPosition() = 0;
	virtual void InvalidatePosition() = 0;
	virtual void SetActiveSizeAndPositionTransition() = 0;
	virtual void SetActivePositionTransition() = 0;
	virtual bool IsSizeValid() = 0;
	virtual bool IsPositionValid() = 0;
	virtual bool IsChildSizeValid() = 0;
	virtual bool IsChildPositionValid() = 0;
	virtual bool IsSizeTransitioning() = 0;
	virtual bool IsPositionTransitioning() = 0;
	virtual bool IsChildPositionTransitioning() = 0;
	virtual bool IsChildSizeTransitioning() = 0;
	virtual void TransitionPositionApplied( bool bImmediate ) = 0;

	// size getters
	virtual float GetDesiredLayoutWidth() const = 0;
	virtual float GetDesiredLayoutHeight() const = 0;

	// Content size is what our contents actually take up, not accounting for fixed/relative
	// size set on us in styles which affect desired layout size
	virtual float GetContentWidth() const = 0;
	virtual float GetContentHeight() const = 0;

	// Actual size is the size given to the panel after layout, hopefully as big as its desired size.  
	// Actual size does NOT include margins (which are really in the parent).
	virtual float GetActualLayoutWidth() const = 0;
	virtual float GetActualLayoutHeight() const = 0;

	// Render size is the size of the content for rendering, this is either the actual layout size, or if
	// that is smaller than the content size + padding then it's the content size + padding.
	virtual float GetActualRenderWidth() = 0;
	virtual float GetActualRenderHeight() = 0;

	// Offset will include position, alignment, and margin adjustments
	virtual float GetActualXOffset() const = 0;
	virtual float GetActualYOffset() const = 0;

	// returns offset for drawing minus position
	virtual float GetRawActualXOffset() const = 0;
	virtual float GetRawActualYOffset() const = 0;

	// Offset to apply to contents for scrolling
	virtual float GetContentsYScrollOffset() const = 0;
	virtual float GetContentsXScrollOffset() const = 0;
	virtual float GetContentsYScrollOffsetTarget() const = 0;
	virtual float GetContentsXScrollOffsetTarget() const = 0;
	virtual double GetContentsXScrollTransitionStart() const = 0;
	virtual double GetContentsYScrollTransitionStart() const = 0;
	virtual float GetInterpolatedXScrollOffset() = 0;
	virtual float GetInterpolatedYScrollOffset() = 0;

	// Can the panel scroll further?
	virtual bool BCanScrollUp() = 0;
	virtual bool BCanScrollDown() = 0;
	virtual bool BCanScrollLeft() = 0;
	virtual bool BCanScrollRight() = 0;

	// style class management
	virtual void AddClass( const char *pchName ) = 0;
	virtual void AddClass( CPanoramaSymbol symbol ) = 0;
	virtual void AddClasses( const char *pchName ) = 0;
	virtual void AddClasses( CPanoramaSymbol *pSymbols, uint cSymbols ) = 0;
	virtual void RemoveClass( const char *pchName ) = 0;
	virtual void RemoveClass( CPanoramaSymbol symName ) = 0;
	virtual void RemoveClasses( const CPanoramaSymbol *pSymbols, uint cSymbols ) = 0;
	virtual void RemoveClasses( const char *pchName ) = 0;
	virtual void RemoveAllClasses() = 0;
	virtual const CUtlVector< CPanoramaSymbol > &GetClasses() const = 0;
	virtual bool BHasClass( const char *pchName ) = 0;
	virtual bool BHasClass( CPanoramaSymbol symName ) = 0;
	virtual bool BAscendantHasClass( const char *pchName ) = 0;
	virtual bool BAscendantHasClass( CPanoramaSymbol symName ) = 0;
	virtual void ToggleClass( const char *pchName ) = 0;
	virtual void ToggleClass( CPanoramaSymbol symName ) = 0;
	virtual void SetHasClass( const char *pchName, bool bHasClass ) = 0;
	virtual void SetHasClass( CPanoramaSymbol symName, bool bHasClass ) = 0;
	virtual void SwitchClass( const char *pchAttribute, const char *pchName ) = 0;
	virtual void SwitchClass( const char *pchAttribute, CPanoramaSymbol symName ) = 0;

	virtual bool BAcceptsInput() = 0;
	virtual void SetAcceptsInput( bool bAllowInput ) = 0;
	virtual bool BAcceptsFocus() const = 0;
	virtual void SetAcceptsFocus( bool bAllowFocus ) = 0;
	// true if it would normally sink input, but may not right now if disabled
	virtual bool BCanAcceptInput() = 0;
	virtual void SetDefaultFocus( const char *pchChildID ) = 0;
	virtual const char *GetDefaultFocus() const = 0;
	virtual void SetDisableFocusOnMouseDown( bool bDisable ) = 0;
	virtual bool BFocusOnMouseDown() = 0;

	// Should this panel be the top of an input hierarchy and keep track of focus within itself, not losing focus when a panel in some
	// other hierarchy changes focus?  Use this for panels that are peers like friends vs browser vs mainmenu in tenfoot
	virtual bool BTopOfInputContext() = 0;
	virtual void SetTopOfInputContext( bool bIsTopOfInputContext ) = 0;

	virtual IUIPanel *GetParentInputContext() = 0;

	// Get the default input focus child within this panel, may be null
	virtual IUIPanel *GetDefaultInputFocus() = 0;

	// Set focus to this panel, which will auto-scroll it into full view as well if parent has overflow: scroll
	virtual bool SetFocus() = 0;

	// Set the focus to this panel in it's input context, but do not make the context change if some other context currently
	// has focus
	virtual bool UpdateFocusInContext() = 0;

	// Set the focus in response to receiving hover (on panels that a parent sets childfocusonhover), this will
	// never scroll the parent.
	virtual bool SetFocusDueToHover() = 0;

	virtual void SetInputContextFocus() = 0;

	// retrieve the style flags (map to CSS psuedo-classes) for this panel
	virtual uint GetStyleFlags() const = 0;
	virtual void AddStyleFlag( EStyleFlags eStyleFlag ) = 0;
	virtual void RemoveStyleFlag( EStyleFlags eStyleFlag ) = 0;
	virtual bool IsInspected() const = 0;
	virtual bool BHasHoverStyle() const = 0;
	virtual void SetSelected( bool bSelected ) = 0;
	virtual bool IsSelected() const = 0;
	virtual bool BHasKeyFocus() const = 0;
	virtual bool BHasDescendantKeyFocus() const = 0;
	virtual bool IsLayoutLoading() const = 0;

	// enable/disable
	virtual void SetEnabled( bool bEnabled ) = 0;
	virtual bool IsEnabled() const = 0;

	virtual bool IsActivationEnabled() = 0;

	// Set activation disabled on this panel, input/focus still generally work, but Activate events won't be handled, useful to prevent a button
	// being clicked when out of focus, but leave it able to be focused for later activation or such
	virtual void SetActivationEnabled( bool bEnabled ) = 0;

	// Set all our immediate children enabled/disabled
	virtual void SetAllChildrenActivationEnabled( bool bEnabled ) = 0;

	// Enable/disable hit testing of this panel, you may want a parent that is never hit test that has a large region, but clicks
	// just pass through to other things behind it.  Children may still hit test.
	virtual void SetHitTestEnabled( bool bEnabled ) = 0;
	virtual bool BHitTestEnabled() const = 0;
	virtual void SetHitTestEnabledTraverse( bool bEnabled ) = 0;

	// drag/drop
	virtual void SetDraggable( bool bEnabled ) = 0;
	virtual bool IsDraggable() const = 0;

	// the input namespace to use for this panel
	virtual const char *GetInputNamespace() const = 0;

	virtual void SetInputNamespace( const char *pchNamespace ) = 0;

	// Mark styles dirty for the panel
	virtual void MarkStylesDirty( bool bIncludeChildren ) = 0;

	// Check if styles are dirty for the panel
	virtual bool BStylesDirty() const = 0;

	// Check if styles are possibly dirty for any of our children
	virtual bool BChildStylesDirty() = 0;

	// Parse panel event for this panel
	virtual bool BParsePanelEvent( CPanoramaSymbol symPanelEvent, const char *pchValue ) = 0;

	// Check if panel event is set on panel for event type
	virtual bool BIsPanelEventSet( CPanoramaSymbol symPanelEvent ) = 0;

	// Check if the event is a valid panel event type
	virtual bool BIsPanelEvent( CPanoramaSymbol symPanelEvent ) const = 0;

	// Dispatch the panel event if the panel has something set for it now
	virtual bool DispatchPanelEvent( CPanoramaSymbol symPanelEvent ) = 0;

	// Get the containing panel for this panels javascript context
	virtual panorama::IUIPanel *GetJavaScriptContextParent() const = 0;

	// Accessor for appropriate image manager for panel
	virtual panorama::IUIImageManager* UIImageManager() = 0;

	// Accessor for appropriate 3d surface interface for this panel
	virtual panorama::IUIRenderEngine *UIRenderEngine() = 0;

	// Explicit call to paint the panel and all it's children, normally called internally by window
	virtual void PaintTraverse() = 0;

	// Tab index setting
	virtual void SetTabIndex( float flTabIndex ) = 0;
	virtual float GetTabIndex() const = 0;
	virtual void SetSelectionPosition( float flXPos, float flYPos ) = 0;
	virtual void SetSelectionPositionX( float flXPos ) = 0;
	virtual void SetSelectionPositionY( float flYPos ) = 0;
	virtual float GetSelectionPositionX() const = 0;
	virtual float GetSelectionPositionY() const = 0;

	// Tab index and selection position backing values rather than interpreted (ie. will return k_flSelectionPosAuto rather than the auto-calculated value)
	virtual float GetTabIndex_Raw() const = 0;
	virtual float GetSelectionPositionX_Raw() const = 0;
	virtual float GetSelectionPositionY_Raw() const = 0;

	// Tell the panel to set focus to the next panel in specified movement/order method
	virtual bool SetFocusToNextPanel( int nRepeats, EFocusMoveDirection moveType, bool bAllowWrap, float flTabIndexCurrent, float flXPosCurrent, float flYPosCurrent, float flXStart, float flYStart ) = 0;
	virtual bool SetInputFocusToFirstOrLastChildInFocusOrder( EFocusMoveDirection moveType, float flXStart, float flYStart ) = 0;

	// Is this panel a selection pos repeat boundary?
	virtual bool BSelectionPosVerticalBoundary() = 0;
	virtual bool BSelectionPosHorizontalBoundary() = 0;

	// controls if clicking on an unfocused panel should set focus
	virtual void SetChildFocusOnHover( bool bEnable ) = 0;
	virtual bool GetChildFocusOnHover() = 0;

	// Panel scrolling
	virtual void ScrollToTop() = 0;
	virtual void ScrollToBottom() = 0;
	virtual void ScrollToLeftEdge() = 0;
	virtual void ScrollToRightEdge() = 0;
	virtual void ScrollParentToMakePanelFit( ScrollBehavior_t behavior = SCROLL_BEHAVIOR_DEFAULT, bool bImmediateScroll = false ) = 0;
	virtual void ScrollToFitRegion( float x0, float x1, float y0, float y1, ScrollBehavior_t behavior = SCROLL_BEHAVIOR_DEFAULT, bool bDirectParentScrollOnly = false, bool bImmediateScroll = false ) = 0;
	virtual bool BCanSeeInParentScroll() = 0;

	virtual void OnScrollPositionChanged() = 0;
	virtual void SetSendChildScrolledIntoViewEvents( bool bSendChildReadyEvents ) = 0;	// this must be enabled on your parent for ScrolledIntoView events to fire and IsScrolledIntoView state to be set
	virtual bool OnCheckChildrenScrolledIntoView() = 0;
	virtual void FireScrolledIntoViewEvent() = 0;
	virtual void FireScrolledOutOfViewEvent() = 0;
	virtual bool IsScrolledIntoView() const = 0;

	// Direct child management
	virtual void SortChildren( int( __cdecl *pfnCompare )(const ClientPanelPtr_t *, const ClientPanelPtr_t *) ) = 0;

	// child management, use with caution! normally always managed internally.
	virtual void AddChild( IUIPanel *pChild ) = 0;

	// child management, use with caution! normally always managed internally.  Returns child index we inserted at.
	virtual int AddChildSorted( bool( __cdecl *pfnLessFunc )(ClientPanelPtr_t const &p1, ClientPanelPtr_t const &p2), IUIPanel *pChild ) = 0;

	// child management, use with caution! normally always managed internally.
	virtual void RemoveChild( IUIPanel *pChild ) = 0;

	// Move child after another child
	virtual void MoveChildAfter( IUIPanel *pChildToMove, IUIPanel *pBefore ) = 0;

	// Move child before another child
	virtual void MoveChildBefore( IUIPanel *pChildToMove, IUIPanel *pAfter ) = 0;

	virtual void SetMouseCanActivate( EMouseCanActivate eMouseCanActivate, const char *pchOptionalParent = NULL ) = 0;
	virtual EMouseCanActivate GetMouseCanActivate() = 0;
	virtual IUIPanel *FindParentForMouseCanActivate() = 0;

	virtual bool BReloadLayout( CPanoramaSymbol symPath ) = 0;

	virtual void ReloadStyleFileTraverse( CPanoramaSymbol symPath ) = 0;

	virtual bool BHasOnActivateEvent() = 0;
	virtual bool BHasOnMouseActivateEvent() = 0;

	// Called to ask us to setup object template for Javascript, you can implement this in a child class and then call
	// the base method (so all the normal panel2d stuff gets exposed), plus call the various RegisterJS helpers yourself
	// to expose additional panel type specific data/methods.
	virtual void SetupJavascriptObjectTemplate() = 0;

	virtual void SetLayoutFile( CPanoramaSymbol symLayoutFile ) = 0;

	// Low level build matching style list for debugger use
	virtual bool BBuildMatchingStyleList( CUtlVector< CascadeStyleFileInfo_t > *pvecStyles ) = 0;

	// Getter for panel attributes
	virtual int GetAttribute( const char *pchAttrName, int nDefaultValue ) = 0;

	// Getter for panel attributes
	virtual const char *GetAttribute( const char *pchAttrName, const char * pchDefaultValue ) = 0;

	// Getter for panel attributes
	virtual uint32 GetAttribute( const char *pchAttrName, uint32 unDefaultValue ) = 0;

	// Getter for panel attributes
	virtual uint64 GetAttribute( const char *pchAttrName, uint64 unDefaultValue ) = 0;

	// Setter for panel attributes
	virtual void SetAttribute( const char *pchAttrName, int nValue ) = 0;

	// Setter for panel attributes
	virtual void SetAttribute( const char *pchAttrName, const char * pchValue ) = 0;

	// Setter for panel attributes
	virtual void SetAttribute( const char *pchAttrName, uint32 unValue ) = 0;

	// Setter for panel attributes
	virtual void SetAttribute( const char *pchAttrName, uint64 unValue ) = 0;

	// Set animation property on panel
	virtual void SetAnimation( const char *pchAnimationName, float flDuration, float flDelay, EAnimationTimingFunction eTimingFunc, EAnimationDirection eDirection, float flIterations ) = 0;

	// Force an immediate update of the visibility list on our window for our current visibility
	virtual void UpdateVisibility( bool bUseDirtyStyles ) = 0;

	// Base class implementation for valid XML properties to be set
	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) = 0;

	// Builds a string of properties and values to display in the debugger
	virtual void GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties ) = 0;

	// Populates a vector with all immediate children matching a class
	virtual void FindChildrenWithClass( const char *pchClass, CUtlVector<IUIPanel *> &vecMatchingChildren ) = 0;

	// Populates a vector with all children matching a class
	virtual void FindChildrenWithClassTraverse( const char *pchClass, CUtlVector<IUIPanel *> &vecMatchingChildren ) = 0;

	// Play focus change sound accounting for fast scroll volume fade effects, etc
	virtual void PlayFocusChangeSound( int nRepeats, float flPan ) = 0;

	// Clear all panel events
	virtual void ClearPanelEvents() = 0;

	// Clear specific panel event
	virtual void ClearPanelEvents( CPanoramaSymbol symPanelEvent ) = 0;

	// panel events
	virtual void SetPanelEvent( CPanoramaSymbol symPanelEvent, IUIEvent *pEvent ) = 0;
	virtual void SetPanelEvent( CPanoramaSymbol symPanelEvent, VecUIEvents_t *pEvents ) = 0;

	// Should analog stick be able to scroll this panel?
	virtual bool BEnableAnalogStickScrolling() = 0;

	// Set mouse tracking state
	virtual void SetMouseTracking( bool bState ) = 0;

	// Should only be called in the very limited cases where we are creating scrollbars within a layout pass,
	// exposed for HTML but should really only be used inside UIPanel normally
	virtual void SetInScrollbarConstruction( bool bConstructing ) = 0;

	// Get the scrollbar for this panel if it exists
	virtual IUIScrollBar *GetVerticalScrollBar() = 0;
	virtual IUIScrollBar *GetHorizontalScrollBar() = 0;

	// Get panel events set on panel
	virtual VecUIEvents_t * GetPanelEvents( CPanoramaSymbol symEvent ) = 0;

	// Has this panel ever been layed out
	virtual bool BHasBeenLayedOut() const = 0;

	// Callback that styles have cleaned up some transitions, we should update cached state about what styles are present
	virtual void OnStyleTransitionsCleanup() = 0;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName ) = 0;
#endif
};

}
#endif // IUIPANEL_H
