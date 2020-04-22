//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "ControlPanel.h"
#include "ViewerSettings.h"
#include "StudioModel.h"
#include "IStudioRender.h"
#include "MatSysWin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mxtk/mx.h>
#include <mxtk/mxBmp.h>
#include "FlexPanel.h"
#include "mxExpressionTray.h"
#include "PhonemeEditor.h"
#include "hlfaceposer.h"
#include "expclass.h"
#include "mxExpressionTab.h"
#include "ExpressionTool.h"
#include "MDLViewer.h"
#include "choreowidgetdrawhelper.h"
#include "faceposer_models.h"
#include "ifaceposerworkspace.h"
#include "choreoview.h"
#include "GestureTool.h"
#include "RampTool.h"
#include "SceneRampTool.h"
#include "phonemeextractor/PhonemeExtractor.h"
#include "tier1/KeyValues.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
extern char g_appTitle[];

ControlPanel *g_pControlPanel = 0;

//-----------------------------------------------------------------------------
// Purpose: A simple subclass so we can paint the window background
//-----------------------------------------------------------------------------
class CControlPanelTabWindow : public mxWindow
{
public:
	CControlPanelTabWindow( mxWindow *parent, int x, int y, int w, int h ) :
	  mxWindow( parent, x, y, w, h )
	  {
		  FacePoser_AddWindowStyle( this, WS_CLIPSIBLINGS | WS_CLIPCHILDREN );
	  };
	  
	  virtual bool PaintBackground( void )
	  {
		  CChoreoWidgetDrawHelper drawHelper( this );
		  
		  RECT rc;
		  drawHelper.GetClientRect( rc );
		  
		  drawHelper.DrawFilledRect( GetSysColor( COLOR_BTNFACE ), rc );
		  return false;
	  }
};

ControlPanel::ControlPanel (mxWindow *parent)
: IFacePoserToolWindow( "ControlPanel", "Control Panel" ), mxWindow( parent, 0, 0, 0, 0 ), tab( 0 )
{
	// create tabcontrol with subdialog windows
	tab = new mxTab (this, 0, 0, 0, 0, IDC_TAB);

	CControlPanelTabWindow *wRender = new CControlPanelTabWindow (this, 0, 0, 0, 0);
	tab->add (wRender, "Render");
	cRenderMode = new mxChoice (wRender, 5, 5, 100, 22, IDC_RENDERMODE);
	cRenderMode->add ("Wireframe");
	cRenderMode->add ("Flatshaded");
	cRenderMode->add ("Smoothshaded");
	cRenderMode->add ("Textured");
	cRenderMode->select (3);
	mxToolTip::add (cRenderMode, "Select Render Mode");

	slModelGap = new mxSlider( wRender, 220, 5, 140, 20, IDC_MODELSPACING );
	slModelGap->setRange( 0.0f, 64.0f, 256 );
	slModelGap->setValue( 16 );
	mxToolTip::add (slModelGap, "Select Model Spacing" );
	new mxLabel (wRender, 220, 25, 140, 20, "Model Spacing");

	cbAllWindowsDriveSpeech = new mxCheckBox( wRender, 220, 45, 140, 20, "All tools drive mouth", IDC_TOOLSDRIVEMOUTH );
	cbAllWindowsDriveSpeech->setChecked( g_viewerSettings.faceposerToolsDriveMouth );

	cbGround = new mxCheckBox (wRender, 110, 5, 100, 20, "Ground", IDC_GROUND);
	cbGround->setEnabled( true );
	cbMovement = new mxCheckBox (wRender, 110, 25, 100, 20, "Movement", IDC_MOVEMENT);
	cbMovement->setEnabled( false );
	cbBackground = new mxCheckBox (wRender, 110, 45, 100, 20, "Background", IDC_BACKGROUND);
	cbBackground->setEnabled( false );
	new mxCheckBox (wRender, 110, 65, 100, 20, "Hit Boxes", IDC_HITBOXES);
	new mxCheckBox (wRender, 5, 65, 100, 20, "Bones", IDC_BONES);
	mxCheckBox *cbAttachments = new mxCheckBox (wRender, 5, 45, 100, 20, "Attachments", IDC_ATTACHMENTS);
	cbAttachments->setEnabled( false );

	CControlPanelTabWindow *wSequence = new CControlPanelTabWindow (this, 0, 0, 0, 0);
	tab->add (wSequence, "Sequence");
	cSequence = new mxChoice (wSequence, 5, 5, 200, 22, IDC_SEQUENCE);	
	mxToolTip::add (cSequence, "Select Sequence");


	slSpeedScale = new mxSlider (wSequence, 5, 32, 200, 18, IDC_SPEEDSCALE);
	slSpeedScale->setRange (0.0, 5.0 );
	slSpeedScale->setValue (0.0);
	mxToolTip::add (slSpeedScale, "Speed Scale");
	lSpeedScale = new mxLabel( wSequence, 5, 50, 200, 18 );
	lSpeedScale->setLabel( "Speed scale" );

	CControlPanelTabWindow *wBody = new CControlPanelTabWindow (this, 0, 0, 0, 0);
	tab->add (wBody, "Body");
	cBodypart = new mxChoice (wBody, 5, 5, 100, 22, IDC_BODYPART);
	mxToolTip::add (cBodypart, "Choose a bodypart");
	cSubmodel = new mxChoice (wBody, 110, 5, 100, 22, IDC_SUBMODEL);
	mxToolTip::add (cSubmodel, "Choose a submodel of current bodypart");
	cController = new mxChoice (wBody, 5, 30, 100, 22, IDC_CONTROLLER);	
	mxToolTip::add (cController, "Choose a bone controller");
	slController = new mxSlider (wBody, 105, 32, 100, 18, IDC_CONTROLLERVALUE);
	slController->setRange (0, 45);
	mxToolTip::add (slController, "Change current bone controller value");
	lModelInfo1 = new mxLabel (wBody, 220, 5, 120, 100, "No Model.");
	lModelInfo2 = new mxLabel (wBody, 340, 5, 120, 100, "");
	cSkin = new mxChoice (wBody, 5, 55, 100, 22, IDC_SKINS);
	mxToolTip::add (cSkin, "Choose a skin family");
}

