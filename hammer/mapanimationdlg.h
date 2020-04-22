//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPANIMATIONDLG_H
#define MAPANIMATIONDLG_H
#ifdef _WIN32
#pragma once
#endif


#include "HammerBar.h"
#include "MapClass.h"


class CMapAnimationDlg : public CHammerBar
{
public:
    CMapAnimationDlg();
    
    bool Create( CWnd *pParentWnd );

	void RunFrame( void );
	void SelectionChanged( CMapObjectList &NewSelList);

protected:

	//{{AFX_DATA(CMapAnimationDlg)
	enum { IDD = IDD_ANIMATIONDLG };
    CSliderCtrl		m_TimeSlider;        // time in animation
    CButton         m_Play;              // plays the current animation
	//}}AFX_DATA

	//{{AFX_MSG( CMapAnimationDlg )
    afx_msg void OnHScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar );
	afx_msg void OnPlay();
	afx_msg void OnCreateKeyFrame();
	afx_msg void UpdateControl( CCmdUI *pCmdUI );
	//}}AFX_MSG

	void AdvanceAnimationTime( void );

    void InitTimeSlider( void );
	void UpdateAnimationTime( void );
	void ResetTimeSlider( void );
	void PausePlayback( void );


	bool m_bPlaying;
	bool m_bEnabled;
	float m_flAnimationDuration;
	float m_flAnimationStart;
	float m_flAnimTime;

	DECLARE_MESSAGE_MAP()
};

#endif // MAPANIMATIONDLG_H
