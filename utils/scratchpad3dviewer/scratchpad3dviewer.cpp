//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "d3dapp.h"
#include "d3dx8math.h"
#include "mathlib/mathlib.h"
#include "ScratchPad3D.h"
#include "tier1/strtools.h"


#define SPColorExpand( c ) (c).m_vColor.x, (c).m_vColor.y, (c).m_vColor.z, (c).m_flAlpha


class VertPosDiffuse
{
public:
	inline void		Init(Vector const &vPos, float r, float g, float b, float a)
	{
		m_Pos = vPos;
		SetDiffuse( r, g, b, a );
	}

	inline void		SetDiffuse( float r, float g, float b, float a )
	{
		m_Diffuse[0] = (unsigned char)(b * 255.9f);
		m_Diffuse[1] = (unsigned char)(g * 255.9f);
		m_Diffuse[2] = (unsigned char)(r * 255.9f);
		m_Diffuse[3] = (unsigned char)(a * 255.9f);
	}

	inline void		SetDiffuse( Vector const &vColor )
	{
		SetDiffuse( vColor.x, vColor.y, vColor.z, 1 );
	}

	inline void		SetTexCoords( const Vector2D &tCoords )
	{
		m_tCoords = tCoords;
	}

	static inline DWORD	GetFVF()	{return D3DFVF_DIFFUSE | D3DFVF_XYZ | D3DFVF_TEX1;}

	Vector			m_Pos;
	unsigned char	m_Diffuse[4];
	Vector2D		m_tCoords;
};

class PosController
{
public:
	Vector	m_vPos;
	QAngle	m_vAngles;
};

int g_nLines, g_nPolygons;

PosController	g_ViewController;
Vector			g_IdentityBasis[3] = {Vector(1,0,0), Vector(0,1,0), Vector(0,0,1)};
Vector			g_ViewerPos;
Vector			g_ViewerBasis[3];
VMatrix			g_mModelView;
CScratchPad3D	*g_pScratchPad = NULL;
FILETIME		g_LastWriteTime;
char			g_Filename[256];


// ------------------------------------------------------------------------------------------ //
// Helper functions.
// ------------------------------------------------------------------------------------------ //

inline float FClamp(float val, float min, float max)
{
	return (val < min) ? min : (val > max ? max : val);
}

inline float FMin(float val1, float val2)
{
	return (val1 < val2) ? val1 : val2;
}

inline float FMax(float val1, float val2)
{
	return (val1 > val2) ? val1 : val2;
}

inline float CosDegrees(float angle)
{
	return (float)cos(DEG2RAD(angle));
}

inline float SinDegrees(float angle)
{
	return (float)sin(DEG2RAD(angle));
}

inline float FRand(float a, float b)
{
	return a + (b - a) * ((float)rand() / VALVE_RAND_MAX);
}

void CheckResult( HRESULT hr )
{
	if ( FAILED( hr ) )
	{
		Assert( 0 );
	}
}

void DrawLine2(const Vector &vFrom, const Vector &vTo, float r1, float g1, float b1, float a1, float r2, float g2, float b2, float a2)
{	
	VertPosDiffuse verts[2];
	
	verts[0].Init( vFrom, r1, g1, b1, 1 );
	verts[1].Init( vTo,   r2, g2, b2, 1 );

	CheckResult ( g_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE ) );
	CheckResult( g_pDevice->SetTexture( 0, NULL ) );
	CheckResult( g_pDevice->DrawPrimitiveUP( D3DPT_LINELIST, 1, verts, sizeof(verts[0]) ) );

	++g_nLines;
}


void DrawLine(const Vector &vFrom, const Vector &vTo, float r, float g, float b, float a)
{
	DrawLine2(vFrom, vTo, r,g,b,a, r,g,b,a);
}


// zAngle's range is [-90,90].
// When zAngle is 0, the position is in the middle of the sphere (vertically).
// When zAngle is  90, the position is at the top of the sphere.
// When zAngle is -90, the position is at the bottom of the sphere.
Vector CalcSphereVecAngles(float xyAngle, float zAngle, float fRadius)
{
	Vector vec;

	vec.x = CosDegrees(xyAngle) * CosDegrees(zAngle);
	vec.y = SinDegrees(xyAngle) * CosDegrees(zAngle);
	vec.z = SinDegrees(zAngle);

	return vec * fRadius;
}


