//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <mxtk/mx.h>
#include <mxtk/mxMessageBox.h>
#include <mxtk/mxTga.h>
#include <mxtk/mxPcx.h>
#include <mxtk/mxBmp.h>
#include <mxtk/mxMatSysWindow.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "MatSysWin.h"
#include "MDLViewer.h"
#include "StudioModel.h"
#include "ControlPanel.h"
#include "ViewerSettings.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "filesystem.h"
#include <keyvalues.h>
#include "materialsystem/imesh.h"
#include "expressions.h"
#include "hlfaceposer.h"
#include "ifaceposersound.h"
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "materialsystem/itexture.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "istudiorender.h"
#include "choreowidgetdrawhelper.h"
#include "faceposer_models.h"
#include "tier0/icommandline.h"
#include "mathlib/vmatrix.h"
#include "vstdlib/cvar.h"

IFileSystem *filesystem = NULL;

extern char g_appTitle[];

// FIXME: move all this to mxMatSysWin

class DummyMaterialProxyFactory : public IMaterialProxyFactory
{
public:
	virtual IMaterialProxy *CreateProxy( const char *proxyName )	{return NULL;}
	virtual void DeleteProxy( IMaterialProxy *pProxy )				{}
};
DummyMaterialProxyFactory	g_DummyMaterialProxyFactory;


static void ReleaseMaterialSystemObjects()
{
	StudioModel::ReleaseStudioModel();
	models->ReleaseModels();
}

static void RestoreMaterialSystemObjects( int nChangeFlags )
{
	StudioModel::RestoreStudioModel();
	models->RestoreModels();
}

void InitMaterialSystemConfig(MaterialSystem_Config_t *pConfig)
{
	pConfig->SetFlag( MATSYS_VIDCFG_FLAGS_USING_MULTIPLE_WINDOWS, true );
}

Vector g_vright( 50, 50, 0 );		// needs to be set to viewer's right in order for chrome to work

IMaterial *g_materialBackground = NULL;
IMaterial *g_materialWireframe = NULL;
IMaterial *g_materialWireframeVertexColor = NULL;
IMaterial *g_materialWireframeVertexColorNoCull = NULL;
IMaterial *g_materialDebugCopyBaseTexture = NULL;
IMaterial *g_materialFlatshaded = NULL;
IMaterial *g_materialSmoothshaded = NULL;
IMaterial *g_materialBones = NULL;
IMaterial *g_materialLines = NULL;
IMaterial *g_materialFloor = NULL;
IMaterial *g_materialVertexColor = NULL;
IMaterial *g_materialShadow = NULL;

MatSysWindow		*g_pMatSysWindow = 0;

#define MATSYSWIN_NAME "3D View"

