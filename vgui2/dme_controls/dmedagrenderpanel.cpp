//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "dme_controls/dmedagrenderpanel.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/timeutils.h"
#include "movieobjects/dmeanimationlist.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmemdlmakefile.h"
#include "movieobjects/dmedccmakefile.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmedrawsettings.h"
#include "dme_controls/dmepanel.h"
#include "tier1/KeyValues.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "tier3/tier3.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imesh.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/MenuBar.h"
#include "vgui_controls/MenuButton.h"
#include "vgui/IVGui.h"


//-----------------------------------------------------------------------------
//
// Hook into the dme panel editor system
//
//-----------------------------------------------------------------------------
IMPLEMENT_DMEPANEL_FACTORY( CDmeDagRenderPanel, DmeDag, "DmeDagRenderer", "DmeDag Preview Renderer", false );
IMPLEMENT_DMEPANEL_FACTORY( CDmeDagRenderPanel, DmeSourceSkin, "DmeSourceSkinPreview", "MDL Skin Previewer", false );
IMPLEMENT_DMEPANEL_FACTORY( CDmeDagRenderPanel, DmeSourceAnimation, "DmeSourceAnimationPreview", "MDL Animation Previewer", false );
IMPLEMENT_DMEPANEL_FACTORY( CDmeDagRenderPanel, DmeDCCMakefile, "DmeMakeFileOutputPreview", "DCC MakeFile Output Preview", false );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CDmeDagRenderPanel::CDmeDagRenderPanel( vgui::Panel *pParent, const char *pName ) : BaseClass( pParent, pName )
{											 
	// Used to poll input
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	m_bDrawJointNames = false;
	m_bDrawJoints = false;
	m_bDrawGrid = true;

	// Deal with the default cubemap
	ITexture *pCubemapTexture = g_pMaterialSystem->FindTexture( "editor/cubemap", NULL, true );
	m_DefaultEnvCubemap.Init( pCubemapTexture );
	pCubemapTexture = g_pMaterialSystem->FindTexture( "editor/cubemap.hdr", NULL, true );
	m_DefaultHDREnvCubemap.Init( pCubemapTexture );

	m_pDrawSettings = CreateElement< CDmeDrawSettings >( "drawSettings", g_pDataModel->FindOrCreateFileId( "DagRenderPanelDrawSettings" ) );
	m_hDrawSettings = m_pDrawSettings;

	m_pMenuBar = new vgui::MenuBar( this, "Dag Render Panel Menu Bar" );

	m_pShadingMenu = new vgui::Menu( NULL, "Shading Menu" );
	m_nMenuSmoothShade = m_pShadingMenu->AddCheckableMenuItem( "&Smooth Shade", new KeyValues( "SmoothShade" ), this );
	m_nMenuFlatShade = m_pShadingMenu->AddCheckableMenuItem( "&Flat Shade", new KeyValues( "FlatShade" ), this );
	m_nMenuWireframe = m_pShadingMenu->AddCheckableMenuItem( "&Wireframe", new KeyValues( "Wireframe" ), this );
	m_pShadingMenu->AddSeparator();
	m_nMenuBoundingBox = m_pShadingMenu->AddCheckableMenuItem( "&Bounding Box", new KeyValues( "BoundingBox" ), this );
	m_pShadingMenu->AddSeparator();	// Bug is visibility
	m_nMenuNormals = m_pShadingMenu->AddCheckableMenuItem( "&Normals", new KeyValues( "Normals" ), this );
	m_nMenuWireframeOnShaded = m_pShadingMenu->AddCheckableMenuItem( "WireFrame &On Shaded", new KeyValues( "WireframeOnShaded" ), this );
	m_nMenuBackfaceCulling = m_pShadingMenu->AddCheckableMenuItem( "&Backface Culling", new KeyValues( "BackfaceCulling" ), this );
	m_nMenuXRay = m_pShadingMenu->AddCheckableMenuItem( "&X-Ray", new KeyValues( "XRay" ), this );
	m_nMenuGrayShade = m_pShadingMenu->AddCheckableMenuItem( "&Gray Shade", new KeyValues( "GrayShade" ), this );

	// For now...
	m_pShadingMenu->SetItemVisible( m_nMenuFlatShade, false );
	m_pShadingMenu->SetItemEnabled( m_nMenuFlatShade, false );
	m_pShadingMenu->SetItemVisible( m_nMenuBoundingBox, false );
	m_pShadingMenu->SetItemEnabled( m_nMenuBoundingBox, false );
	m_pShadingMenu->SetItemVisible( m_nMenuBackfaceCulling, false );
	m_pShadingMenu->SetItemEnabled( m_nMenuBackfaceCulling, false );
	m_pShadingMenu->SetItemVisible( m_nMenuXRay, false );
	m_pShadingMenu->SetItemEnabled( m_nMenuXRay, false );

	m_pMenuBar->AddMenu( "&Shading", m_pShadingMenu );

	UpdateMenu();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeDagRenderPanel::~CDmeDagRenderPanel()
{
}


//-----------------------------------------------------------------------------
// Scheme settings
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetBorder( pScheme->GetBorder( "MenuBorder") );
	m_hFont = pScheme->GetFont( "DmePropertyVerySmall", IsProportional() );
}