// Figure out the rotation to look from vEye to vDest.
void SetupLookAt( const Vector &vEye, const Vector &vDest, Vector basis[3] )
{
	basis[0] = (vDest - vEye);	// Forward.
	VectorNormalize( basis[0] );
	basis[2].Init(0.0f, 0.0f, 1.0f);		// Up.
	
	basis[1] = basis[2].Cross(basis[0]);	// Left.
	VectorNormalize( basis[1] );
	
	basis[2] = basis[0].Cross(basis[1]);	// Regenerate up.
	VectorNormalize( basis[2] );
}


D3DMATRIX* VEngineToTempD3DMatrix( VMatrix const &mat )
{
	static VMatrix ret;
	ret = mat.Transpose();
	return (D3DMATRIX*)&ret;
}


void UpdateView(float mouseDeltaX, float mouseDeltaY)
{
	VMatrix mRot;
	PosController *pController;


	pController = &g_ViewController;

	// WorldCraft-like interface..
	if( Sys_HasFocus() )
	{
		Vector vForward, vUp, vRight;
		AngleVectors( pController->m_vAngles, &vForward, &vRight, &vUp );

		static float fAngleScale = 0.4f;
		static float fDistScale = 0.5f;

		if( Sys_GetKeyState( APPKEY_LBUTTON ) )
		{
			if( Sys_GetKeyState( APPKEY_RBUTTON ) )
			{
				// Ok, move forward and backwards.
				pController->m_vPos += vForward * -mouseDeltaY * fDistScale;
				pController->m_vPos += vRight * mouseDeltaX * fDistScale;
			}
			else
			{
				pController->m_vAngles.y += -mouseDeltaX * fAngleScale;
				pController->m_vAngles.x += mouseDeltaY * fAngleScale;
			}
		}
		else if( Sys_GetKeyState( APPKEY_RBUTTON ) )
		{
			pController->m_vPos += vUp * -mouseDeltaY * fDistScale;
			pController->m_vPos += vRight * mouseDeltaX * fDistScale;
		}
	}

    // Set the projection matrix to 90 degrees.
	D3DXMATRIX matProj;
     D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/2, Sys_ScreenWidth() / (float)Sys_ScreenHeight(), 1.0f, 10000.0f );
    g_pDevice->SetTransform( D3DTS_PROJECTION, &matProj );


	// This matrix converts from D3D coordinates (X=right, Y=up, Z=forward)
	// to VEngine coordinates (X=forward, Y=left, Z=up).
	VMatrix mD3DToVEngine(
		0.0f,  0.0f,  1.0f, 0.0f,
		-1.0f, 0.0f,  0.0f,  0.0f,
		0.0f,  1.0f,  0.0f,  0.0f,
		0.0f,  0.0f,  0.0f,  1.0f);
	
	g_ViewerPos = pController->m_vPos;
	mRot = SetupMatrixAngles( pController->m_vAngles );

	g_mModelView = ~mD3DToVEngine * mRot.Transpose3x3() * SetupMatrixTranslation(-g_ViewerPos);

	CheckResult( g_pDevice->SetTransform( D3DTS_VIEW, VEngineToTempD3DMatrix(g_mModelView) ) );

	// World matrix is identity..
	VMatrix mIdentity = SetupMatrixIdentity();
	CheckResult( g_pDevice->SetTransform( D3DTS_WORLD, (D3DMATRIX*)&mIdentity ) );
}


// ------------------------------------------------------------------------------------------ //
// ScratchPad3D command implementation.
// ------------------------------------------------------------------------------------------ //

