//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//


#include "hlfaceposer.h"
#include "FlexPanel.h"
#include "ViewerSettings.h"
#include "StudioModel.h"
#include "MatSysWin.h"
#include "ControlPanel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mxtk/mx.h>
#include <mxtk/mxBmp.h>

#include "mxbitmapwindow.h"
#include "mxExpressionTray.h"
#include "expressions.h"
#include "expressiontool.h"
#include "filesystem.h"
#include "mdlviewer.h"
#include "ExpressionProperties.h"
#include "expclass.h"
#include "choreowidgetdrawhelper.h"
#include "choreoview.h"
#include "choreoscene.h"
#include "mxExpressionSlider.h"
#include "faceposer_models.h"

LocalFlexController_t FindFlexControllerIndexByName( StudioModel *model, char const *searchname );
char const *GetGlobalFlexControllerName( int index );

extern char g_appTitle[];

#define LINE_HEIGHT 20

#define FLEXSLIDER_INVALID_INDEX	-1

FlexPanel		*g_pFlexPanel = 0;

void FlexPanel::PositionControls( int width, int height )
{
	int buttonwidth = 80;
	int buttonx = 3;
	int row = height - 18;
	int buttonheight = 18;

	btnResetSliders->setBounds( buttonx, row, buttonwidth, buttonheight );

	buttonx += buttonwidth + 5;
	btnCopyToSliders->setBounds( buttonx, row, buttonwidth, buttonheight );

	buttonx += buttonwidth + 5;
	buttonwidth = 100;
	btnCopyFromSliders->setBounds( buttonx, row, buttonwidth, buttonheight );

	buttonx += buttonwidth + 5;
	buttonwidth = 100;

	btnMenu->setBounds( buttonx, row, buttonwidth, buttonheight );
}

FlexPanel::FlexPanel (mxWindow *parent)
: IFacePoserToolWindow( "FlexPanel", "Flex Sliders" ), mxWindow( parent, 0, 0, 0, 0 )
{
	m_nViewableFlexControllerCount = 0;

	m_bNewExpressionMode = true;

	btnResetSliders = new mxButton( this, 0, 0, 100, 20, "Zero Sliders", IDC_EXPRESSIONRESET );

	btnCopyToSliders = new mxButton( this, 0, 0, 100, 20, "Get Tracks", IDC_COPY_TO_FLEX );
	btnCopyFromSliders = new mxButton( this, 0, 0, 100, 20, "Make Keyframe", IDC_COPY_FROM_FLEX );
	btnMenu = new mxButton( this, 0, 0, 100, 20, "Menu", IDC_FP_MENU );

	mxWindow *wFlex = this;
	for (int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++)
	{
		int w = 5; // (i / 4) * 156 + 5;
		int h = i * LINE_HEIGHT + 5; // (i % 4) * 20 + 5;

		slFlexScale[i] = new mxExpressionSlider (wFlex, w, h, 220, LINE_HEIGHT, IDC_FLEXSCALE + i);
	}

	slScrollbar = new mxScrollbar( wFlex, 0, 0, 18, 100, IDC_FLEXSCROLL, mxScrollbar::Vertical );
	slScrollbar->setRange( 0, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * LINE_HEIGHT );
	slScrollbar->setPagesize( 100 );
}



FlexPanel::~FlexPanel()
{
}

void FlexPanel::redraw()
{
	if ( !ToolCanDraw() )
		return;

	CChoreoWidgetDrawHelper helper( this, GetSysColor( COLOR_BTNFACE ) );
	HandleToolRedraw( helper );

	BaseClass::redraw();
}