ControlPanel::~ControlPanel()
{
}

bool ControlPanel::CanClose( void )
{
	workspacefiles->StartStoringFiles( IWorkspaceFiles::EXPRESSION );
	for ( int i = 0 ; i < expressions->GetNumClasses(); i++ )
	{
		CExpClass *cl = expressions->GetClass( i );
		if ( cl )
		{
			workspacefiles->StoreFile( IWorkspaceFiles::EXPRESSION, cl->GetFileName() );
		}
	}
	workspacefiles->FinishStoringFiles( IWorkspaceFiles::EXPRESSION );
	// Now close them all, or abort exit if user doesn't want to close any that have changed
	return Closeall();
}

void ControlPanel::OnDelete()
{
}

void ControlPanel::PositionControls( int width, int height )
{
	if ( tab )
	{
		tab->setBounds( 0, GetCaptionHeight(), width, height );
	}
}

void ControlPanel::redraw()
{
	if ( !ToolCanDraw() )
		return;

	CChoreoWidgetDrawHelper helper( this, GetSysColor( COLOR_BTNFACE ) );
	HandleToolRedraw( helper );

	BaseClass::redraw();
}

int
ControlPanel::handleEvent (mxEvent *event)
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
			PositionControls( event->width, event->height );
			iret = 1;
		}
		break;
	case mxEvent::Action:
		{
			iret = 1;
			switch (event->action)
			{
			case IDC_TOOLSDRIVEMOUTH:
				{
					g_viewerSettings.faceposerToolsDriveMouth = ((mxCheckBox *)event->widget)->isChecked();
				}
				break;
			case IDC_TAB:
				{
					g_viewerSettings.showTexture = (tab->getSelectedIndex() == 3);
				}
				break;
				
			case IDC_RENDERMODE:
				{
					int index = cRenderMode->getSelectedIndex();
					if (index >= 0)
					{
						setRenderMode (index);
					}
				}
				break;
				
			case IDC_GROUND:
				setShowGround (((mxCheckBox *) event->widget)->isChecked());
				break;
				
			case IDC_MOVEMENT:
				setShowMovement (((mxCheckBox *) event->widget)->isChecked());
				break;
				
			case IDC_BACKGROUND:
				setShowBackground (((mxCheckBox *) event->widget)->isChecked());
				break;
				
			case IDC_HITBOXES:
				g_viewerSettings.showHitBoxes = ((mxCheckBox *) event->widget)->isChecked();
				break;
				
			case IDC_PHYSICSMODEL:
				g_viewerSettings.showPhysicsModel = ((mxCheckBox *) event->widget)->isChecked();
				break;
				
			case IDC_BONES:
				g_viewerSettings.showBones = ((mxCheckBox *) event->widget)->isChecked();
				break;
				
			case IDC_ATTACHMENTS:
				g_viewerSettings.showAttachments = ((mxCheckBox *) event->widget)->isChecked();
				break;
				
			case IDC_SEQUENCE:
				{
					int index = cSequence->getSelectedIndex();
					if (index >= 0)
					{
						setSequence ( index );
					}
				}
				break;
			
			case IDC_SPEEDSCALE:
				{
					g_viewerSettings.speedScale = ((mxSlider *) event->widget)->getValue();
					lSpeedScale->setLabel( va( "Speed scale %.2f", g_viewerSettings.speedScale ) );
				}
				break;
				
			case IDC_PRIMARYBLEND:
				{
					setBlend( 0, ((mxSlider *) event->widget)->getValue() );
				}
				break;
				
			case IDC_SECONDARYBLEND:
				{
					setBlend( 1, ((mxSlider *) event->widget)->getValue() );
				}
				break;
				
			case IDC_BODYPART:
				{
					int index = cBodypart->getSelectedIndex();
					if (index >= 0)
					{
						setBodypart (index);
						
					}
				}
				break;
				
			case IDC_SUBMODEL:
				{
					int index = cSubmodel->getSelectedIndex();
					if (index >= 0)
					{
						setSubmodel (index);
						
					}
				}
				break;
				
			case IDC_CONTROLLER:
				{
					int index = cController->getSelectedIndex();
					if (index >= 0)
						setBoneController (index);
				}
				break;
				
			case IDC_CONTROLLERVALUE:
				{
					int index = cController->getSelectedIndex();
					if (index >= 0)
						setBoneControllerValue (index, slController->getValue());
				}
				break;
				
			case IDC_SKINS:
				{
					int index = cSkin->getSelectedIndex();
					if (index >= 0)
					{
						models->GetActiveStudioModel()->SetSkin (index);
						g_viewerSettings.skin = index;
						g_pMatSysWindow->redraw();
					}
				}
				break;
			default:
				iret = 0;
				break;
			}
		}
	}

	return iret;
}