void CommandRender_Point( CScratchPad3D::CBaseCommand *pInCmd, IDirect3DDevice8 *pDevice )
{
	CScratchPad3D::CCommand_Point *pCmd = (CScratchPad3D::CCommand_Point*)pInCmd;
	
	g_pDevice->SetRenderState( D3DRS_POINTSIZE, *((DWORD*)&pCmd->m_flPointSize) );

	VertPosDiffuse vert;
	vert.Init( pCmd->m_Vert.m_vPos, SPColorExpand(pCmd->m_Vert.m_vColor) );

	g_pDevice->DrawPrimitiveUP( D3DPT_POINTLIST, 1, &vert, sizeof(vert) );
}


VertPosDiffuse g_LineBatchVerts[1024];
int g_nLineBatchVerts = 0;


void CommandRender_LinesStart( IDirect3DDevice8 *pDevice )
{
	// Set states for line drawing.
	CheckResult( g_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE ) );
	CheckResult( g_pDevice->SetTexture( 0, NULL ) );
}

void CommandRender_LinesStop( IDirect3DDevice8 *pDevice )
{
	CheckResult( g_pDevice->DrawPrimitiveUP( D3DPT_LINELIST, g_nLineBatchVerts / 2, g_LineBatchVerts, sizeof(g_LineBatchVerts[0]) ) );
	g_nLines += g_nLineBatchVerts / 2;
	g_nLineBatchVerts = 0;
}

void CommandRender_Line( CScratchPad3D::CBaseCommand *pInCmd, IDirect3DDevice8 *pDevice )
{
	CScratchPad3D::CCommand_Line *pCmd = (CScratchPad3D::CCommand_Line*)pInCmd;

	// Flush out the line cache?
	if ( g_nLineBatchVerts == sizeof( g_LineBatchVerts ) / sizeof( g_LineBatchVerts[0] ) )
	{
		CommandRender_LinesStop( pDevice );
	}

	g_LineBatchVerts[g_nLineBatchVerts].m_Pos = pCmd->m_Verts[0].m_vPos;
	g_LineBatchVerts[g_nLineBatchVerts].SetDiffuse( SPColorExpand( pCmd->m_Verts[0].m_vColor ) );
	++g_nLineBatchVerts;

	g_LineBatchVerts[g_nLineBatchVerts].m_Pos = pCmd->m_Verts[1].m_vPos;
	g_LineBatchVerts[g_nLineBatchVerts].SetDiffuse( SPColorExpand( pCmd->m_Verts[1].m_vColor ) );
	++g_nLineBatchVerts;
}


void CommandRender_Polygon( CScratchPad3D::CBaseCommand *pInCmd, IDirect3DDevice8 *pDevice )
{
	VertPosDiffuse verts[65];
	CScratchPad3D::CCommand_Polygon *pCmd = (CScratchPad3D::CCommand_Polygon*)pInCmd;

	int nVerts = min( 64, pCmd->m_Verts.Size() );
	for( int i=0; i < nVerts; i++ )
	{
		verts[i].m_Pos[0] = pCmd->m_Verts[i].m_vPos.x;
		verts[i].m_Pos[1] = pCmd->m_Verts[i].m_vPos.y;
		verts[i].m_Pos[2] = pCmd->m_Verts[i].m_vPos.z;
		verts[i].SetDiffuse( SPColorExpand( pCmd->m_Verts[i].m_vColor ) );
	}

	// Draw wireframe manually since D3D draws internal edges of the triangle fan.
	DWORD dwFillMode;
	g_pDevice->GetRenderState( D3DRS_FILLMODE, &dwFillMode );
	if( dwFillMode == D3DFILL_WIREFRAME )
	{
		if( nVerts >= 2 )
		{
			g_pDevice->DrawPrimitiveUP( D3DPT_LINESTRIP, nVerts-1, verts, sizeof(verts[0]) );
			
			verts[nVerts] = verts[0];
			g_pDevice->DrawPrimitiveUP( D3DPT_LINESTRIP, 1, &verts[nVerts-1], sizeof(verts[0]) );
		}
	}
	else
	{
		CheckResult( g_pDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, nVerts - 2, verts, sizeof(verts[0]) ) );
	}

	++g_nPolygons;
}

