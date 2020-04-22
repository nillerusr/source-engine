//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_SLIDESHOW_H
#define PANORAMA_SLIDESHOW_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/panel2d.h"
#include "panorama/controls/mousescroll.h"

namespace panorama
{

DECLARE_PANEL_EVENT1( SlideShowPanelChanged, int );
DECLARE_PANEL_EVENT0( SlideShowOnLayoutInitialized );

//-----------------------------------------------------------------------------
// Purpose: Panel that shows a slideshow of panels
//-----------------------------------------------------------------------------
class CSlideShow : public CPanel2D
{
	DECLARE_PANEL2D( CSlideShow, CPanel2D );

public:
	CSlideShow( CPanel2D *pParent, const char *pchID );
	virtual ~CSlideShow();

	void AddPanel( CPanel2D *pPanel, bool bDontSetFocusBySideEffect );
	void RemoveAndDeletePanel( CPanel2D *pPanel );
	void SetManageFocus( bool bManageFocus ) { m_bManageFocus = bManageFocus; }

	void SetFocusIndex( int iFocus, bool bSkipChildCountCheck = false );
	int GetFocusIndex() { return m_iFocusChild; }
	CPanel2D *GetFocusChild() { return GetChild(m_iFocusChild); }
	bool BFocusChildRightMost() { return (m_iFocusChild == (GetChildCount() - 1)); }

	virtual bool OnMoveRight( int nRepeats );
	virtual bool OnMoveLeft( int nRepeats );
	virtual bool OnTabForward( int nRepeats );
	virtual bool OnTabBackward( int nRepeats );

	virtual void Paint();
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );

	virtual bool OnSetFocusToNextPanel( int nRepeats, EFocusMoveDirection moveType, bool bAllowWrap, float flTabIndexCurrent, float flXPosCurrent, float flYPosCurrent, float flXStart, float fYStart ) OVERRIDE
	{
		switch( moveType )
		{
		case k_ENextInTabOrder:
			if ( m_bManageFocus && OnTabForward( nRepeats ) )
				return true;
			break;
		case k_ENextByXPosition:
			if ( m_bManageFocus && OnMoveRight( nRepeats ) )
				return true;
			break;
		case k_EPrevInTabOrder:
			if ( m_bManageFocus && OnTabBackward( nRepeats ) )
				return true;
			break;
		case k_EPrevByXPosition:
			if ( m_bManageFocus && OnMoveLeft( nRepeats ) )
				return true;
			break;
		case k_ENextByYPosition:
			if ( m_bManageFocus && OnMoveDown( nRepeats ) )
				return true;
			break;
		case k_EPrevByYPosition:
			if ( m_bManageFocus && OnMoveUp( nRepeats ) )
				return true;
			break;
		default:
			break;
		}

		return false;
	}

	virtual panorama::IUIPanel *OnGetDefaultInputFocus() OVERRIDE;


#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

protected:
	bool EventInputFocusSet( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel );
	bool EventCarouselMouseScroll( const CPanelPtr< IUIPanel > &ptrPanel, int cRepeat );
	bool EventSlideShowOnLayoutInitialized( const CPanelPtr< IUIPanel > &ptrPanel );
	virtual void OnInitializedFromLayout();
	void SetPanelStyles( int iOldFocus, int iNewFocus );
	void LayoutMouseScrollRegions( float flFinalWidth, float flFinalHeight );

private:
	virtual void AddDisabledFlagToChildren() OVERRIDE;
	virtual void RemoveDisabledFlagFromChildren() OVERRIDE;
	void SetIndividualPanelStyle( int iChild, int iOldFocus, int iNewFocus );
	void SetMouseScrollVisibility( int iFocus );

	int m_iFocusChild;
	bool m_bManageFocus;
	CMouseScrollRegion *m_pLeftMouseScrollRegion;
	CMouseScrollRegion *m_pRightMouseScrollRegion;
};

} // namespace panorama

#endif // PANORAMA_SLIDESHOW_H