void ControlPanel::dumpModelInfo() { }

void ControlPanel::ChangeModel( const char *filename )
{
	HCURSOR hPrevCursor = SetCursor( LoadCursor( NULL, IDC_WAIT ) );

	// init all the selection tabs based on the current model
	initSequenceChoices();
	initBodypartChoices();
	initBoneControllerChoices();
	initSkinChoices();

	setModelInfo();

	SetCloseCaptionLanguageId( g_viewerSettings.cclanguageid, true );

	g_viewerSettings.m_iEditAttachment = -1;

	g_viewerSettings.enableIK = true;
	g_viewerSettings.enableTargetIK = false;

	setSequence( models->GetActiveStudioModel()->GetSequence() );
	setSpeed( g_viewerSettings.speedScale );

	mx_setcwd (mx_getpath (filename));

	g_pFlexPanel->initFlexes();
	
	//	centerView();
	//	CenterOnFace();

	IFacePoserToolWindow::ModelChanged();
	
	CExpClass *cl = expressions->GetActiveClass();
	if ( cl )
	{
		cl->SelectExpression( cl->GetSelectedExpression() );
	}

	SetSuffix( va( " - %s.mdl", models->GetActiveModelName() ) );
	redraw();

	SetCursor( hPrevCursor );
}



void
ControlPanel::setRenderMode (int mode)
{
	g_viewerSettings.renderMode = mode;
	g_pMatSysWindow->redraw();
}


void 
ControlPanel::setHighlightBone( int index )
{
	g_viewerSettings.highlightPhysicsBone = index;
}