void CommandRender_Matrix( CScratchPad3D::CBaseCommand *pInCmd, IDirect3DDevice8 *pDevice )
{
	CScratchPad3D::CCommand_Matrix *pCmd = (CScratchPad3D::CCommand_Matrix*)pInCmd;

	VMatrix mTransposed = pCmd->m_mMatrix.Transpose();
	CheckResult( g_pDevice->SetTransform( D3DTS_WORLD, (D3DMATRIX*)mTransposed.m ) );
}

void CommandRender_RenderState( CScratchPad3D::CBaseCommand *pInCmd, IDirect3DDevice8 *pDevice )
{
	CScratchPad3D::CCommand_RenderState *pCmd = (CScratchPad3D::CCommand_RenderState*)pInCmd;

	switch( pCmd->m_State )
	{
		case IScratchPad3D::RS_FillMode:
		{
			if( pCmd->m_Val == IScratchPad3D::FillMode_Wireframe )
				g_pDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );
			else
				g_pDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
		}
		break;

		case IScratchPad3D::RS_ZRead:
		{
			g_pDevice->SetRenderState( D3DRS_ZENABLE, pCmd->m_Val );
		}
		break;

		case IScratchPad3D::RS_ZBias:
		{
			g_pDevice->SetRenderState( D3DRS_ZBIAS, pCmd->m_Val );
		}
		break;
	}
}

class CCachedTextData : public CScratchPad3D::ICachedRenderData
{
public:
	CCachedTextData()
	{
		m_pTexture = NULL;
	}

	~CCachedTextData()
	{
		if ( m_pTexture )
			m_pTexture->Release();
	}

	virtual void Release()
	{
		delete this;
	}

	IDirect3DTexture8 *m_pTexture;
	int m_BitmapWidth;
	int m_BitmapHeight;
	int m_nChars;
};


void GenerateTextGreyscaleBitmap( 
	const char *pText, 
	CUtlVector<unsigned char> &bitmap,
	int *pWidth,
	int *pHeight )
{
	*pWidth = *pHeight = 0;

	
	// Create a bitmap, font, and HDC.
	HDC hDC = CreateCompatibleDC( NULL );
	Assert( hDC );

	HFONT hFont = ::CreateFontA(
		18,		// font height
		0, 0, 0, 
		FW_MEDIUM, 
		false, 
		false, 
		false, 
		ANSI_CHARSET, 
		OUT_DEFAULT_PRECIS, 
		CLIP_DEFAULT_PRECIS, 
		ANTIALIASED_QUALITY, 
		DEFAULT_PITCH | FF_DONTCARE, 
		"Arial" );
	Assert( hDC );
	if ( !hFont )
	{
		DeleteDC( hDC );
		return;
	}

	
	// Create a bitmap. Allow for width of 512. Hopefully, that can fit all the text we need.
	int bigImageWidth = 512;
	int bigImageHeight = 64;

	BITMAPINFOHEADER bmi;
	memset( &bmi, 0, sizeof( bmi ) );

	bmi.biSize         = sizeof(BITMAPINFOHEADER);
	bmi.biWidth        = bigImageWidth;
	bmi.biHeight       = -bigImageHeight;
	bmi.biBitCount     = 24;
	bmi.biPlanes       = 1;
	bmi.biCompression  = BI_RGB;
	
	void *pBits = NULL;
	HBITMAP hBitmap = CreateDIBSection( hDC, 
		(BITMAPINFO*)&bmi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0 );
	Assert( hBitmap && pBits );

	if ( !hBitmap )
	{
		DeleteObject( hFont );
		DeleteDC( hDC );
		return;
	}

	// Select the font and bitmap into the DC.
	HFONT hOldFont = (HFONT)SelectObject( hDC, hFont );
	HBITMAP hOldBitmap = (HBITMAP)SelectObject( hDC, hBitmap );


	// Draw the text into the DC.
	SIZE size;
	int textLen = strlen( pText );
	GetTextExtentPoint32( hDC, pText, textLen, &size );
	TextOut( hDC, 0, 0, pText, textLen );

	// Now copy the bits out.
	const unsigned char *pSrcBase = (const unsigned char*)pBits;
	*pWidth = size.cx;
	*pHeight = size.cy;
	bitmap.SetSize( size.cy * size.cx );
	for ( int y=0; y < size.cy; y++ )
	{
		for ( int x=0; x < size.cx; x++ )
		{
			const unsigned char *pSrc = &pSrcBase[ (y*bigImageWidth+x) * 3 ];
			unsigned char *pDest = &bitmap[y * size.cx + x];

			int avg = (pSrc[0] + pSrc[1] + pSrc[2]) / 3;
			*pDest = 0xFF - (unsigned char)avg;
		}
	}


	// Unselect the objects from the DC and cleanup everything.
	SelectObject( hDC, hOldFont );
	DeleteObject( hFont );

	SelectObject( hDC, hOldBitmap );
	DeleteObject( hBitmap );

	DeleteDC( hDC );
}


