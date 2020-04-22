//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUIPANELCLIENT_H
#define IUIPANELCLIENT_H

#ifdef _WIN32
#pragma once
#endif

#include "panoramatypes.h"
#include "panoramasymbol.h"
#include "input/mousecursors.h"
#include "input/iuiinput.h"

namespace panorama
{

class IUIPanelStyle;
class IUIScrollBar;

//-----------------------------------------------------------------------------
// Purpose: Struct containing a panel property info parsed from a layout file
//-----------------------------------------------------------------------------
struct ParsedPanelProperty_t
{
	CPanoramaSymbol m_symName;
	const char *m_pchValue;
};

//-----------------------------------------------------------------------------
// Purpose: Client side of panel interface, this is what client code implementing controls
// must provide to the core UI engine in terms of a callback interface
//-----------------------------------------------------------------------------
class IUIPanelClient
{
public:

	// Access the panorama side UI panel interface for the client panel
	virtual IUIPanel *UIPanel() const = 0;

	// Callback telling client/control code to delete the client panel object
	virtual void OnDeletePanel() = 0;

	// Get the panel type of a panel
	virtual CPanoramaSymbol GetPanelType() const = 0;

	// Paint the panels contents
	virtual void Paint() = 0;

	// override to change how this panel is measured
	virtual void OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions ) = 0;

	// override to change how the panel lays out it's children (instead of default css positioning/flow-children support)
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) = 0;

	// Direct key/input handling... better to use binds and events and to not directly
	// override these in your panel in nearly all cases.
	virtual bool OnKeyDown( const KeyData_t &code ) = 0;
	virtual bool OnKeyUp( const KeyData_t & code ) = 0;
	virtual bool OnKeyTyped( const KeyData_t &unichar ) = 0;
	virtual bool OnGamePadDown( const GamePadData_t &code ) = 0;
	virtual bool OnGamePadUp( const GamePadData_t &code ) = 0;
	virtual bool OnGamePadAnalog( const GamePadData_t &code ) = 0;
	virtual bool OnMouseButtonDown( const MouseData_t &code ) = 0;
	virtual bool OnMouseButtonUp( const MouseData_t &code ) = 0;
	virtual bool OnMouseButtonDoubleClick( const MouseData_t &code ) = 0;
	virtual bool OnMouseButtonTripleClick( const MouseData_t &code ) = 0;
	virtual bool OnMouseWheel( const MouseData_t &code ) = 0;
	virtual void OnMouseMove( float flMouseX, float flMouseY ) = 0;
	virtual bool OnClick( IUIPanel *pPanel, const MouseData_t &code ) = 0;

	// Override to make a panel allow new panel event symbols in XML
	virtual bool BIsClientPanelEvent( CPanoramaSymbol symProperty ) = 0;

	// Override to handle new properties from xml on your panel
	virtual bool BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties ) = 0;

	// Override to handle new properties from xml on your panel
	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) = 0;

	// Should the panel event be applied only after all children are created as well?
	virtual bool BIsDelayedProperty( CPanoramaSymbol symProperty ) = 0;

	// Callback before children of this panel change
	virtual void OnBeforeChildrenChanged() = 0;

	// Callback for when a child of this panel is removed
	virtual void OnRemoveChild( IUIPanel *pChild ) = 0;

	// Callback after children of this panel change
	virtual void OnAfterChildrenChanged() = 0;

	// Callback on panel getting initialized from layout file
	virtual void OnInitializedFromLayout() = 0;

	// Callback when styles are changing for this panel
	virtual void OnStylesChanged() = 0;

	// Callback when styles are changing for an immediate child of this panel
	virtual void OnChildStylesChanged() = 0;

	// Callback when visibility of the panel is changing
	virtual void OnVisibilityChanged() = 0;

	// Callback which allows panels to override focus movement behavior, this is called before the base
	// framework behavior is executed and returning true will prevent the base behavior from occuring.
	virtual bool OnSetFocusToNextPanel( int nRepeats, panorama::EFocusMoveDirection moveType, bool bAllowWrap, float flTabIndexCurrent, float flXPosCurrent, float flYPosCurrent, float flXStart, float fYStart ) = 0;

	// Allows overriding the parent who will be considered by the localization system for dialog variables,
	// dropdowns do this to allow the dropdown to have dialog vars set that then impact the created popup menu for
	// the actual list... You should almost never need this... try to avoid!
	virtual IUIPanel *GetLocalizationParent() const = 0;

	// Allows panels that know they may draw outside their bounds to tell us to create a clip layer, 
	// we don't always do this since it can be expensive
	virtual bool BRequiresContentClipLayer() = 0;

	// Callback you must first register for with UIEngine()->CallBeforeStyleAndLayout() if you need it
	virtual void OnCallBeforeStyleAndLayout() = 0;

	// bugbug jmccaskey - DELETE ME	
	// Callback when a panel event gets set, DO NOT USE, DELETE THIS AS SOON AS WE CAN, ITS HARMFUL
	virtual void OnPanelEventSet( CPanoramaSymbol symEvent ) = 0;

	// Override to set appropriate mouse cursor when hovering over this panel
	virtual EMouseCursors GetMouseCursor() = 0;

	// Callback when UI scale factor changes, which may affect panel contents sizing/layout
	virtual void OnUIScaleFactorChanged( float flScaleFactor ) = 0;

	// Called to ask us to setup object template for Javascript, you can implement this in a child class and then call
	// the base method (so all the normal panel2d stuff gets exposed), plus call the various RegisterJS helpers yourself
	// to expose additional panel type specific data/methods.
	virtual void SetupJavascriptObjectTemplate() = 0;

	// Callback to client panel to create a scrollbar
	virtual IUIScrollBar *CreateNewVerticalScrollBar( float flInitialScrollPos ) = 0;

	// Callback to client panel to create a scrollbar
	virtual IUIScrollBar *CreateNewHorizontalScrollBar( float flInitialScrollPos ) = 0;

	// Callback to hide tooltip if it's visible
	virtual void HideTooltip() = 0;

	// Override getting default input focus
	virtual IUIPanel *OnGetDefaultInputFocus() = 0;

	virtual void GetPositionWithinAncestor( CPanel2D *pAncestor, float *pflX, float *pflY ) = 0;

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) = 0;
#endif
};

class IUIScrollBar
{
public:

	virtual IUIPanel* UIPanel() = 0;
	virtual IUIPanelClient* ClientPtr() = 0;

	virtual void Normalize( bool bImmediateThumbUpdate = false ) = 0;

	virtual void SetRangeMinMax( float flRangeMin, float flRangeMax ) = 0;

	virtual float GetRangeSize() const = 0;
	virtual float GetRangeMin() const = 0;
	virtual float GetRangeMax() const = 0;

	// Set the window size
	virtual void SetScrollWindowSize( float flWindowSize ) = 0;

	// Get scroll window size
	virtual float GetScrollWindowSize() = 0;

	// Set the current window position
	virtual void SetScrollWindowPosition( float flWindowPos, bool bImmediateMove = false ) = 0;

	virtual float GetLastScrollTime() = 0;

	// Get scroll window position
	virtual float GetScrollWindowPosition() = 0;

	// Return true if the user is manually dragging the scrollbar with the mouse
	virtual bool BLastMoveImmediate() = 0;
};

}
#endif // IUIPANELCLIENT_H