void FlexPanel::PositionSliders( int sboffset )
{
	int reservedheight = GetCaptionHeight() + 5 /*gap at top*/ + 1 * 20 /* space for buttons/edit controls*/;

	int widthofslidercolumn = slFlexScale[ 0 ]->w() + 10;

	int colsavailable = ( this->w2() - 20 /*scrollbar*/ - 10 /*left edge gap + right gap*/ ) / widthofslidercolumn;
	// Need at least one column
	colsavailable = max( colsavailable, 1 );

	int rowsneeded = GLOBAL_STUDIO_FLEX_CONTROL_COUNT;

	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( hdr )
	{
		rowsneeded = m_nViewableFlexControllerCount;
	}

	int rowsvisible = ( this->h2() - reservedheight ) / LINE_HEIGHT;

	int rowspercol = rowsvisible;

	if ( rowsvisible * colsavailable < rowsneeded )
	{
		// Figure out how many controls should go in each available column
		rowspercol = (rowsneeded + (colsavailable - 1)) / colsavailable;

		slScrollbar->setPagesize( rowsvisible * LINE_HEIGHT );
		slScrollbar->setRange( 0, rowspercol * LINE_HEIGHT );
	}

	int row = 0;
	int col = 0;
	for (int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++)
	{
		int x = 5 + col * widthofslidercolumn;
		int y = row * LINE_HEIGHT + 5 + GetCaptionHeight() - sboffset; // (i % 4) * 20 + 5;

		slFlexScale[ i ]->setBounds( x, y, slFlexScale[i]->w(), slFlexScale[i]->h() );

		if ( i >= rowsneeded || 
			( y + LINE_HEIGHT - 5 > ( this->h2() - reservedheight ) ) )
		{
			slFlexScale[ i ]->setVisible( false );
		}
		else
		{
			slFlexScale[ i ]->setVisible( true );
		}

		row++;
		if ( row >= rowspercol )
		{
			col++;
			row = 0;
		}
	}
}

int FlexPanel::handleEvent (mxEvent *event)
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	int iret = 0;

	if ( HandleToolEvent( event ) )
	{
		return iret;
	}

	switch ( event->event )
	{
	case mxEvent::Size:
		{
			int trueh = h2() - GetCaptionHeight();
			PositionControls( w2(), h2() );
			slScrollbar->setPagesize( trueh );
			slScrollbar->setBounds( w2() - 18, GetCaptionHeight(), 18, trueh );
			PositionSliders( 0 );
			iret = 1;
		}
		break;
	case mxEvent::Action:
		{
			iret = 1;
			switch ( event->action )
			{
			default:
				iret = 0;
				break;
			case IDC_FLEXSCROLL:
				{
					if ( event->event == mxEvent::Action &&
						event->modifiers == SB_THUMBTRACK)
					{
						int offset = event->height; // ((mxScrollbar *) event->widget)->getValue( );
						
						slScrollbar->setValue( offset ); // if (offset > slScrollbar->getPagesize()
						
						PositionSliders( offset );

						IFacePoserToolWindow::SetActiveTool( this );
					}
				}
				break;
			case IDC_EXPRESSIONRESET:
				{
					ResetSliders( true, true );
					IFacePoserToolWindow::SetActiveTool( this );
				}
				break;

			case IDC_COPY_TO_FLEX:
				{
					g_pExpressionTool->OnCopyToFlex( g_pChoreoView->GetScene()->GetTime(), true );
				}
				break;
			case IDC_COPY_FROM_FLEX:
				{
					g_pExpressionTool->OnCopyFromFlex( g_pChoreoView->GetScene()->GetTime(), false );
				}
				break;
			case IDC_FP_UNCHECK_ALL:
				{
					OnSetAll( FP_STATE_UNCHECK );
				}
				break;
			case IDC_FP_CHECK_ALL:
				{
					OnSetAll( FP_STATE_CHECK );
				}
				break;
			case IDC_FP_INVERT:
				{
					OnSetAll( FP_STATE_INVERT );
				}
				break;
			case IDC_FP_MENU:
				{
					OnMenu();
				}
				break;
			}

			if ( event->action >= IDC_FLEXSCALE && event->action < IDC_FLEXSCALE + GLOBAL_STUDIO_FLEX_CONTROL_COUNT)
			{
				iret = 1;

				bool pushundo = false;
				
				mxExpressionSlider *slider = ( mxExpressionSlider * )event->widget;
				int barnumber = event->height;
				int slidernum = ( event->action - IDC_FLEXSCALE );

				float value = slider->getValue ( barnumber );
				float influ = slider->getInfluence( );
				
				switch( event->modifiers )
				{
				case SB_THUMBPOSITION:
				case SB_THUMBTRACK:
					break;
				case SB_ENDSCROLL:
					pushundo = true;
					break;
				}
				int flex = LookupFlex( slidernum, barnumber );
				int flex2 = LookupPairedFlex( flex );
				float value2 = GetSlider( flex2 );
				
				CExpClass *active = expressions->GetActiveClass();
				if ( active )
				{
					int index = active->GetSelectedExpression();
					if ( pushundo && index != -1 )
					{
						CExpression *exp = active->GetExpression( index );
						if ( exp )
						{
							float *settings = exp->GetSettings();
							float *weights = exp->GetWeights();
							Assert( settings );	
							
							if ( settings[ flex ] != value || 
								 settings[ flex2 ] != value2 || 
								 weights[ flex ] != influ )
							{
								exp->PushUndoInformation();
								
								active->SetDirty( true );
								
								settings[ flex ] = value;
								settings[ flex2 ] = value2;
								weights[ flex ] = influ;
								weights[ flex2 ] = influ;
								
								exp->PushRedoInformation();
								
								g_pExpressionTrayTool->redraw();
							}
						}
					}
				}

				// FIXME: Needs to drive the current actor, not model

				// Go from global to local indices
				LocalFlexController_t localflex = FindFlexControllerIndexByName( models->GetActiveStudioModel(), GetGlobalFlexControllerName( flex ) );
				if ( localflex >= 0 )
				{
					// Update the face
					// FIXME: I'm not sure this is needed anymore....
					models->GetActiveStudioModel()->SetFlexController( localflex, value * influ );
					if (flex2 != flex)
					{
						LocalFlexController_t localflex2 = FindFlexControllerIndexByName( models->GetActiveStudioModel(), GetGlobalFlexControllerName( flex2 ) );
						if ( localflex2 >= 0 )
						{
							models->GetActiveStudioModel()->SetFlexController( localflex2, value2 * influ );
						}
						else
						{
							Assert( 0 );
						}
					}
				}
				else
				{
					Assert( 0 );
				}

				models->SetSolveHeadTurn( 1 );
				IFacePoserToolWindow::SetActiveTool( this );
			}
		}
	}
	
	return iret;
}