IDirect3DTexture8* MakeD3DTextureFromBitmap( 
	CUtlVector<unsigned char> &bitmap, 
	int width, 
	int height,
	bool bSolidBackground )
{
	IDirect3DTexture8 *pRet = NULL;
	HRESULT hr = g_pDevice->CreateTexture( 
		width,
		height,
		1,
		0,
		D3DFMT_A8R8G8B8,
		D3DPOOL_MANAGED,
		&pRet );

	if ( !pRet || FAILED( hr ) )
		return NULL;

	// Lock the texture and fill it up.
	D3DLOCKED_RECT lockedRect;
	hr = pRet->LockRect( 0, &lockedRect, NULL, 0 );
	if ( FAILED( hr ) )
	{
		Assert( false );
		pRet->Release();
		return NULL;
	}

	// Now fill it up.
	unsigned char *pDestData = (unsigned char*)lockedRect.pBits;
	for ( int y=0; y < height; y++ )
	{
		for ( int x=0; x < width; x++ )
		{
			unsigned char *pDestPixel = &pDestData[ (y*lockedRect.Pitch + x*4) ];
			unsigned char cSrcColor = bitmap[y*width+x];
			
			if ( bSolidBackground )
			{
				pDestPixel[3] = 0xFF;
				pDestPixel[0] = pDestPixel[1] = pDestPixel[2] = cSrcColor;
			}
			else
			{
				pDestPixel[0] = pDestPixel[1] = pDestPixel[2] = pDestPixel[3] = 0xFF;
				pDestPixel[3] = cSrcColor;
			}
		}
	}

	pRet->UnlockRect( 0 );
	return pRet;
}