MatSysWindow::MatSysWindow (mxWindow *parent, int x, int y, int w, int h, const char *label, int style)
: IFacePoserToolWindow( "3D View", "3D View" ), mxMatSysWindow ( parent, x, y, w, h, label, style )
{
	g_pMaterialSystem->SetMaterialProxyFactory( &g_DummyMaterialProxyFactory );

	SetAutoProcess( true );

	setLabel( MATSYSWIN_NAME );

	m_bSuppressSwap = false;

	m_hWnd = (HWND)getHandle();

	Con_Printf( "Setting material system video mode\n" );

	MaterialSystem_Config_t config;
	config = g_pMaterialSystem->GetCurrentConfigForVideoCard();
	InitMaterialSystemConfig(&config);
	g_pMaterialSystem->OverrideConfig( config, false );
	
//	config.m_VideoMode.m_Width = config.m_VideoMode.m_Height = 0;
	config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, true );
	config.SetFlag( MATSYS_VIDCFG_FLAGS_RESIZING, true );

	if (!g_pMaterialSystem->SetMode( ( void * )m_hWnd, config ) )
	{
		return;
	}
	g_pMaterialSystem->AddReleaseFunc( ReleaseMaterialSystemObjects );
	g_pMaterialSystem->AddRestoreFunc( RestoreMaterialSystemObjects );

	Con_Printf( "Loading debug materials\n" );

	ITexture *pCubemapTexture = g_pMaterialSystem->FindTexture( "hlmv/cubemap", NULL, true );
	pCubemapTexture->IncrementReferenceCount();
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->BindLocalCubemap( pCubemapTexture );

	g_materialBackground	= g_pMaterialSystem->FindMaterial("particle/particleapp_background", TEXTURE_GROUP_OTHER, true);
	g_materialWireframe		= g_pMaterialSystem->FindMaterial("debug/debugmrmwireframe", TEXTURE_GROUP_OTHER, true);
	g_materialWireframeVertexColor = g_pMaterialSystem->FindMaterial("debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER, true);

	// test: create this from code - you need a vmt to make $nocull 1 happen, can't do it from the render context
	{
		KeyValues *pVMTKeyValues = new KeyValues( "Wireframe" );
		pVMTKeyValues->SetInt("$ignorez", 1);
		pVMTKeyValues->SetInt("$nocull", 1);
		pVMTKeyValues->SetInt("$vertexcolor", 1);
		pVMTKeyValues->SetInt("$decal", 1);
		g_materialWireframeVertexColorNoCull = g_pMaterialSystem->CreateMaterial( "debug/wireframenocull", pVMTKeyValues );
	}
	{
		KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
		pVMTKeyValues->SetString("$basetexture", "vgui/white" );
		g_materialDebugCopyBaseTexture = g_pMaterialSystem->CreateMaterial( "debug/copybasetexture", pVMTKeyValues );

	}

	g_materialFlatshaded	= g_pMaterialSystem->FindMaterial("debug/debugdrawflatpolygons", TEXTURE_GROUP_OTHER, true);
	g_materialSmoothshaded	= g_pMaterialSystem->FindMaterial("debug/debugmrmfullbright2", TEXTURE_GROUP_OTHER, true);
	g_materialBones			= g_pMaterialSystem->FindMaterial("debug/debugmrmwireframe", TEXTURE_GROUP_OTHER, true);
	g_materialLines			= g_pMaterialSystem->FindMaterial("debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER, true);
	g_materialFloor			= g_pMaterialSystem->FindMaterial("hlmv/floor", TEXTURE_GROUP_OTHER, true);
	g_materialVertexColor   = g_pMaterialSystem->FindMaterial("debug/debugvertexcolor", TEXTURE_GROUP_OTHER, true);
	g_materialShadow		= g_pMaterialSystem->FindMaterial("hlmv/shadow", TEXTURE_GROUP_OTHER, true);

	if (!parent)
		setVisible (true);
	else
		mx::setIdleWindow (this);

	m_bSuppressResize = false;

	m_stickyDepth = 0;
	m_bIsSticky = false;
	m_snapshotDepth = 0;
}



MatSysWindow::~MatSysWindow ()
{
	mx::setIdleWindow (0);
}

void MatSysWindow::redraw()
{
	BaseClass::redraw();
return;
	if ( IsLocked() )
	{
		RECT bounds;
		GetClientRect( (HWND)getHandle(), &bounds );
		bounds.bottom = bounds.top + GetCaptionHeight();
		CChoreoWidgetDrawHelper helper( this, bounds );
		HandleToolRedraw( helper );
	}
}

#define MAX_FPS 250.0f
#define MIN_TIMESTEP ( 1.0f / MAX_FPS )

double realtime = 0.0f;

void MatSysWindow::Frame( void )
{
	static bool recursion_guard = false;

	static double prev = 0.0;
	double curr = (double) mx::getTickCount () / 1000.0;
	double dt = ( curr - prev );
	
	if ( recursion_guard )
		return;

	recursion_guard = true;

	// clamp to MAX_FPS
	if ( dt >= 0.0 && dt < MIN_TIMESTEP )
	{
		Sleep( max( 0, (int)( ( MIN_TIMESTEP - dt ) * 1000.0f ) ) );

		recursion_guard = false;
		return;
	}

	if ( prev != 0.0 )
	{
		dt = min( 0.1, dt );
		
		g_MDLViewer->Think( dt );

		realtime += dt;
	}
	
	prev = curr;

	DrawFrame();

	recursion_guard = false;
}

void MatSysWindow::DrawFrame( void )
{
	if (!g_viewerSettings.pause)
	{
		redraw ();
	}
}

int MatSysWindow::handleEvent (mxEvent *event)
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	int iret = 0;
	
	if ( HandleToolEvent( event ) )
	{
		return iret;
	}

	static float oldrx = 0, oldry = 0, oldtz = 50, oldtx = 0, oldty = 0;
	static float oldlrx = 0, oldlry = 0;
	static int oldx, oldy;

	switch (event->event)
	{
	case mxEvent::Idle:
		{
			iret = 1;

			Frame();
		}
		break;

	case mxEvent::MouseDown:
		{
			StudioModel *pModel = models->GetActiveStudioModel();
			if (!pModel)
				break;
			oldrx = pModel->m_angles[0];
			oldry = pModel->m_angles[1];
			oldtx = pModel->m_origin[0];
			oldty = pModel->m_origin[1];
			oldtz = pModel->m_origin[2];
			oldx = (short)event->x;
			oldy = (short)event->y;
			oldlrx = g_viewerSettings.lightrot[0];
			oldlry = g_viewerSettings.lightrot[1];
			g_viewerSettings.pause = false;

			float r = 1.0/3.0 * min( w(), h() );

			float d = sqrt( ( float )( (event->x - w()/2) * (event->x - w()/2) + (event->y - h()/2) * (event->y - h()/2) ) );

			if (d < r)
				g_viewerSettings.rotating = false;
			else
				g_viewerSettings.rotating = true;

			iret = 1;
		}
		break;

	case mxEvent::MouseDrag:
		{
			StudioModel *pModel = models->GetActiveStudioModel();
			if (!pModel)
				break;

			if (event->buttons & mxEvent::MouseLeftButton)
			{
				if (event->modifiers & mxEvent::KeyShift)
				{
					pModel->m_origin[1] = oldty - (float) ((short)event->x - oldx) * 0.1;
					pModel->m_origin[2] = oldtz + (float) ((short)event->y - oldy) * 0.1;
				}
				else if (event->modifiers & mxEvent::KeyCtrl)
				{
					float ry = (float) (event->y - oldy);
					float rx = (float) (event->x - oldx);
					oldx = event->x;
					oldy = event->y;
					
					QAngle movement = QAngle( ry, rx, 0 );
					
					matrix3x4_t tmp1, tmp2, tmp3;
					AngleMatrix( g_viewerSettings.lightrot, tmp1 );
					AngleMatrix( movement, tmp2 );
					ConcatTransforms( tmp2, tmp1, tmp3 );
					MatrixAngles( tmp3, g_viewerSettings.lightrot );
				}
				else
				{
					if (!g_viewerSettings.rotating)
					{
						float ry = (float) (event->y - oldy);
						float rx = (float) (event->x - oldx);
						oldx = event->x;
						oldy = event->y;
						
						QAngle movement;
						matrix3x4_t tmp1, tmp2, tmp3;
						
						movement = QAngle( 0, rx, 0 );
						AngleMatrix( pModel->m_angles, tmp1 );
						AngleMatrix( movement, tmp2 );
						ConcatTransforms( tmp1, tmp2, tmp3 );
						MatrixAngles( tmp3, pModel->m_angles );
						
						movement = QAngle( ry, 0, 0 );
						AngleMatrix( pModel->m_angles, tmp1 );
						AngleMatrix( movement, tmp2 );
						ConcatTransforms( tmp2, tmp1, tmp3 );
						MatrixAngles( tmp3, pModel->m_angles );
					}
					else
					{
						float ang1 = (180 / 3.1415) * atan2( oldx - w()/2.0, oldy - h()/2.0 );
						float ang2 = (180 / 3.1415) * atan2( event->x - w()/2.0, event->y - h()/2.0 );
						oldx = event->x;
						oldy = event->y;
						
						QAngle movement = QAngle( 0, 0, ang2 - ang1 );
						
						matrix3x4_t tmp1, tmp2, tmp3;
						AngleMatrix( pModel->m_angles, tmp1 );
						AngleMatrix( movement, tmp2 );
						ConcatTransforms( tmp2, tmp1, tmp3 );
						MatrixAngles( tmp3, pModel->m_angles );
					}			
				}
			}
			else if (event->buttons & mxEvent::MouseRightButton)
			{
				pModel->m_origin[0] = oldtx + (float) ((short)event->y - oldy) * 0.1;
				pModel->m_origin[0] = clamp( pModel->m_origin[0], 8.0f, 1024.0f );
			}
			redraw ();

			iret = 1;
		}
		break;

	case mxEvent::KeyDown:
		{
			iret = 1;
			switch (event->key)
			{
			default:
				iret = 0;
				break;
			case 116: // F5
				{
					g_MDLViewer->Refresh();
				}
				break;
			case 32:
				{
					int iSeq = models->GetActiveStudioModel()->GetSequence();
					if (iSeq == models->GetActiveStudioModel()->SetSequence (iSeq + 1))
					{
						g_pControlPanel->setSequence( 0 );
					}
					else
					{
						g_pControlPanel->setSequence( iSeq + 1 );
					}
				}
				break;
				
			case 27:
				if (!getParent ()) // fullscreen mode ?
					mx::quit ();
				break;
				
			case 'g':
				g_viewerSettings.showGround = !g_viewerSettings.showGround;
				break;
				
			case 'h':
				g_viewerSettings.showHitBoxes = !g_viewerSettings.showHitBoxes;
				break;
				
			case 'o':
				g_viewerSettings.showBones = !g_viewerSettings.showBones;
				break;
				
			case 'b':
				g_viewerSettings.showBackground = !g_viewerSettings.showBackground;
				break;
				
			case 'm':
				g_viewerSettings.showMovement = !g_viewerSettings.showMovement;
				break;
				
			case '1':
			case '2':
			case '3':
			case '4':
				g_viewerSettings.renderMode = event->key - '1';
				break;
				
			case '-':
				g_viewerSettings.speedScale -= 0.1f;
				if (g_viewerSettings.speedScale < 0.0f)
					g_viewerSettings.speedScale = 0.0f;
				break;
				
			case '+':
				g_viewerSettings.speedScale += 0.1f;
				if (g_viewerSettings.speedScale > 5.0f)
					g_viewerSettings.speedScale = 5.0f;
				break;
			}
		}
		break;
	} // switch (event->event)

	return iret;
}