void FlexPanel::initFlexes()
{
	m_nViewableFlexControllerCount = 0;

	memset( nFlexSliderIndex, 0, sizeof( nFlexSliderIndex ) );
	memset( nFlexSliderBarnum, 0, sizeof( nFlexSliderBarnum ) );

	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if (hdr)
	{
		for (int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++)
		{
			slFlexScale[i]->setVisible( false );
			slFlexScale[i]->setLabel( "" );
			slFlexScale[i]->SetMode( false );
			// init to invalid slider index
			nFlexSliderIndex[i] = FLEXSLIDER_INVALID_INDEX;
		}

		// J is the slider number we're filling in
		int j = 0;
		for ( LocalFlexController_t k = LocalFlexController_t(0); k < hdr->numflexcontrollers(); k++ )
		{
			// Lookup global flex controller index
			int controller = hdr->pFlexcontroller( k )->localToGlobal;
			Assert( controller != -1 );

			// Con_Printf( "%i Setting up %s global %i\n", k, hdr->pFlexcontroller(k)->pszName(), controller );

			slFlexScale[j]->setLabel( hdr->pFlexcontroller(k)->pszName() );

			if ( nFlexSliderIndex[controller] == FLEXSLIDER_INVALID_INDEX )
			{
				//Con_Printf( "Assigning bar %i to barnum %i of slider %s for controller %i\n",
				//	j, 0, hdr->pFlexcontroller(k)->pszName(), controller );

				nFlexSliderIndex[controller] = j;
				nFlexSliderBarnum[controller] = 0;
			}
			else
			{
				Assert( 0 );
			}

			if (hdr->pFlexcontroller(k)->min != hdr->pFlexcontroller(k)->max)
				slFlexScale[j]->setRange( 0, hdr->pFlexcontroller(k)->min, hdr->pFlexcontroller(k)->max );

			if (strncmp( "right_", hdr->pFlexcontroller(k)->pszName(), 6 ) == 0)
			{
				if (hdr->pFlexcontroller(k)->min != hdr->pFlexcontroller(k)->max)
					slFlexScale[j]->setRange( 1, 0.0f, 1.0f );
				slFlexScale[j]->setLabel( &hdr->pFlexcontroller(k)->pszName()[6] );

				slFlexScale[j]->SetMode( true );
				k++;
				controller = hdr->pFlexcontroller( k )->localToGlobal;
				Assert( controller != -1 );
				if ( nFlexSliderIndex[controller] == FLEXSLIDER_INVALID_INDEX )
				{
					nFlexSliderIndex[controller] = j;
					nFlexSliderBarnum[controller] = 1;

					//Con_Printf( "Assigning stereo side of bar %i to barnum %i of slider %s for controller\n",
					//	j, 1, hdr->pFlexcontroller(k)->pszName(), controller );

				}
				else
				{
					Assert(0);
				}
			}
			m_nViewableFlexControllerCount++;

			slFlexScale[j]->setVisible( true );
			slFlexScale[j]->redraw();

			j++;
		}
	}

	slScrollbar->setRange( 0, m_nViewableFlexControllerCount * LINE_HEIGHT + 5 );

	int trueh = h2() - GetCaptionHeight();
	PositionControls( w2(), h2() );
	slScrollbar->setPagesize( trueh );
	slScrollbar->setBounds( w2() - 18, GetCaptionHeight(), 18, trueh );
	PositionSliders( 0 );
}