void CommandRender_Text( CScratchPad3D::CBaseCommand *pInCmd, IDirect3DDevice8 *pDevice )
{
	CScratchPad3D::CCommand_Text *pCmd = (CScratchPad3D::CCommand_Text*)pInCmd;
	CTextParams *pParams = &pCmd->m_TextParams;

	// First, see if we've already generated the texture for this string.
	CCachedTextData *pCached = (CCachedTextData*)pCmd->m_pCachedRenderData;
	if ( !pCached )
	{
		// Generate a bitmap for this text string.
		CUtlVector<unsigned char> bitmap;
		int width, height;
		GenerateTextGreyscaleBitmap( pCmd->m_String.Base(), bitmap, &width, &height );

		// Convert the bitmap into a D3D texture.
		pCached = new CCachedTextData;
		pCached->m_pTexture = MakeD3DTextureFromBitmap( bitmap, width, height, pParams->m_bSolidBackground );
		pCached->m_BitmapWidth = width;
		pCached->m_BitmapHeight = height;
		pCached->m_nChars = strlen( pCmd->m_String.Base() );

		// Cache it.
		pCmd->m_pCachedRenderData = pCached;
	}


	// Figure out its orientation vectors.
	Vector vForward, vRight, vUp;
	AngleVectors( pParams->m_vAngles, &vForward, &vRight, &vUp );

	// Backface removal?
	bool bFlip = true;
	if ( vForward.Dot( g_ViewerPos - pParams->m_vPos ) < 0 )
	{
		if ( pParams->m_bTwoSided )
			bFlip = false;
		else
			return;
	}

	// This is really kludgy, but it's the best info we have here.
	float flTotalWidth = pParams->m_flLetterWidth * pCached->m_nChars;

	float flAvgCharWidth = (float)flTotalWidth / pCached->m_nChars;
	float flTotalHeight = flAvgCharWidth * 3;
	
	Vector vShift( 0, 0, 0 );
	if ( pParams->m_bCentered )
		vShift = vRight * ( -flTotalWidth/2 ) + vUp * ( flTotalHeight/2 );

	// Now draw the quad with the texture in it.
	VertPosDiffuse quad[5]; // Leave space for 1 more for the line strip for the border.

	quad[0].m_Pos = pParams->m_vPos;
	quad[1].m_Pos = pParams->m_vPos + vRight * flTotalWidth;
	quad[2].m_Pos = quad[1].m_Pos - vUp * flTotalHeight;
	quad[3].m_Pos = pParams->m_vPos - vUp * flTotalHeight;
	
	// Set tex coords.
	if ( bFlip )
	{
		quad[0].m_tCoords.Init( 1, 0 );
		quad[1].m_tCoords.Init( 0, 0 );
		quad[2].m_tCoords.Init( 0, 1 );
		quad[3].m_tCoords.Init( 1, 1 );
	}
	else
	{
		quad[0].m_tCoords.Init( 0, 0 );
		quad[1].m_tCoords.Init( 1, 0 );
		quad[2].m_tCoords.Init( 1, 1 );
		quad[3].m_tCoords.Init( 0, 1 );
	}

	for ( int i=0; i < 4; i++ )
	{
		quad[i].m_Pos += vShift;
		quad[i].SetDiffuse( pParams->m_vColor.x, pParams->m_vColor.y, pParams->m_vColor.z, pParams->m_flAlpha );
	}


	// Draw.
	
	// Backup render states.
	DWORD tss[][3] = {
		{ D3DTSS_COLOROP, D3DTOP_MODULATE, 0 },
		{ D3DTSS_COLORARG1, D3DTA_DIFFUSE, 0 },
		{ D3DTSS_COLORARG2, D3DTA_TEXTURE, 0 },
		{ D3DTSS_ALPHAOP, D3DTOP_MODULATE, 0 },
		{ D3DTSS_ALPHAARG1, D3DTA_DIFFUSE, 0 },
		{ D3DTSS_ALPHAARG2, D3DTA_TEXTURE, 0 } 
		};
	#define NUM_TSS ( sizeof( tss ) / sizeof( tss[0] ) )

	DWORD rss[][3] = {
		{ D3DRS_ALPHABLENDENABLE, TRUE, 0 },
		{ D3DRS_FILLMODE, D3DFILL_SOLID, 0 },
		{ D3DRS_SRCBLEND, D3DBLEND_SRCALPHA, 0 },
		{ D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA, 0 },
		{ D3DRS_FILLMODE, D3DFILL_SOLID, 0 }
		};
	#define NUM_RSS ( sizeof( rss ) / sizeof( rss[0] ) )
	
	for ( int i=0; i < NUM_TSS; i++ )
	{
		g_pDevice->GetTextureStageState( 0, (D3DTEXTURESTAGESTATETYPE)tss[i][0], &tss[i][2] );
		g_pDevice->SetTextureStageState( 0, (D3DTEXTURESTAGESTATETYPE)tss[i][0], tss[i][1] );
	}
	for ( int i=0; i < NUM_RSS; i++ )
	{
		g_pDevice->GetRenderState( (D3DRENDERSTATETYPE)rss[i][0], &rss[i][2] );
		g_pDevice->SetRenderState( (D3DRENDERSTATETYPE)rss[i][0], rss[i][1] );
	}


	g_pDevice->SetTexture( 0, pCached->m_pTexture );
	CheckResult( g_pDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, quad, sizeof(quad[0]) ) );
	g_pDevice->SetTexture( 0, NULL );

	++g_nPolygons;


	// Restore render states.
	for ( int i=0; i < NUM_TSS; i++ )
		g_pDevice->SetTextureStageState( 0, (D3DTEXTURESTAGESTATETYPE)tss[i][0], tss[i][2] );

	for ( int i=0; i < NUM_RSS; i++ )
		g_pDevice->SetRenderState( (D3DRENDERSTATETYPE)rss[i][0], rss[i][2] );


	// Draw wireframe outline..
	if ( pParams->m_bOutline )
	{
		DWORD fillMode;
		g_pDevice->GetRenderState( D3DRS_FILLMODE, &fillMode );
		g_pDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );
		
		quad[4] = quad[0];
		g_pDevice->DrawPrimitiveUP( D3DPT_LINESTRIP, 4, quad, sizeof(quad[0]) );
		
		g_pDevice->SetRenderState( D3DRS_FILLMODE, fillMode );
	}
}