void
drawFloor ()
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind(g_materialFloor);
	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->MatrixMode(MATERIAL_VIEW);
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	{
		IMesh* pMesh = pRenderContext->GetDynamicMesh();
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		float dist=-15000.0f;
		float tMin=0, tMax=1;
		
		meshBuilder.Position3f(-dist, dist, dist);
		meshBuilder.TexCoord2f( 0, tMin,tMax );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( dist, dist, dist);
		meshBuilder.TexCoord2f( 0, tMax,tMax );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( dist,-dist, dist);
		meshBuilder.TexCoord2f( 0, tMax,tMin );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f(-dist,-dist, dist);
		meshBuilder.TexCoord2f( 0, tMin,tMin );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}
	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->PopMatrix();
	pRenderContext->MatrixMode(MATERIAL_VIEW);
	pRenderContext->PopMatrix();
}



void
setupRenderMode ()
{
}

void MatSysWindow::SuppressBufferSwap( bool bSuppress )
{
	m_bSuppressSwap = bSuppress;
}

void MatSysWindow::draw ()
{
	int i;

	g_pMaterialSystem->BeginFrame( 0 );
	CUtlVector< StudioModel * > modellist;

	modellist.AddToTail( models->GetActiveStudioModel() );

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

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->ClearBuffers(true, true);

	int captiony = GetCaptionHeight();
	int viewh = h2() - captiony;

	g_pMaterialSystem->SetView( (HWND)getHandle() );

	pRenderContext->Viewport( 0, captiony, w2(), viewh );

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadIdentity( );
	pRenderContext->PerspectiveX(20.0f, (float)w2() / (float)viewh, 1.0f, 20000.0f);
	
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity( );
	// FIXME: why is this needed?  Doesn't SetView() override this?
	pRenderContext->Rotate( -90,  1, 0, 0 );	    // put Z going up
	pRenderContext->Rotate( -90,  0, 0, 1 );

	int modelcount = modellist.Count();
	int countover2 = modelcount / 2;
	int ydelta = g_pControlPanel->GetModelGap();
	int yoffset = -countover2 * ydelta;
	for ( i = 0 ; i < modelcount; i++ )
	{
		modellist[ i ]->IncrementFramecounter( );

		Vector oldtrans = modellist[ i ]->m_origin;

		modellist[ i ]->m_origin[ 1 ] = oldtrans[ 1 ] + yoffset;
		yoffset += ydelta;

		modellist[ i ]->GetStudioRender()->BeginFrame();
		modellist[ i ]->DrawModel();
		modellist[ i ]->GetStudioRender()->EndFrame();

		modellist[ i ]->m_origin = oldtrans;
	}

	//
	// draw ground
	//
	if (g_viewerSettings.showGround)
	{
		drawFloor ();
	}

	if (!m_bSuppressSwap)
	{
		g_pMaterialSystem->SwapBuffers();
	}

	g_pMaterialSystem->EndFrame();
}

