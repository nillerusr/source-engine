//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CAROUSEL_H
#define CAROUSEL_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/mathlib.h"
#include "mathlib/beziercurve.h"
#include "panel2d.h"
#include "panorama/controls/label.h"
#include "panorama/controls/mousescroll.h"

namespace panorama
{

DECLARE_PANORAMA_EVENT0( ResetCarouselMouseWheelCounts );
DECLARE_PANORAMA_EVENT1( SetCarouselSelectedChild, CPanelPtr<CPanel2D> );


//-----------------------------------------------------------------------------
// Purpose: Button
//-----------------------------------------------------------------------------
class CCarousel : public CPanel2D
{
	DECLARE_PANEL2D( CCarousel, CPanel2D );

public:
	CCarousel( CPanel2D *parent, const char * pchPanelID );
	virtual ~CCarousel();

	enum EFocusType
	{
		k_EFocusTypeLeft,
		k_EFocusTypeEdge,
		k_EFocusTypeCenter
	};

	void SetTitleText( const char *pchTitle );
	void SetTitleVisible( bool bVisible );
	void SetWrap( bool bWrap );
	void SetFocusType( EFocusType eType );
	void SetOffset( CUILength len );
	void DrawFocusFrame( bool bDraw );
	void DeleteChildren();
	
	bool SetFocusToIndex( int iFocus );
	int GetFocusIndex() const { return GetChildIndex( m_pFocusedChild.Get() ); }
	CPanel2D *GetFocusChild() const { return m_pFocusedChild.Get(); }

	// Sets the child that will get focus when the carousel has focus. Remembered between focus calls
	void SetSelectedChild( CPanel2D *pPanel );

	// Sets the panel for which focus state is checked when applying focus offset.
	void SetFocusOffsetPanel( CPanel2D *pPanel ) { m_ptrPanelFocusOffset = pPanel; }

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties );
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );
	virtual void Paint();

	virtual bool OnMoveRight( int nRepeats );
	virtual bool OnMoveLeft( int nRepeats );
	virtual bool OnTabForward( int nRepeats ) { return OnMoveRight( nRepeats ); }
	virtual bool OnTabBackward( int nRepeats ) { return OnMoveLeft( nRepeats ); }
	virtual bool OnMouseWheel( const panorama::MouseData_t &code );
	virtual void OnStylesChanged();

	virtual void OnUIScaleFactorChanged( float flScaleFactor ) OVERRIDE;

	virtual bool BRequiresContentClipLayer() OVERRIDE { return true; }

	virtual void OnInitializedFromLayout();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName );
#endif

protected:

	virtual bool OnSetFocusToNextPanel( int nRepeats, EFocusMoveDirection moveType, bool bAllowWrap, float flTabIndexCurrent, float flXPosCurrent, float flYPosCurrent, float flXStart, float fYStart ) OVERRIDE
	{
		if ( m_bWrap )
		{
			switch( moveType )
			{
			case k_ENextInTabOrder:
			case k_ENextByXPosition:
				return OnMoveRight( nRepeats );
			case k_EPrevInTabOrder:
			case k_EPrevByXPosition:
				return OnMoveLeft( nRepeats );
			default:
				break;
			}
		}
		else
		{
			int iFocusChild = GetChildIndex( m_pFocusedChild.Get() );
			switch( moveType )
			{
			case k_ENextInTabOrder:
			case k_ENextByXPosition:
				if ( iFocusChild < GetChildCount() - 1 )
				{
					return OnMoveRight( nRepeats );
				}
				break;
			case k_EPrevInTabOrder:
			case k_EPrevByXPosition:
				if ( iFocusChild > 0 )
				{
					return OnMoveLeft( nRepeats );
				}
				break;
			default:
				break;
			}
		}

		return false;
	}

	// child management
	virtual void OnBeforeChildrenChanged();
	virtual void OnCallBeforeStyleAndLayout() { UpdateFocusAndDirtyChildStyles(); }