void
ControlPanel::setShowGround (bool b)
{
	g_viewerSettings.showGround = b;
	cbGround->setChecked (b);
}



void
ControlPanel::setShowMovement (bool b)
{
	g_viewerSettings.showMovement = b;
	cbMovement->setChecked (b);
}



void
ControlPanel::setShowBackground (bool b)
{
	g_viewerSettings.showBackground = b;
	cbBackground->setChecked (b);
}



void
ControlPanel::initSequenceChoices()
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if (hdr)
	{
		cSequence->removeAll();
		for (int i = 0; i < hdr->GetNumSeq(); i++)
		{
			cSequence->add (hdr->pSeqdesc(i).pszLabel());
		}

		cSequence->select (0);
	}
}



void
ControlPanel::setSequence (int index)
{
	cSequence->select (index);
	models->GetActiveStudioModel()->SetSequence(index);

	initPoseParameters( );
}


void
ControlPanel::setSpeed( float value )
{
	g_viewerSettings.speedScale = value;
	slSpeedScale->setValue( value );
}


void ControlPanel::setBlend(int index, float value )
{
	models->GetActiveStudioModel()->SetPoseParameter( index, value );
}


void
ControlPanel::initBodypartChoices()
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if (hdr)
	{
		int i;
		mstudiobodyparts_t *pbodyparts = hdr->pBodypart(0);

		cBodypart->removeAll();
		if (hdr->numbodyparts() > 0)
		{
			for (i = 0; i < hdr->numbodyparts(); i++)
				cBodypart->add (pbodyparts[i].pszName());

			cBodypart->select (0);

			cSubmodel->removeAll();
			for (i = 0; i < pbodyparts[0].nummodels; i++)
			{
				char str[64];
				sprintf (str, "Submodel %d", i + 1);
				cSubmodel->add (str);
			}
			cSubmodel->select (0);
		}
	}
}



void
ControlPanel::setBodypart (int index)
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if (hdr)
	{
		//cBodypart->setEn
		cBodypart->select (index);
		if (index < hdr->numbodyparts())
		{
			mstudiobodyparts_t *pbodyparts = hdr->pBodypart(0);
			cSubmodel->removeAll();
		
			for (int i = 0; i < pbodyparts[index].nummodels; i++)
			{
				char str[64];
				sprintf (str, "Submodel %d", i + 1);
				cSubmodel->add (str);
			}
			cSubmodel->select (0);
			//models->GetActiveStudioModel()->SetBodygroup (index, 0);
		}
	}
}



void
ControlPanel::setSubmodel (int index)
{
	models->GetActiveStudioModel()->SetBodygroup (cBodypart->getSelectedIndex(), index);
}



void
ControlPanel::initBoneControllerChoices()
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if (hdr)
	{
		cController->setEnabled (hdr->numbonecontrollers() > 0);
		slController->setEnabled (hdr->numbonecontrollers() > 0);
		cController->removeAll();

		for (int i = 0; i < hdr->numbonecontrollers(); i++)
		{
			mstudiobonecontroller_t *pbonecontroller = hdr->pBonecontroller(i);
			char str[32];
			sprintf (str, "Controller %d", pbonecontroller->inputfield);
			cController->add (str);
		}

		if (hdr->numbonecontrollers() > 0)
		{
			mstudiobonecontroller_t *pbonecontrollers = hdr->pBonecontroller(0);
			cController->select (0);
			slController->setRange (pbonecontrollers->start, pbonecontrollers->end);
			slController->setValue (0);
		}
	}
}



void
ControlPanel::setBoneController (int index)
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if (hdr)
	{
		mstudiobonecontroller_t *pbonecontroller = hdr->pBonecontroller(index);
		slController->setRange ( pbonecontroller->start, pbonecontroller->end);
		slController->setValue (0);
	}
}



void
ControlPanel::setBoneControllerValue (int index, float value)
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if (hdr)
	{
		mstudiobonecontroller_t *pbonecontrollers = hdr->pBonecontroller(index);
		models->GetActiveStudioModel()->SetController (pbonecontrollers->inputfield, value);
	}
}