void MatSysWindow::EnableStickySnapshotMode( )
{
	m_stickyDepth++;
}

void MatSysWindow::DisableStickySnapshotMode( )
{
	if (--m_stickyDepth == 0)
	{
		if (m_bIsSticky)
		{
			m_bIsSticky = false;

			HWND wnd = (HWND)getHandle();

			// Move back to original position
			SetWindowPlacement( wnd, &m_wp );

			SuppressResize( false );

			SetCursor( m_hPrevCursor );
		}
	}
}


void MatSysWindow::PushSnapshotMode( int nSnapShotSize )
{
	if (m_snapshotDepth++ == 0)
	{
		if (m_stickyDepth)
		{
			if (m_bIsSticky)
				return;

			m_bIsSticky = true;
			m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_WAIT ) );
		}

		SuppressResize( true );

		RECT rcClient;
		HWND wnd = (HWND)getHandle();

		GetWindowPlacement( wnd, &m_wp );

		GetClientRect( wnd, &rcClient );

		MoveWindow( wnd, 0, 0, nSnapShotSize + 16, nSnapShotSize + 16, TRUE );
	}
}


void MatSysWindow::PopSnapshotMode( )
{
	if (--m_snapshotDepth == 0)
	{
		if (m_stickyDepth == 0)
		{
			HWND wnd = (HWND)getHandle();

			// Move back to original position
			SetWindowPlacement( wnd, &m_wp );

			SuppressResize( false );
		}
	}
}