typedef void (*CommandRenderFunction_Start)( IDirect3DDevice8 *pDevice );
typedef void (*CommandRenderFunction_Stop)( IDirect3DDevice8 *pDevice );
typedef void (*CommandRenderFunction)( CScratchPad3D::CBaseCommand *pInCmd, IDirect3DDevice8 *pDevice );

class CCommandRenderFunctions
{
public:
	CommandRenderFunction_Start m_StartFn;
	CommandRenderFunction_Start m_StopFn;
	CommandRenderFunction m_RenderFn;
};

CCommandRenderFunctions g_CommandRenderFunctions[CScratchPad3D::COMMAND_NUMCOMMANDS] =
{
	{ NULL, NULL, CommandRender_Point },
	{ CommandRender_LinesStart, CommandRender_LinesStop, CommandRender_Line },
	{ NULL, NULL, CommandRender_Polygon },
	{ NULL, NULL, CommandRender_Matrix },
	{ NULL, NULL, CommandRender_RenderState },
	{ NULL, NULL, CommandRender_Text }
};


void RunCommands( )
{
	// Set all the initial states.
	g_pDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
	g_pDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );

	VMatrix mIdentity = SetupMatrixIdentity();
	g_pDevice->SetTransform( D3DTS_WORLD, (D3DMATRIX*)&mIdentity );

	int iLastCmd = -1;
	for( int i=0; i < g_pScratchPad->m_Commands.Size(); i++ )
	{
		CScratchPad3D::CBaseCommand *pCmd = g_pScratchPad->m_Commands[i];

		if( pCmd->m_iCommand >= 0 && pCmd->m_iCommand < CScratchPad3D::COMMAND_NUMCOMMANDS )
		{
			// Call the start/stop handlers for this command type if they exist.
			// These can be used to batch primitives.
			if ( pCmd->m_iCommand != iLastCmd )
			{
				if ( iLastCmd != -1 )
				{
					if ( g_CommandRenderFunctions[iLastCmd].m_StopFn )
						g_CommandRenderFunctions[iLastCmd].m_StopFn( g_pDevice );
				}

				iLastCmd = pCmd->m_iCommand;

				if ( g_CommandRenderFunctions[pCmd->m_iCommand].m_StartFn )
					g_CommandRenderFunctions[pCmd->m_iCommand].m_StartFn( g_pDevice );
			}

			g_CommandRenderFunctions[pCmd->m_iCommand].m_RenderFn( pCmd, g_pDevice );
		}
	}

	// Call the final stop function.
	if ( iLastCmd != -1 )
	{
		if ( g_CommandRenderFunctions[iLastCmd].m_StopFn )
			g_CommandRenderFunctions[iLastCmd].m_StopFn( g_pDevice );
	}
}


bool CheckForNewFile( bool bForce )
{
	// See if the file has changed..
	HANDLE hFile = CreateFile( 
		g_pScratchPad->m_pFilename, 
		GENERIC_READ, 
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		0,
		NULL );

	if( !hFile )
		return false;

	FILETIME createTime, accessTime, writeTime;
	if( !GetFileTime( hFile, &createTime, &accessTime, &writeTime ) )
	{
		CloseHandle( hFile );
		return false;
	}

	bool bChange = false;
	if( memcmp(&writeTime, &g_LastWriteTime, sizeof(writeTime)) != 0 || bForce )
	{
		bChange = g_pScratchPad->LoadCommandsFromFile();
		if( bChange )
		{
			memcpy( &g_LastWriteTime, &writeTime, sizeof(writeTime) );
		}
	}

	CloseHandle( hFile );
	return bChange;
}