//-----------------------------------------------------------------------------
// Set the scene
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::SetDmeElement( CDmeDag *pScene )
{
	m_hDag = pScene;
	ComputeDefaultTangentData( m_hDag, false );
}


//-----------------------------------------------------------------------------
// Other methods which hook into DmePanel
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::SetDmeElement( CDmeSourceSkin *pSkin )
{
	// First, try to grab the dependent makefile
	CDmeMakefile *pSourceMakefile = pSkin->GetDependentMakefile();
	if ( !pSourceMakefile )
	{
		m_hDag = NULL;
		return;
	}

	// Next, try to grab the output of that makefile
	CDmElement *pOutput = pSourceMakefile->GetOutputElement( true );
	if ( !pOutput )
	{
		m_hDag = NULL;
		return;
	}

	// Finally, grab the 'skin' attribute of that makefile
	m_hDag = pOutput->GetValueElement< CDmeDag >( "model" );
	ComputeDefaultTangentData( m_hDag, false );
	DrawJoints( false );
	DrawJointNames( false );
}

void CDmeDagRenderPanel::SetDmeElement( CDmeSourceAnimation *pAnimation )
{
	// First, try to grab the dependent makefile
	CDmeMakefile *pSourceMakefile = pAnimation->GetDependentMakefile();
	if ( !pSourceMakefile )
	{
		m_hDag = NULL;
		return;
	}

	// Next, try to grab the output of that makefile
	CDmElement *pOutput = pSourceMakefile->GetOutputElement( true );
	if ( !pOutput )
	{
		m_hDag = NULL;
		return;
	}

	// Finally, grab the 'model' or 'skeleton' attribute of that makefile
	CDmeDag *pDag = pOutput->GetValueElement< CDmeDag >( "model" );
	if ( !pDag )
	{
		pDag = pOutput->GetValueElement< CDmeDag >( "skeleton" ); 
	}
	if ( !pDag )
		return;

	CDmeAnimationList *pAnimationList = pOutput->GetValueElement< CDmeAnimationList >( "animationList" );
	if ( !pAnimationList )
		return;

	if ( pAnimationList->FindAnimation( pAnimation->m_SourceAnimationName ) < 0 )
		return;

	m_hDag = pDag;
	ComputeDefaultTangentData( m_hDag, false );
	m_hAnimationList = pAnimationList;
	SelectAnimation( pAnimation->m_SourceAnimationName );
	DrawJoints( true );
	DrawJointNames( true );
}