void MatSysWindow::TakeSnapshotRect( const char *pFilename, int x, int y, int w, int h )
{
	int i;
	HANDLE hf;
	BITMAPFILEHEADER hdr;
	BITMAPINFOHEADER bi;
	DWORD dwTmp, imageSize;
	byte *hp, b, *pBlue, *pRed;

	w = ( w + 3 ) & ~3;

	imageSize = w * h * 3;
	// Create the file
	hf = CreateFile( pFilename, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if( hf == INVALID_HANDLE_VALUE )
	{
		return;
	}

	// file header
	hdr.bfType = 0x4d42;	// 'BM'
	hdr.bfSize = (DWORD) ( sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageSize );
	hdr.bfReserved1 = 0;
	hdr.bfReserved2 = 0;
	hdr.bfOffBits = (DWORD) ( sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) );

	if( !WriteFile( hf, (LPVOID) &hdr, sizeof(BITMAPFILEHEADER), (LPDWORD) &dwTmp, NULL ) )
		Error( "Couldn't write file header to snapshot.\n" );

	// bitmap header
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = w;
	bi.biHeight = h;
	bi.biPlanes = 1;
	bi.biBitCount = 24;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;	//vid.rowbytes * vid.height;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	if( !WriteFile( hf, (LPVOID) &bi, sizeof(BITMAPINFOHEADER), (LPDWORD) &dwTmp, NULL ) )
		Error( "Couldn't write bitmap header to snapshot.\n" );

	// bitmap bits
	hp = (byte *) malloc(imageSize);
	
	if (hp == NULL)
		Error( "Couldn't allocate bitmap header to snapshot.\n" );

	// Get Bits from the renderer
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->ReadPixels( x, y, w, h, hp, IMAGE_FORMAT_RGB888 );

	// Invert vertically for BMP format
	for (i = 0; i < h / 2; i++)
	{
		byte *top = hp + i * w * 3;
		byte *bottom = hp + (h - i - 1) * w * 3;
		for (int j = 0; j < w * 3; j++)
		{
			b = *top;
			*top = *bottom;
			*bottom = b;
			top++;
			bottom++;
		}
	}

	// Reverse Red and Blue
	pRed = hp;
	pBlue = pRed + 2;
	for(i = 0; i < w * h;i++)
	{
		b = *pRed;
		*pRed = *pBlue;
		*pBlue = b;
		pBlue += 3;
		pRed += 3;
	}

	if( !WriteFile( hf, (LPVOID)hp, imageSize, (LPDWORD) &dwTmp, NULL ) )
		Error( "Couldn't write bitmap data snapshot.\n" );

	free(hp);

	// clean up
	if( !CloseHandle( hf ) )
		Error( "Couldn't close file for snapshot.\n" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool MatSysWindow::IsSuppressingResize( void )
{
	return m_bSuppressResize;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : suppress - 
//-----------------------------------------------------------------------------
void MatSysWindow::SuppressResize( bool suppress )
{
	m_bSuppressResize = suppress;
}

void
MatSysWindow::TakeScreenShot (const char *filename)
{
	redraw ();
	int w = w2 ();
	int h = h2 ();

	mxImage *image = new mxImage ();
	if (image->create (w, h, 24))
	{
#if 0
		glReadBuffer (GL_FRONT);
		glReadPixels (0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, image->data);
#else
		HDC hdc = GetDC ((HWND) getHandle ());
		byte *data = (byte *) image->data;
		int i = 0;
		for (int y = 0; y < h; y++)
		{
			for (int x = 0; x < w; x++)
			{
				COLORREF cref = GetPixel (hdc, x, y);
				data[i++] = (byte) ((cref >> 0)& 0xff);
				data[i++] = (byte) ((cref >> 8) & 0xff);
				data[i++] = (byte) ((cref >> 16) & 0xff);
			}
		}
		ReleaseDC ((HWND) getHandle (), hdc);
#endif
		if (!mxTgaWrite (filename, image))
			mxMessageBox (this, "Error writing screenshot.", g_appTitle, MX_MB_OK | MX_MB_ERROR);

		delete image;
	}
}