// ------------------------------------------------------------------------------------------ //
// App callbacks.
// ------------------------------------------------------------------------------------------ //

void UpdateWindowText()
{
	char str[512];
	sprintf( str, "ScratchPad3DViewer: <%s>  lines: %d, polygons: %d", g_Filename, g_nLines, g_nPolygons );
	Sys_SetWindowText( str );
}


void AppInit()
{
	// Viewer info.
	g_ViewController.m_vPos.Init( -200, 0, 0 );
	g_ViewController.m_vAngles.Init( 0, 0, 0 );

	char const *pFilename = Sys_FindArg( "-file", "scratch.pad" );
	Q_strncpy( g_Filename, pFilename, sizeof( g_Filename ) );

	IFileSystem *pFileSystem = ScratchPad3D_SetupFileSystem();
	if( !pFileSystem || pFileSystem->Init() != INIT_OK )
	{
		Sys_Quit();
	}

	// FIXME: I took this out of scratchpad 3d, not sure if this is even necessary any more
	pFileSystem->AddSearchPath( ".", "PLATFORM" );	

	g_pScratchPad = new CScratchPad3D( pFilename, pFileSystem, false );

	g_nLines = g_nPolygons = 0;
	UpdateWindowText();

	g_pDevice->SetRenderState( D3DRS_EDGEANTIALIAS, FALSE );
	g_pDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
	g_pDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
	g_pDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
	g_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
	g_pDevice->SetTexture( 0, NULL ); 

	// Setup point scaling parameters.
	float flOne=1;
	float flZero=0;
	g_pDevice->SetRenderState( D3DRS_POINTSCALEENABLE, TRUE );
	g_pDevice->SetRenderState( D3DRS_POINTSCALE_A, *((DWORD*)&flZero) );
	g_pDevice->SetRenderState( D3DRS_POINTSCALE_B, *((DWORD*)&flZero) );
	g_pDevice->SetRenderState( D3DRS_POINTSCALE_C, *((DWORD*)&flOne) );

	memset( &g_LastWriteTime, 0, sizeof(g_LastWriteTime) );
}


void AppRender( float frametime, float mouseDeltaX, float mouseDeltaY, bool bInvalidRect )
{
	g_nLines = 0;
	g_nPolygons = 0;

	g_pDevice->SetVertexShader( VertPosDiffuse::GetFVF() );
	
	if( !bInvalidRect && 
		!Sys_GetKeyState( APPKEY_LBUTTON ) && 
		!Sys_GetKeyState( APPKEY_RBUTTON ) && 
		!CheckForNewFile(false) )
	{
		Sys_Sleep( 100 );
		return;
	}

	g_pDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1, 0 );

	g_pDevice->BeginScene();

	UpdateView( mouseDeltaX, mouseDeltaY );
	
	RunCommands();

	g_pDevice->EndScene();

	g_pDevice->Present( NULL, NULL, NULL, NULL );

	UpdateWindowText();
}


void AppPreResize()
{
	for( int i=0; i < g_pScratchPad->m_Commands.Size(); i++ )
	{
		CScratchPad3D::CBaseCommand *pCmd = g_pScratchPad->m_Commands[i];

		if ( pCmd->m_iCommand == CScratchPad3D::COMMAND_TEXT )
		{
			// Delete the cached data if there is any.
			pCmd->ReleaseCachedRenderData();
		}
	}	
}

void AppPostResize()
{
}


void AppExit( )
{
}


void AppKey( int key, int down )
{
	if( key == 27 )
	{
		Sys_Quit();
	}
	else if( toupper(key) == 'U' )
	{
		CheckForNewFile( true );
		AppRender( 0.1f, 0, 0, true );
	}
}


void AppChar( int key )
{
}




