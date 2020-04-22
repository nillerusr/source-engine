//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdafx.h>
#include "GlobalFunctions.h"
#include "History.h"
#include "MainFrm.h"
#include "MapAnimator.h"
#include "MapAnimationDlg.h"
#include "MapClass.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapWorld.h"
#include "hammer.h"
#include "Selection.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


BEGIN_MESSAGE_MAP( CMapAnimationDlg, CHammerBar )
	//{{AFX_MSG_MAP( CMapAnimationDlg )
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_ANIMATIONPLAY, OnPlay)
	ON_BN_CLICKED(IDC_ANIMATIONCREATEKEYFRAME, OnCreateKeyFrame)
	ON_UPDATE_COMMAND_UI(IDC_ANIMATIONPLAY, UpdateControl)
	ON_UPDATE_COMMAND_UI(IDC_ANIMATIONCREATEKEYFRAME, UpdateControl)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: CMapAnimationDlg contructor
//-----------------------------------------------------------------------------
CMapAnimationDlg::CMapAnimationDlg()
{
	m_flAnimationDuration = 5.0f;
	m_flAnimationStart = 0.0f;
	m_flAnimTime = 0.0f;
	m_bPlaying = false;
}


static const int ANIMSLIDER_NUMTICS = 100;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMapAnimationDlg::Create( CWnd *pParentWnd )
{
    //
    // create a modeless dialog toolbar
    //
    if( !( CHammerBar::Create( pParentWnd, IDD, CBRS_RIGHT, IDCB_ANIMATIONBAR ) ) )
    {
        return false;
    }

    // to remain consistant with the other toolbars in the editor
    SetWindowText( _T( "Animation" ) );

    // set dialog bar style
    SetBarStyle( GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_FIXED );
    
    // enable docking
    EnableDocking( CBRS_ALIGN_ANY );

    //
    // initialize the dialog items
    //
    InitTimeSlider();

	m_Play.SubclassDlgItem( IDC_ANIMATIONPLAY, this );

    // show the dialog
    ShowWindow( SW_SHOW );

	m_bEnabled = false;

    // created successfully
    return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called every frame, used to update animation time
//-----------------------------------------------------------------------------
void CMapAnimationDlg::RunFrame( void )
{
	if ( m_bPlaying )
	{
		AdvanceAnimationTime();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapAnimationDlg::InitTimeSlider( void )
{
    m_TimeSlider.SubclassDlgItem( IDC_TIMESLIDER, this );
    m_TimeSlider.SetRange( 0, ANIMSLIDER_NUMTICS );
    m_TimeSlider.SetTicFreq( ANIMSLIDER_NUMTICS / 4 );
    m_TimeSlider.SetPos( 0 );

	m_TimeSlider.EnableWindow( false );
}

//-----------------------------------------------------------------------------
// Purpose: Sets Enable/Disable state for any controls
// Input  : *pCmdUI - 
//-----------------------------------------------------------------------------
void CMapAnimationDlg::UpdateControl( CCmdUI *pCmdUI )
{
    CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
    if ( !pDoc || !m_bEnabled )
    {
		pCmdUI->Enable( false );
        return;
    }
	else
	{
		pCmdUI->Enable( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Communicates to the doc the current animation time
// Input  : time - 
//-----------------------------------------------------------------------------
void CMapAnimationDlg::UpdateAnimationTime( void )
{
    CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
    if( !pDoc )
    {
        return;
    }

	pDoc->SetAnimationTime( m_flAnimTime );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapAnimationDlg::OnHScroll( UINT nSBCode, UINT nPos, CScrollBar *pScrollBar ) 
{
	// get the new time from the slider bar
	m_flAnimTime = ((float)m_TimeSlider.GetPos() / ANIMSLIDER_NUMTICS) * m_flAnimationDuration;

	// stop any playback
	PausePlayback();

	UpdateAnimationTime();
	CHammerBar::OnHScroll( nSBCode, nPos, pScrollBar );
}

//-----------------------------------------------------------------------------
// Purpose: Moves the animation time forward with real time
//-----------------------------------------------------------------------------
void CMapAnimationDlg::OnPlay( void )
{
    CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
    if ( !pDoc )
    {
        return;
    }

	// if we're not playing, start
	if ( !m_bPlaying )
	{
		m_flAnimationStart = pDoc->GetTime() - m_flAnimTime;
		m_bPlaying = true;

		// change the animation text
		SetDlgItemText( IDC_ANIMATIONPLAY, "Stop" );

		UpdateAnimationTime();
	}
	else
	{
		PausePlayback();
	}
}

//-----------------------------------------------------------------------------
// Purpose: pauses the animation playback at the current time
//-----------------------------------------------------------------------------
void CMapAnimationDlg::PausePlayback( void )
{
	m_bPlaying = false;
	SetDlgItemText( IDC_ANIMATIONPLAY, "Play" );
}


//-----------------------------------------------------------------------------
// Purpose: Creates a new keyframe in the cycle at the current time in the animation
//-----------------------------------------------------------------------------
void CMapAnimationDlg::OnCreateKeyFrame( void )
{
	// stop any playback
	PausePlayback();

	GetHistory()->MarkUndoPosition( NULL, "New Keyframe" );

	// get the animating object
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	const CMapObjectList *pSelection = pDoc->GetSelection()->GetList();
	
	for (int i = 0; i < pSelection->Count(); i++)
	{
		CMapEntity *ent = dynamic_cast<CMapEntity*>( pSelection->Element( i ) );

		if ( ent && ent->IsAnimationController() )
		{
			// tell the animating object to create a new keyframe
			CMapAnimator *anim = ent->GetChildOfType( (CMapAnimator*)NULL );
			if ( anim )
			{
				CMapEntity *pNewEntity = anim->CreateNewKeyFrame( m_flAnimTime );

				CMapDoc::GetActiveMapDoc()->AddObjectToWorld( pNewEntity );
				GetHistory()->KeepNew( pNewEntity );

				// change the selection and then update the view
				CMapDoc::GetActiveMapDoc()->SelectObject(pNewEntity, scClear|scSaveChanges );

				break;
			}
		}
	}

	ResetTimeSlider();
}


//-----------------------------------------------------------------------------
// Purpose: moves the current animation time forward, if currently playing
//-----------------------------------------------------------------------------
void CMapAnimationDlg::AdvanceAnimationTime( void )
{
	if ( !m_bPlaying )
		return;

    CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
    if ( !pDoc )
    {
        return;
    }

	// make sure the animation is long enough to play
	if ( m_flAnimationDuration <= 0.01 )
	{
		ResetTimeSlider();
		return;
	}

	// calculate the new position along the time slider
	m_flAnimTime = pDoc->GetTime() - m_flAnimationStart;

	// check to see if we've hit the end of the animation
	if ( m_flAnimTime >= m_flAnimationDuration )
	{
		ResetTimeSlider();
		return;
	}

	// set the new animtion time
	m_TimeSlider.SetPos( (m_flAnimTime / m_flAnimationDuration) * ANIMSLIDER_NUMTICS );

	UpdateAnimationTime();
}

//-----------------------------------------------------------------------------
// Purpose: Resets the slider bar and all times
//-----------------------------------------------------------------------------
void CMapAnimationDlg::ResetTimeSlider( void )
{
	PausePlayback();
	m_flAnimTime = 0.0f;
	m_flAnimationStart = 0.0f;
	m_TimeSlider.SetPos( 0 );
	UpdateAnimationTime();
}


//-----------------------------------------------------------------------------
// Purpose: Called whenever the selection changes, so the slider bar can update
//			with the selected keyframe info
// Input  : &selection - 
//-----------------------------------------------------------------------------
void CMapAnimationDlg::SelectionChanged( CMapObjectList &selection )
{
	// reset the slider
	ResetTimeSlider();
	m_bEnabled = false;

	// loop through the selection looking for potential animating objects
	CMapEntity *ent = NULL;
	
	FOR_EACH_OBJ( selection, pos )
	{
		ent = dynamic_cast<CMapEntity*>( selection.Element(pos) );

		if ( ent )
		{
			if ( ent->IsAnimationController() && ent->GetChildOfType((CMapAnimator*)NULL) )
			{
				m_bEnabled = true;
				break;
			}
		}
	}

	// find out our enabled state
	if ( !m_bEnabled )
	{
	    m_TimeSlider.EnableWindow( false );
		return;
	}
		
	m_TimeSlider.EnableWindow( true );

	// set up the slider from the selection
	CMapAnimator *anim = ent->GetChildOfType( (CMapAnimator*)NULL );
	Assert( anim != NULL );

	m_flAnimationDuration = anim->GetRemainingTime();
}