private:
	enum EFocusEdge
	{
		k_EFocusEdgeLeft,
		k_EFocusEdgeRight
	};

	// event handlers
	bool EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel );
	bool EventInputFocusLost( const CPanelPtr< IUIPanel > &ptrPanel );
	bool OnResetMouseWheelCounts();
	bool EventCarouselMouseScroll( const CPanelPtr< IUIPanel > &ptrPanel, int cRepeat );
	bool EventWindowCursorShown( IUIWindow *pWindow );
	bool EventWindowCursorHidden( IUIWindow *pWindow );

	// owned panels
	CLabel *CreateTitleLabel();

	// focus
	bool BSetFocusToChild( CPanel2D *pPanel );
	void MarkFocusDirty();
	bool UpdateFocusAndDirtyChildStyles();

	// helpers
	int GetPreviousWrapPanel( int i );
	int GetNextWrapPanel( int i );
	float GetFinalChildWidth( CPanel2D *pChild, float flContainerHeight );
	void GetFinalChildDimensions( float *pflWidth, float *pflHeight, CPanel2D *pChild, float flContainerHeight );		
	int CalcIndexDistanceBetweenPanels( int iLHS, int iRHS );
	int GetNextPanelInLayout( int iStart );
	int GetPreviousPanelInLayout( int iStart );
	void AddCarouselStyle( CPanel2D *pChild, int iChild, int iCurrentFocus );
	void RemoveCarouselStyle( CPanel2D *pChild, int iChild, int iCurrentFocus );
	void RegisterForCursorChanges();
	void UnregisterForCursorChanges();

	// configured offets
	void GetPanelOffsets( CUILength *plenX, CUILength *plenY, CUILength *plenZ, int nDistanceFromFocus, float flWidth, float flHeight );
	CUILength GetPanelOffset( int nDistanceFromFocus, bool bUseFocus, const CUtlVector< CUILength > &vecOffsets, const CUtlVector< CUILength > &vecFocusOffsets );

	// layout related
	void GetLayoutStart( int iFocusChild, float *pflOffset, float flLeft, float flCarouselOffset, const float flContainerWidth, const float flContainerHeight );
	void LayoutChildPanels( int iFocusChild, float flOffset, float flLeft, float flRight, const float flContainerWidth, const float flContainerHeight, const CUtlVector< CPanel2D* > &vecNewChildren );
	bool BPositionPanelRight( int iPanel, int nDistanceFromFocus, float *pflOffset, float flLeft, float flContainerWidth, float flContainerHeight, bool bCheckFits, const CUtlVector< CPanel2D* > &vecNewChildren );
	bool BPositionPanelLeft( int iPanel, int nDistanceFromFocus, float *pflOffset, float flLeft, float flContainerWidth, float flContainerHeight, bool bCheckFits, const CUtlVector< CPanel2D* > &vecNewChildren );
	void LayoutMouseScrollRegions( float flFinalWidth, float flFinalHeight );
	
	struct DirtyChildStyles_t
	{
		int m_iOriginalFocus;
		CUtlVector< CPanel2D* > m_vecPanels;
	};
	DirtyChildStyles_t *m_pDirtyChildStyles;


	CLabel *m_pTitleLabel;
	CMouseScrollRegion *m_pLeftMouseScrollRegion;
	CMouseScrollRegion *m_pRightMouseScrollRegion;
	CPanelPtr< CPanel2D > m_pFocusedChild;

	EFocusType m_eFocusType;
	bool m_bWrap;
	CUILength m_lenOffset;
	bool m_bIncludeScale2d;

	// for edge focus
	EFocusEdge m_eLastFocusEdge;
	int m_iFocusLastEdge;

	struct ChildOffsets_t
	{
		CUtlVector< CUILength > x;
		CUtlVector< CUILength > y;
		CUtlVector< CUILength > z;
	};
	ChildOffsets_t m_childOffsets;
	ChildOffsets_t m_childOffsetsFocus;	
	bool m_bFlowingLayout;
	bool m_bHadFocus;

	double m_flLastMouseWheel;
	uint32 m_unMouseWheelCount;

	double m_flLastMove;
	bool m_bDelayedMovePosted;
	bool m_bRegisteredForCursorChanges;
	
	bool m_bShuffleIntoView;

	int32 m_nPanelsVisible;

	CPanelPtr< CPanel2D > m_ptrPanelFocusOffset;
};


} // namespace panorama

#endif // CAROUSEL_H