void CDmeDagRenderPanel::SetDmeElement( CDmeDCCMakefile *pDCCMakefile )
{
	// First, try to grab the dependent makefile
	CDmElement *pOutputElement = pDCCMakefile->GetOutputElement( true );
	if ( !pOutputElement )
	{
		m_hDag = NULL;
		return;
	}

	// Finally, grab the 'model' or 'skeleton' attribute of that makefile
	CDmeDag *pDag = pOutputElement->GetValueElement< CDmeDag >( "model" );
	if ( !pDag )
	{
		pDag = pOutputElement->GetValueElement< CDmeDag >( "skeleton" ); 
	}
	if ( !pDag )
		return;

	CDmeAnimationList *pAnimationList = pOutputElement->GetValueElement< CDmeAnimationList >( "animationList" );

	m_hDag = pDag;
	ComputeDefaultTangentData( m_hDag, false );
	m_hAnimationList = pAnimationList;
	SelectAnimation( 0 );
	DrawJoints( pAnimationList != NULL );
	DrawJointNames( pAnimationList != NULL );
}

CDmeDag *CDmeDagRenderPanel::GetDmeElement()
{
	return m_hDag;
}


//-----------------------------------------------------------------------------
// Draw joint names
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::DrawJointNames( CDmeDag *pRoot, CDmeDag *pDag, const matrix3x4_t& parentToWorld )
{
	CDmeTransform *pJointTransform = pDag->GetTransform();
	int nJointIndex = -1;

	CDmeModel *pRootModel = CastElement<CDmeModel>( pRoot );
	if ( pRootModel )
	{
		nJointIndex = pRootModel->GetJointTransformIndex( pJointTransform );
		if ( nJointIndex < 0 && pRootModel != pDag )
			return;
	}

	matrix3x4_t jointToParent, jointToWorld;
	pJointTransform->GetTransform( jointToParent );
	ConcatTransforms( parentToWorld, jointToParent, jointToWorld );

	CDmeJoint *pJoint = CastElement< CDmeJoint >( pDag );
	if ( pJoint )
	{
		Vector vecJointOrigin;
		MatrixGetColumn( jointToWorld, 3, &vecJointOrigin );

		Vector2D vecPanelPos;
		ComputePanelPosition( vecJointOrigin, &vecPanelPos );

		char pJointName[512];
		if ( nJointIndex >= 0 )
		{
			Q_snprintf( pJointName, sizeof(pJointName), "%d : %s", nJointIndex, pJoint->GetName() );
		}
		else
		{
			Q_snprintf( pJointName, sizeof(pJointName), "%s", pJoint->GetName() );
		}
		g_pMatSystemSurface->DrawColoredText( m_hFont, vecPanelPos.x + 5, vecPanelPos.y, 255, 255, 255, 255, pJointName );
	}

	int nCount = pDag->GetChildCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeDag *pChild = pDag->GetChild(i);
		if ( !pChild )
			continue;

		DrawJointNames( pRoot, pChild, jointToWorld );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::OnSmoothShade()
{
	if ( m_pShadingMenu->IsChecked( m_nMenuSmoothShade ) )
	{
		m_pDrawSettings->SetDrawType( CDmeDrawSettings::DRAW_SMOOTH );
	}
	UpdateMenu();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::OnFlatShade()
{
	if ( m_pShadingMenu->IsChecked( m_nMenuFlatShade ) )
	{
		m_pDrawSettings->SetDrawType( CDmeDrawSettings::DRAW_FLAT );
	}
	UpdateMenu();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::OnWireframe()
{
	if ( m_pShadingMenu->IsChecked( m_nMenuWireframe ) )
	{
		m_pDrawSettings->SetDrawType( CDmeDrawSettings::DRAW_WIREFRAME );
	}
	UpdateMenu();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::OnBoundingBox()
{
	if ( m_pShadingMenu->IsChecked( m_nMenuBoundingBox ) )
	{
		m_pDrawSettings->SetDrawType( CDmeDrawSettings::DRAW_BOUNDINGBOX );
	}
	UpdateMenu();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::OnNormals()
{
	m_pDrawSettings->SetNormals( m_pShadingMenu->IsChecked( m_nMenuNormals ) );
	UpdateMenu();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::OnWireframeOnShaded()
{
	m_pDrawSettings->SetWireframeOnShaded( m_pShadingMenu->IsChecked( m_nMenuWireframeOnShaded ) );
	UpdateMenu();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::OnBackfaceCulling()
{
	m_pDrawSettings->SetBackfaceCulling( m_pShadingMenu->IsChecked( m_nMenuBackfaceCulling ) );
	UpdateMenu();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::OnXRay()
{
	m_pDrawSettings->SetXRay( m_pShadingMenu->IsChecked( m_nMenuXRay ) );
	UpdateMenu();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::OnGrayShade()
{
	m_pDrawSettings->SetGrayShade( m_pShadingMenu->IsChecked( m_nMenuGrayShade ) );
	UpdateMenu();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::OnFrame()
{
	if ( !m_hDag )
		return;

	float flRadius;
	Vector vecCenter, vecWorldCenter;
	m_hDag->GetBoundingSphere( vecCenter, flRadius );

	matrix3x4_t dmeToEngine;
	CDmeDag::DmeToEngineMatrix( dmeToEngine );
	VectorTransform( vecCenter, dmeToEngine, vecWorldCenter );
	LookAt( vecWorldCenter, flRadius );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( m_pMenuBar->IsVisible() )
	{
		int iWidth;
		int iHeight;
		GetSize( iWidth, iHeight );

		int iMenuWidth;	// Unused
		int iMenuHeight;
		m_pMenuBar->GetSize( iMenuWidth, iMenuHeight );
		m_pMenuBar->SetSize( iWidth, iMenuHeight );
	}
}


//-----------------------------------------------------------------------------
// paint it!
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::Paint()
{
	if ( m_hCurrentAnimation.Get() )
	{
		DmeTime_t currentTime( Plat_FloatTime() - m_flStartTime );
		if ( m_hCurrentAnimation->GetDuration() != DMETIME_ZERO )
		{
			currentTime = currentTime % m_hCurrentAnimation->GetDuration();
		}
		else
		{
			currentTime = DMETIME_ZERO;
		}
		currentTime += m_hCurrentAnimation->GetStartTime();
		DmeTime_t mediaTime = m_hCurrentAnimation->ToChildMediaTime( currentTime, true );

		int nChannelCount = m_hCurrentAnimation->m_Channels.Count();
		for ( int i = 0; i < nChannelCount; ++i )
		{
			m_hCurrentAnimation->m_Channels[i]->SetCurrentTime( mediaTime );
		}
	}

	if ( m_hCurrentVertexAnimation.Get() )
	{
		DmeTime_t currentTime( Plat_FloatTime() - m_flStartTime );
		currentTime = currentTime % m_hCurrentVertexAnimation->GetDuration();
		currentTime += m_hCurrentVertexAnimation->GetStartTime();
		DmeTime_t mediaTime = m_hCurrentVertexAnimation->ToChildMediaTime( currentTime, true );

		int nChannelCount = m_hCurrentVertexAnimation->m_Channels.Count();
		for ( int i = 0; i < nChannelCount; ++i )
		{
			m_hCurrentVertexAnimation->m_Channels[i]->SetCurrentTime( mediaTime );
		}
	}

	// FIXME: Shouldn't this happen at the application level?
	// run the machinery - apply, resolve, dependencies, operate, resolve
	{
		CDisableUndoScopeGuard guard;
		g_pDmElementFramework->SetOperators( m_operators );
		g_pDmElementFramework->Operate( true );
	}

	// allow elements and attributes to be edited again
	g_pDmElementFramework->BeginEdit();

	BaseClass::Paint();

	// Overlay the joint names
	if ( m_bDrawJointNames && m_hDag )
	{
		matrix3x4_t modelToWorld;
		CDmeDag::DmeToEngineMatrix( modelToWorld );
		DrawJointNames( m_hDag, m_hDag, modelToWorld );
	}
}


//-----------------------------------------------------------------------------
// Indicate we should draw joint names
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::DrawJointNames( bool bDrawJointNames )
{
	m_bDrawJointNames = bDrawJointNames;
}


//-----------------------------------------------------------------------------
// Indicate we should draw joints
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::DrawJoints( bool bDrawJoint )
{
	m_bDrawJoints = bDrawJoint;
}


//-----------------------------------------------------------------------------
// Indicate we should draw the grid
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::DrawGrid( bool bDrawGrid )
{
	m_bDrawGrid = bDrawGrid;
}


//-----------------------------------------------------------------------------
// paint it!
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::OnPaint3D()
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	ITexture *pLocalCube = pRenderContext->GetLocalCubemap();
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_NONE )
	{
		pRenderContext->BindLocalCubemap( m_DefaultEnvCubemap );
	}
	else
	{
		pRenderContext->BindLocalCubemap( m_DefaultHDREnvCubemap );
	}

	if ( m_bDrawGrid )
	{
		BaseClass::DrawGrid();
	}

	if ( m_bDrawJoints )
	{
		CDmeJoint::DrawJointHierarchy( true );
	}

	pRenderContext->CullMode( MATERIAL_CULLMODE_CW );
	CDmeDag::DrawUsingEngineCoordinates( true );
	m_pDrawSettings->DrawDag( m_hDag );
	CDmeDag::DrawUsingEngineCoordinates( false );

	pRenderContext->Flush();
	pRenderContext->BindLocalCubemap( pLocalCube );
}


//-----------------------------------------------------------------------------
// input
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::OnMouseDoublePressed( vgui::MouseCode code )
{
	OnFrame();

	BaseClass::OnMouseDoublePressed( code );
}


//-----------------------------------------------------------------------------
// TODO: Have a whole groovy keybinding thingy like SFM
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::OnKeyCodePressed( vgui::KeyCode code )
{
	BaseClass::OnKeyCodePressed( code );

	if ( code == KEY_F )
	{
		OnFrame();
	}
}


//-----------------------------------------------------------------------------
// Rebuilds the list of operators
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::RebuildOperatorList( )
{
	m_operators.RemoveAll();

	if ( m_hCurrentAnimation.Get() )
	{
		int nChannelCount = m_hCurrentAnimation->m_Channels.Count();
		for ( int i = 0; i < nChannelCount; ++i )
		{
			m_hCurrentAnimation->m_Channels[i]->SetMode( CM_PLAY );
			m_operators.AddToTail( m_hCurrentAnimation->m_Channels[i] );
		}
	}

	if ( m_hCurrentVertexAnimation.Get() )
	{
		int nChannelCount = m_hCurrentVertexAnimation->m_Channels.Count();
		for ( int i = 0; i < nChannelCount; ++i )
		{
			m_hCurrentVertexAnimation->m_Channels[i]->SetMode( CM_PLAY );
			m_operators.AddToTail( m_hCurrentVertexAnimation->m_Channels[i] );
		}
	}

	m_flStartTime = Plat_FloatTime();
}

//-----------------------------------------------------------------------------
// Select animation by index
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::SelectAnimation( int nIndex )
{
	m_hCurrentAnimation = NULL;
	if ( m_hAnimationList.Get() && ( nIndex >= 0 ) )
	{
		// FIXME: How is this actually going to work?
		m_hCurrentAnimation = m_hAnimationList->GetAnimation( nIndex );
	}
	RebuildOperatorList();
}

void CDmeDagRenderPanel::SelectVertexAnimation( int nIndex )
{
	m_hCurrentVertexAnimation = NULL;
	if ( m_hVertexAnimationList.Get() && ( nIndex >= 0 ) )
	{
		// FIXME: How is this actually going to work?
		m_hCurrentVertexAnimation = m_hVertexAnimationList->GetAnimation( nIndex );
	}
	RebuildOperatorList();
}


//-----------------------------------------------------------------------------
// Select animation by name
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::SelectAnimation( const char *pAnimName )
{
	if ( !pAnimName[0] )
	{
		SelectAnimation( -1 );
		return;
	}

	if ( m_hAnimationList )
	{
		int nIndex = m_hAnimationList->FindAnimation( pAnimName );
		if ( nIndex >= 0 )
		{
			SelectAnimation( nIndex );
		}
	}
}

void CDmeDagRenderPanel::SelectVertexAnimation( const char *pAnimName )
{
	if ( !pAnimName[0] )
	{
		SelectVertexAnimation( -1 );
		return;
	}

	if ( m_hVertexAnimationList )
	{
		int nIndex = m_hVertexAnimationList->FindAnimation( pAnimName );
		if ( nIndex >= 0 )
		{
			SelectVertexAnimation( nIndex );
		}
	}
}


//-----------------------------------------------------------------------------
// Sets animation
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::SetAnimationList( CDmeAnimationList *pAnimationList )
{
	m_hAnimationList = pAnimationList;
	int nCount = pAnimationList ? pAnimationList->GetAnimationCount() : 0;
	if ( nCount == 0 )
	{
		m_hCurrentAnimation = NULL;
		return;
	}

	SelectAnimation( 0 );
}


void CDmeDagRenderPanel::SetVertexAnimationList( CDmeAnimationList *pAnimationList )
{
	m_hVertexAnimationList = pAnimationList;
	int nCount = pAnimationList ? pAnimationList->GetAnimationCount() : 0;
	if ( nCount == 0 )
	{
		m_hCurrentVertexAnimation = NULL;
		return;
	}

	SelectVertexAnimation( 0 );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDagRenderPanel::UpdateMenu()
{
	switch ( m_pDrawSettings->GetDrawType() )
	{
	case CDmeDrawSettings::DRAW_FLAT:
		m_pShadingMenu->SetMenuItemChecked( m_nMenuSmoothShade, false );
		m_pShadingMenu->SetMenuItemChecked( m_nMenuFlatShade, true );
		m_pShadingMenu->SetMenuItemChecked( m_nMenuWireframe, false );
		m_pShadingMenu->SetMenuItemChecked( m_nMenuBoundingBox, false );
		break;
	case CDmeDrawSettings::DRAW_WIREFRAME:
		m_pShadingMenu->SetMenuItemChecked( m_nMenuSmoothShade, false );
		m_pShadingMenu->SetMenuItemChecked( m_nMenuFlatShade, false );
		m_pShadingMenu->SetMenuItemChecked( m_nMenuWireframe, true );
		m_pShadingMenu->SetMenuItemChecked( m_nMenuBoundingBox, false );
		break;
	case CDmeDrawSettings::DRAW_BOUNDINGBOX:
		m_pShadingMenu->SetMenuItemChecked( m_nMenuSmoothShade, false );
		m_pShadingMenu->SetMenuItemChecked( m_nMenuFlatShade, false );
		m_pShadingMenu->SetMenuItemChecked( m_nMenuWireframe, false );
		m_pShadingMenu->SetMenuItemChecked( m_nMenuBoundingBox, true );
		break;
	default:
		m_pShadingMenu->SetMenuItemChecked( m_nMenuSmoothShade, true );
		m_pShadingMenu->SetMenuItemChecked( m_nMenuFlatShade, false );
		m_pShadingMenu->SetMenuItemChecked( m_nMenuWireframe, false );
		m_pShadingMenu->SetMenuItemChecked( m_nMenuBoundingBox, false );
		break;
	}

	m_pShadingMenu->SetMenuItemChecked( m_nMenuNormals, m_pDrawSettings->GetNormals() );
	m_pShadingMenu->SetMenuItemChecked( m_nMenuWireframeOnShaded, m_pDrawSettings->GetWireframeOnShaded() );
	m_pShadingMenu->SetMenuItemChecked( m_nMenuBackfaceCulling, m_pDrawSettings->GetBackfaceCulling() );
	m_pShadingMenu->SetMenuItemChecked( m_nMenuXRay, m_pDrawSettings->GetXRay() );
	m_pShadingMenu->SetMenuItemChecked( m_nMenuGrayShade, m_pDrawSettings->GetGrayShade() );
}