void FlexPanel::OnModelChanged()
{
	ResetSliders( true, false );
	SetEvent( NULL );
	redraw();
}

void FlexPanel::SetEvent( CChoreoEvent *event )
{
	bool bUpdateSliders = false;

	if ( event != NULL )
	{
		CChoreoScene *scene = event->GetScene();
		StudioModel *model = FindAssociatedModel( scene, event->GetActor()  );

		if (model == models->GetActiveStudioModel())
		{
			bUpdateSliders = true;
		}
	}

	btnCopyToSliders->setEnabled( bUpdateSliders );
	btnCopyFromSliders->setEnabled( bUpdateSliders );
	return;
}


bool FlexPanel::IsValidSlider( int iFlexController ) const
{
	if ( nFlexSliderIndex[ iFlexController ] == FLEXSLIDER_INVALID_INDEX )
		return false;

	return true;
}

float	
FlexPanel::GetSlider( int iFlexController )
{
	if ( !IsValidSlider( iFlexController ) )
	{
		Msg( "GetSlider(%d) invalid controller index\n", iFlexController );
		return 0.0f;
	}

	return slFlexScale[ nFlexSliderIndex[ iFlexController ] ]->getValue( nFlexSliderBarnum[ iFlexController ] );
}

float FlexPanel::GetSliderRawValue( int iFlexController )
{
	if ( !IsValidSlider( iFlexController ) )
	{
		Msg( "GetSliderRawValue(%d) invalid controller index\n", iFlexController );
		return 0.0f;
	}

	return slFlexScale[ nFlexSliderIndex[ iFlexController ] ]->getRawValue( nFlexSliderBarnum[ iFlexController ] );
}

void FlexPanel::GetSliderRange( int iFlexController, float& minvalue, float& maxvalue )
{
	int barnum = nFlexSliderBarnum[ iFlexController ];

	if ( !IsValidSlider( iFlexController ) )
	{
		Msg( "GetSliderRange(%d) invalid controller index\n", iFlexController );

		minvalue = 0.0f;
		maxvalue = 1.0f;
		return;
	}

	mxExpressionSlider *sl = slFlexScale[ nFlexSliderIndex[ iFlexController ] ]; 
	Assert( sl );
	minvalue = sl->getMinValue( barnum );
	maxvalue = sl->getMaxValue( barnum );
}

void
FlexPanel::SetSlider( int iFlexController, float value )
{
	if ( !IsValidSlider( iFlexController ) )
	{
		Msg( "SetSlider(%d) invalid controller index\n", iFlexController );
		return;
	}

	slFlexScale[ nFlexSliderIndex[ iFlexController ] ]->setValue( nFlexSliderBarnum[ iFlexController ], value );
}

float	
FlexPanel::GetInfluence( int iFlexController )
{
	if ( !IsValidSlider( iFlexController ) )
	{
		Msg( "GetInfluence(%d) invalid controller index\n", iFlexController );
		return 0.0f;
	}

	return slFlexScale[ nFlexSliderIndex[ iFlexController ] ]->getInfluence( );
}