void
ControlPanel::initPoseParameters()
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if (hdr)
	{
		for (int i = 0; i < hdr->GetNumPoseParameters(); i++)
		{
			setBlend( i, 0.0 );
		}
	}
}

void
ControlPanel::initSkinChoices()
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if (hdr)
	{
		cSkin->setEnabled (hdr->numskinfamilies() > 0);
		cSkin->removeAll();

		for (int i = 0; i < hdr->numskinfamilies(); i++)
		{
			char str[32];
			sprintf (str, "Skin %d", i + 1);
			cSkin->add (str);
		}

		cSkin->select (0);
		models->GetActiveStudioModel()->SetSkin (0);
		g_viewerSettings.skin = 0;
	}
}



void
ControlPanel::setModelInfo()
{
	static char str[2048];
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();

	if (!hdr)
		return;

	int hbcount = 0;
	for ( int s = 0; s < hdr->numhitboxsets(); s++ )
	{
		hbcount += hdr->iHitboxCount( s );
	}

	sprintf (str,
		"Bones: %d\n"
		"Bone Controllers: %d\n"
		"Hit Boxes: %d in %d sets\n"
		"Sequences: %d\n",
		hdr->numbones(),
		hdr->numbonecontrollers(),
		hbcount,
		hdr->numhitboxsets(),
		hdr->GetNumSeq()
		);

	lModelInfo1->setLabel (str);

	sprintf (str,
		"Textures: %d\n"
		"Skin Families: %d\n"
		"Bodyparts: %d\n"
		"Attachments: %d\n",
		hdr->numtextures(),
		hdr->numskinfamilies(),
		hdr->numbodyparts(),
		hdr->GetNumAttachments());

	lModelInfo2->setLabel (str);
}


