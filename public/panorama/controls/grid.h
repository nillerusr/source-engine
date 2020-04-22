//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef GRID_H
#define GRID_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"
#include "panorama/controls/label.h"
#include "panorama/controls/mousescroll.h"

namespace panorama
{

DECLARE_PANEL_EVENT0( ReadyPanelForDisplay )
DECLARE_PANEL_EVENT0( PanelDoneWithDisplay )
DECLARE_PANEL_EVENT0( GridMotionTimeout );
DECLARE_PANEL_EVENT0( GridInFastMotion );
DECLARE_PANEL_EVENT0( GridStoppingFastMotion );
DECLARE_PANEL_EVENT0( GridPageLeft );
DECLARE_PANEL_EVENT0( GridPageRight );
DECLARE_PANEL_EVENT0( GridDirectionalMove );
DECLARE_PANEL_EVENT1( ChildIndexSelected, int );

//-----------------------------------------------------------------------------
// Purpose: Button
//-----------------------------------------------------------------------------
class CGrid : public CPanel2D
{
	DECLARE_PANEL2D( CGrid, CPanel2D );

public:
	CGrid( CPanel2D *parent, const char * pchPanelID );
	virtual ~CGrid();

	CPanel2D * AccessSelectedPanel() { return m_pFocusedChild.Get(); }

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// Scroll the grid so the focused panel is in the top left corner
	void MoveFocusToTopLeft();

	// Scroll the grid all the way to the left regardless of what's
	// focused.
	void ScrollPanelToLeftEdge();

	// Trigger fast motion style temporarily, do this if you are directly setting focus ahead a bunch
	void TriggerFastMotion();
	void BumpFastMotionTimeout();

	void SetHorizontalCount( int nCount ) { SetHorizontalAndVerticalCount( nCount, m_nVerticalCount ); }
	void SetVerticalCount( int nCount ) { SetHorizontalAndVerticalCount( m_nHorizontalCount, nCount ); }
	int GetHorizontalCount() const { return m_nHorizontalCount; }
	int GetVerticalCount() const { return m_nVerticalCount; }

	void SetHorizontalFocusLimit( int nCount ) { m_nHorizontalFocusLimit = nCount; InvalidateSizeAndPosition(); }
	int GetHorizontalFocusLimit() const { return m_nHorizontalFocusLimit; }

	float GetScrollProgress() const { return m_flScrollProgress; }

	virtual bool OnMoveUp( int nRepeats );
	virtual bool OnMoveDown( int nRepeats );
	virtual bool OnMoveRight( int nRepeats );
	virtual bool OnMoveLeft( int nRepeats );
	virtual bool OnTabForward( int nRepeats );
	virtual bool OnTabBackward( int nRepeats );
	virtual bool OnMouseWheel( const panorama::MouseData_t &code );
	virtual bool OnGamePadDown( const panorama::GamePadData_t &code );
	virtual bool OnKeyDown( const KeyData_t &code );

	virtual bool BRequiresContentClipLayer() OVERRIDE { return true; }

	virtual void Paint();
	virtual bool OnSetFocusToNextPanel( int nRepeats, EFocusMoveDirection moveType, bool bAllowWrap, float flTabIndexCurrent, float flXPosCurrent, float flYPosCurrent, float flXStart, float fYStart ) OVERRIDE
	{
		switch( moveType )
		{
		case k_ENextInTabOrder:
			if ( OnTabForward( nRepeats ) )
				return true;
			break;
		case k_ENextByXPosition:
			if ( OnMoveRight( nRepeats ) )
				return true;
			break;
		case k_EPrevInTabOrder:
			if ( OnTabBackward( nRepeats ) )
				return true;
			break;
		case k_EPrevByXPosition:
			if ( OnMoveLeft( nRepeats ) )
				return true;
			break;
		case k_ENextByYPosition:
			if ( OnMoveDown( nRepeats ) )
				return true;
			break;
		case k_EPrevByYPosition:
			if ( OnMoveUp( nRepeats ) )
				return true;
			break;
		default:
			break;
		}

		return false;
	}

	void SetHorizontalAndVerticalCount( int nHorizontalCount, int nVerticalCount );

	void SetIgnoreFastMotion( bool bValue ) { m_bIgnoreFastMotion = bValue; }

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

protected:
	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );
	virtual void OnBeforeChildrenChanged() { m_bForceRelayout = true;  }

	virtual void OnChildStylesChanged() OVERRIDE { m_bVecVisibleDirty = true; }
	virtual void OnAfterChildrenChanged() OVERRIDE { m_bVecVisibleDirty = true; }
private:

	void UpdateVecVisible();
	int GetVisibleChildCount();
	CPanel2D *GetVisibleChild( int iVisibleIndex );

	// event handlers
	bool EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel );
	bool EventInputFocusLost( const CPanelPtr< IUIPanel > &ptrPanel );
	bool MotionTimeout( const CPanelPtr< IUIPanel > &ptrPanel );
	bool OnMouseScroll( const CPanelPtr< IUIPanel > &ptrPanel, int cRepeat );
	void LayoutMouseScrollRegions( float flFinalWidth, float flFinalHeight );
	bool EventWindowCursorShown( IUIWindow *pWindow );
	bool EventWindowCursorHidden( IUIWindow *pWindow );

	void RegisterForCursorChanges();
	void UnregisterForCursorChanges();

	int GetFocusedChildVisibleIndex();
	void UpdateChildPositions( bool bForceTopLeft = false );

	bool m_bHadFocus;

	CPanelPtr< CPanel2D > m_pFocusedChild;
	CUtlVector< CPanelPtr<CPanel2D> > m_vecPanelsReadyForDisplay;

	int m_nScrollOffset;

	float m_flChildWidth;
	float m_flChildHeight;
	float m_flScaleOffset;

	float m_flScrollProgress;

	int m_nHorizontalCount;
	int m_nVerticalCount;
	
	// Override how far right you can move before all items must shift, should be smaller than m_nHorizontalCount
	int m_nHorizontalFocusLimit;

	double m_flLastMouseWheel;
	bool m_bForceRelayout;

	bool m_bIgnoreFastMotion;
	double m_flStartedMotion;
	double m_flLastMotion;
	uint64 m_ulMotionSinceStart;
	bool m_bFastMotionStarted;
	bool m_bVecVisibleDirty;

	CUtlVector< CPanel2D * > m_vecVisibleChildren;
	
	panorama::CMouseScrollRegion *m_pLeftMouseScrollRegion;
	panorama::CMouseScrollRegion *m_pRightMouseScrollRegion;

};


} // namespace panorama

#endif // GRID_H