void
FlexPanel::SetEdited( int iFlexController, bool isEdited )
{
	if ( !IsValidSlider( iFlexController ) )
	{
		Msg( "IsEdited(%d) invalid controller index\n", iFlexController );
		return;
	}

	slFlexScale[ nFlexSliderIndex[ iFlexController ] ]->setEdited( nFlexSliderBarnum[ iFlexController ], isEdited );
}

bool
FlexPanel::IsEdited( int iFlexController )
{
	if ( !IsValidSlider( iFlexController ) )
	{
		Msg( "IsEdited(%d) invalid controller index\n", iFlexController );
		return 0.0f;
	}

	return slFlexScale[ nFlexSliderIndex[ iFlexController ] ]->isEdited( nFlexSliderBarnum[ iFlexController ] );
}

void
FlexPanel::SetInfluence( int iFlexController, float value )
{
	if ( !IsValidSlider( iFlexController ) )
	{
		Msg( "SetInfluence(%d) invalid controller index\n", iFlexController );
		return;
	}

	// Con_Printf( "SetInfluence( %d, %.0f ) : %d %d\n", iFlexController, value, nFlexSliderIndex[ iFlexController ], nFlexSliderBarnum[ iFlexController ] );
	if ( nFlexSliderBarnum[ iFlexController ] == 0)
	{
		slFlexScale[ nFlexSliderIndex[ iFlexController ] ]->setInfluence( value );
	}
}

int
FlexPanel::LookupFlex( int iSlider, int barnum )
{
	for (int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++)
	{
		if (nFlexSliderIndex[i] == iSlider && nFlexSliderBarnum[i] == barnum)
		{
			// char const *name = GetGlobalFlexControllerName( i );
			//Con_Printf( "lookup slider %i bar %i == %s\n",
				//iSlider, barnum, name );

			return i;
		}
	}

	Con_Printf( "lookup slider %i bar %i failed\n",
		iSlider, barnum);
	return 0;
}


int
FlexPanel::LookupPairedFlex( int iFlexController )
{
	if ( !IsValidSlider( iFlexController ) )
	{
		Msg( "LookupPairedFlex(%d) invalid controller index\n", iFlexController );
		return iFlexController;
	}

	if (nFlexSliderBarnum[ iFlexController ] == 1)
	{
		return iFlexController - 1;
	}
	else if (nFlexSliderIndex[ iFlexController + 1 ] == nFlexSliderIndex[ iFlexController ])
	{
		return iFlexController + 1;
	}
	return iFlexController;
}

void
FlexPanel::setExpression( int index )
{
	if ( !models->GetActiveStudioModel() )
		return;

	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( !hdr )
		return;

	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return;

	CExpression *exp = active->GetExpression( index );
	if ( !exp )
		return;

	// Con_Printf( "Setting expression to %i:'%s'\n", index, exp->name );

	float *settings = exp->GetSettings();
	float *weights = exp->GetWeights();
	Assert( settings );	
	Assert( weights );	

	for (LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
	{
		int j = hdr->pFlexcontroller( i )->localToGlobal;
		if ( j == -1 )
			continue;

		//if ( weights[j] > 0.0f )
		//{
		//	Con_Printf( "%i Setting %s to %f\n", j, GetGlobalFlexControllerName( j ),
		//		settings[ j ] );
		//}

		SetSlider( j, settings[j] );
		SetInfluence( j, weights[j] );
		models->GetActiveStudioModel()->SetFlexController( i, settings[j] * weights[j] );
	}
}

void FlexPanel::DeleteExpression( int index )
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( !hdr )
		return;
	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return;

	CExpression *exp = active->GetExpression( index );
	if ( !exp )
		return;

	active->DeleteExpression( exp->name );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//-----------------------------------------------------------------------------
void FlexPanel::RevertExpression( int index )
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( !hdr )
		return;

	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return;

	CExpression *exp = active->GetExpression( index );
	if ( !exp )
		return;

	exp->Revert();
	setExpression( index );
	g_pExpressionTrayTool->redraw();
}