void ControlPanel::CenterOnFace( void )
{
	if ( !models->GetActiveStudioModel() )
		return;

	StudioModel *mdl = models->GetActiveStudioModel();
	if ( !mdl )
		return;

	CStudioHdr *hdr = mdl->GetStudioHdr();
	if ( !hdr )
		return;

	setSpeed( 1.0f );

	int oldSeq = models->GetActiveStudioModel()->GetSequence();

	int seq = models->GetActiveStudioModel()->LookupSequence( "idle_suble" );
	if ( seq == -1 )
		seq = 0;

	if ( seq != oldSeq )
	{
		Con_Printf( "Centering changed model sequence # to %d\n", seq );
	}

	setSequence( seq );
	initPoseParameters( );

	mdl->m_angles.Init();
	mdl->m_origin.Init();

	Vector size;
	VectorSubtract( hdr->hull_max(), hdr->hull_min(), size );

	float eyeheight = hdr->hull_min().z + 0.9 * size.z;

	if ( hdr->GetNumAttachments() > 0 )
	{
		for (int i = 0; i < hdr->GetNumAttachments(); i++)
		{
			const mstudioattachment_t &attachment = hdr->pAttachment( i );
			int iBone = hdr->GetAttachmentBone( i );

			if ( Q_stricmp( attachment.pszName(), "eyes" ) )
				continue;

			mstudiobone_t *bone = hdr->pBone( iBone );
			if ( !bone )
				continue;

			matrix3x4_t boneToPose;
			MatrixInvert( bone->poseToBone, boneToPose );

			matrix3x4_t attachmentPoseToLocal;
			ConcatTransforms( boneToPose, attachment.local, attachmentPoseToLocal );

			Vector localSpaceEyePosition;
			VectorITransform( vec3_origin, attachmentPoseToLocal, localSpaceEyePosition );

			// Not sure why this must be negative?
			eyeheight = -localSpaceEyePosition.z + hdr->hull_min().z;
			break;
		}
	}

	KeyValues *seqKeyValues = new KeyValues("");
	if ( seqKeyValues->LoadFromBuffer( mdl->GetFileName( ), mdl->GetKeyValueText( seq ) ) )
	{
		// Do we have a build point section?
		KeyValues *pkvAllFaceposer = seqKeyValues->FindKey("faceposer");
		if ( pkvAllFaceposer )
		{
			float flEyeheight = pkvAllFaceposer->GetFloat( "eye_height", -9999.0f );
			if ( flEyeheight != -9999.0f )
			{
				eyeheight = flEyeheight;
			}
		}
	}

	seqKeyValues->deleteThis();

	mdl->m_origin.x = size.z * .65f;
	mdl->m_origin.z += eyeheight;

	CUtlVector< StudioModel * > modellist;

	modellist.AddToTail( models->GetActiveStudioModel() );

	int i;
	if ( models->CountVisibleModels() > 0 )
	{
		modellist.RemoveAll();
		for ( i = 0; i < models->Count(); i++ )
		{
			if ( models->IsModelShownIn3DView( i ) )
			{
				modellist.AddToTail( models->GetStudioModel( i ) );
			}
		}
	}

	int modelcount = modellist.Count();
	int countover2 = modelcount / 2;
	int ydelta = GetModelGap();
	int yoffset = -countover2 * ydelta;
	for ( i = 0 ; i < modelcount; i++ )
	{
		if ( models->GetStudioHeader( i ) == hdr )
		{
			mdl->m_origin.y = -yoffset;
		}
		yoffset += ydelta;
	}

	g_pMatSysWindow->redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float ControlPanel::GetModelGap( void )
{
	return slModelGap->getValue();
}

void
ControlPanel::centerView()
{
	StudioModel *pModel = models->GetActiveStudioModel();
	if ( !pModel )
		return;

	Vector min, max;
	models->GetActiveStudioModel()->ExtractBbox (min, max);

	float dx = max[0] - min[0];
	float dy = max[1] - min[1];
	float dz = max[2] - min[2];
	float d = dx;
	if (dy > d)
		d = dy;
	if (dz > d)
		d = dz;
	pModel->m_origin[0] = d * 1.0f;
	pModel->m_origin[1] = 0;
	pModel->m_origin[2] = min[2] + dz / 2;
	pModel->m_angles[0] = 0.0f;
	pModel->m_angles[1] = 0.0f;
	pModel->m_angles[2] = 0.0f;
	g_viewerSettings.lightrot.x = 0.f;
	g_viewerSettings.lightrot.y = -180.0f;
	g_viewerSettings.lightrot.z = 0.0f;
	g_pMatSysWindow->redraw();
}

bool ControlPanel::Close()
{
	int index = g_pExpressionClass->getSelectedIndex();
	CExpClass *cl = expressions->GetClass( index );
	if ( !cl )
		return true;

	return expressions->CloseClass( cl );

}

bool ControlPanel::Closeall() 
{
	bool retval = true;

	while ( expressions->GetNumClasses() > 0 )
	{
		CExpClass *cl = expressions->GetClass( 0 );
		if ( !cl )
			break;

		if ( !expressions->CloseClass( cl ) )
		{
			return false;
		}
	}
	return retval;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ControlPanel::Copy( void )
{
	g_pFlexPanel->CopyControllerSettings();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ControlPanel::Paste( void )
{
	g_pFlexPanel->PasteControllerSettings();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ControlPanel::Undo( void )
{
	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return;
	int index = active->GetSelectedExpression();
	if ( index != -1 )
	{
		UndoExpression( index );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ControlPanel::Redo( void )
{
	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return;
	int index = active->GetSelectedExpression();
	if ( index != -1 )
	{
		RedoExpression( index );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ControlPanel::UndoExpression( int index )
{
	if ( index == -1 )
		return;

	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return;

	CExpression *exp = active->GetExpression( index );
	if ( exp )
	{
		exp->Undo();
		// Show the updated data
		active->SelectExpression( index );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ControlPanel::RedoExpression( int index )
{
	if ( index == -1 )
		return;
	
	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return;

	CExpression *exp = active->GetExpression( index );
	if ( exp )
	{
		exp->Redo();
		// Show the updated data
		active->SelectExpression( index );
	}
}

void ControlPanel::DeleteExpression( int index )
{
	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return;

	CExpression *exp = active->GetExpression( index );
	if ( exp )
	{
		Con_Printf( "Deleting expression %s : %s\n", exp->name, exp->description );
			
		g_pFlexPanel->DeleteExpression( index );

		active->SelectExpression( max( 0, index - 1 ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void ControlPanel::Think( float dt )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ControlPanel::AllToolsDriveSpeech( void )
{
	return cbAllWindowsDriveSpeech->isChecked();
}