void FlexPanel::SaveExpression( int index )
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( !hdr )
		return;

	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return;

	CExpression *exp = active->GetExpression( index );
	if ( !exp )
		return;

	int retval = mxMessageBox( this, "Overwrite existing expression?", g_appTitle, MX_MB_YESNO | MX_MB_QUESTION );
	if ( retval != 0 )
		return;

	float *settings = exp->GetSettings();
	float *weights = exp->GetWeights();
	Assert( settings );	
	Assert( weights );	
	for ( LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++ )
	{
		int j = hdr->pFlexcontroller( i )->localToGlobal;

		settings[ j ] = GetSlider( j );
		weights[ j ] = GetInfluence( j );
	}

	exp->CreateNewBitmap( models->GetActiveModelIndex() );

	exp->ResetUndo();

	exp->SetDirty( false );

	g_pExpressionTrayTool->redraw();
}

void FlexPanel::CopyControllerSettingsToStructure( CExpression *exp )
{
	Assert( exp );

	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( hdr )
	{
		float *settings = exp->GetSettings();
		float *weights = exp->GetWeights();

		for (LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
		{
			int j = hdr->pFlexcontroller( i )->localToGlobal;

			settings[ j ] = GetSlider( j );
			weights[ j ] = GetInfluence( j );
		}
	}
}

void FlexPanel::OnSetAll( int state )
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( hdr )
	{
		for (LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
		{
			int j = hdr->pFlexcontroller( i )->localToGlobal;

			float setting = GetSlider( j );
			float influence = GetInfluence( j );
			switch ( state )
			{
			default:
				Assert( 0 );
				break;
			case FP_STATE_UNCHECK:
				influence = 0.0f;
				break;
			case FP_STATE_CHECK:
				influence = 1.0f;
				break;
			case FP_STATE_INVERT:
				influence = 1.0f - influence;
				break;
			}
			
			SetInfluence( j, influence );
			models->GetActiveStudioModel()->SetFlexController( i, setting * influence );
		}
	}
}

void FlexPanel::ResetSliders( bool preserveundo, bool bDirtyClass )
{
	CExpClass *active = expressions->GetActiveClass();

	bool needredo = false;
	CExpression zeroes;

	CExpression *exp = NULL;
	if ( active )
	{
		int index = active->GetSelectedExpression();
		if ( index != -1 )
		{
			exp = active->GetExpression( index );
			if ( exp )
			{
				float *settings = exp->GetSettings();
				Assert( settings );

				if ( memcmp( settings, zeroes.GetSettings(), GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) ) )
				{
					if ( preserveundo )
					{
						exp->PushUndoInformation();
						needredo = true;
					}

					if ( bDirtyClass )
					{
						active->SetDirty( true );	
					}

					g_pExpressionTrayTool->redraw();
				}
			}
		}
	}

	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( hdr )
	{
		if( exp )
		{
			float *settings = exp->GetSettings();
			float *weights = exp->GetWeights();

			Assert( settings && weights );

			for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
			{
				settings[ i ] = 0.0f;
				weights[ i ] = 0.0f;
			}
		}

		for ( LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++ )
		{
			int j = hdr->pFlexcontroller( i )->localToGlobal;

			if ( j == -1 )
				continue;

			SetSlider( j, 0.0f );
			SetInfluence( j, 0.0f );
			SetEdited( j, false );
			models->GetActiveStudioModel()->SetFlexController( i, 0.0f );
		}
	}

	if ( exp && needredo && preserveundo )
	{
		exp->PushRedoInformation();
	}
}

void FlexPanel::CopyControllerSettings( void )
{
	CExpression *exp = expressions->GetCopyBuffer();
	memset( exp, 0, sizeof( *exp ) );
	CopyControllerSettingsToStructure( exp );
}

void FlexPanel::PasteControllerSettings( void )
{
	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return;

	bool needredo = false;
	CExpression *paste = expressions->GetCopyBuffer();
	if ( !paste )
		return;

	CExpression *exp = NULL;
	int index = active->GetSelectedExpression();
	if ( index != -1 )
	{
		exp = active->GetExpression( index );
		if ( exp )
		{
			float *settings = exp->GetSettings();
			Assert( settings );

			// UPDATEME
			if ( memcmp( settings, paste->GetSettings(), GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) ) )
			{
				exp->PushUndoInformation();
				needredo = true;

				active->SetDirty( true );

				g_pExpressionTrayTool->redraw();
			}
		}
	}

	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( hdr )
	{
		float *settings = paste->GetSettings();
		float *weights = paste->GetWeights();
		Assert( settings );
		Assert( weights );

		for (LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
		{
			int j = hdr->pFlexcontroller( i )->localToGlobal;

			SetSlider( j, settings[j] );
			SetInfluence( j, weights[j] );
			models->GetActiveStudioModel()->SetFlexController( i, settings[j] * weights[j] );
		}
	}

	if ( exp && needredo )
	{
		exp->PushRedoInformation();
	}

}

void FlexPanel::EditExpression( void )
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( !hdr )
	{
		Con_ErrorPrintf( "Can't edit face pose, must load a model first!\n" );
		return;
	}
	
	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return;

	int index = active->GetSelectedExpression();
	if ( index == -1 )
	{
		Con_ErrorPrintf( "Can't edit face pose, must select a face from list first!\n" );
		return;
	}

	CExpression *exp = active->GetExpression( index );
	if ( !exp )
	{
		return;
	}

	bool namechanged = false;
	CExpressionParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Edit Expression" );
	strcpy( params.m_szName, exp->name );
	strcpy( params.m_szDescription, exp->description );

	if ( !ExpressionProperties( &params ) )
		return;

	namechanged = stricmp( exp->name, params.m_szName ) ? true : false;

	if ( ( strlen( params.m_szName ) <= 0 ) ||
		!stricmp( params.m_szName, "unnamed" ) )
	{
		Con_ErrorPrintf( "You must type in a valid name\n" );
		return;
	}

	if ( ( strlen( params.m_szDescription ) <= 0 ) ||
   	   !stricmp( params.m_szDescription, "description" ) )
	{
		Con_ErrorPrintf( "You must type in a valid description\n" );
		return;
	}

	if ( namechanged )
	{
		Con_Printf( "Deleting old bitmap %s\n", exp->GetBitmapFilename( models->GetActiveModelIndex() ) );

		// Remove old bitmap
		_unlink( exp->GetBitmapFilename( models->GetActiveModelIndex() ) );
	}

	strcpy( exp->name, params.m_szName );
	strcpy( exp->description, params.m_szDescription );

	if ( namechanged )
	{
		exp->CreateNewBitmap( models->GetActiveModelIndex() );
	}

	active->SetDirty( true );

	g_pExpressionTrayTool->redraw();
}

void FlexPanel::NewExpression( void )
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( !hdr )
	{
		Con_ErrorPrintf( "Can't create new face pose, must load a model first!\n" );
		return;
	}

	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return;

	g_pExpressionTrayTool->Deselect();

	CExpressionParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Add Expression" );
	strcpy( params.m_szName, "" );
	strcpy( params.m_szDescription, "" );

	if ( !ExpressionProperties( &params ) )
		return;

	if ( ( strlen( params.m_szName ) <= 0 ) ||
		!stricmp( params.m_szName, "unnamed" ) )
	{
		Con_ErrorPrintf( "You must type in a valid name\n" );
		return;
	}

	if ( ( strlen( params.m_szDescription ) <= 0 ) ||
   	   !stricmp( params.m_szDescription, "description" ) )
	{
		Con_ErrorPrintf( "You must type in a valid description\n" );
		return;
	}

	float settings[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];
	float weights[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];
	memset( settings, 0, sizeof( settings ) );
	memset( weights, 0, sizeof( settings ) );
	for ( LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++ )
	{
		int j = hdr->pFlexcontroller( i )->localToGlobal;

		settings[ j ] = GetSlider( j );
		weights[ j ] = GetInfluence( j );
	}	

	active->AddExpression( params.m_szName, params.m_szDescription, settings, weights, true, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool FlexPanel::PaintBackground( void )
{
	redraw();
	return false;
}

void FlexPanel::OnMenu()
{
	POINT pt;
	pt.x = btnMenu->x();
	pt.y = btnMenu->y();
	pt.y -= 3 * btnMenu->h2();
	ScreenToClient( (HWND)getHandle(), &pt );

	mxPopupMenu *pop = new mxPopupMenu();

	pop->add( "Check All", IDC_FP_CHECK_ALL );
	pop->add( "Uncheck All", IDC_FP_UNCHECK_ALL );
	pop->add( "Invert Selection", IDC_FP_INVERT );

	pop->popup( this, pt.x, pt.y );
}
