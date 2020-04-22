//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <vgui/ISurface.h>
#include "bitmap.h"
#include "vgui_internal.h"
#include "filesystem.h"
#include "tier1/utlbuffer.h"
#include <tier0/dbg.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : *filename - image file to load 
//-----------------------------------------------------------------------------
Bitmap::Bitmap(const char *filename, bool hardwareFiltered)
{
	_filtered = hardwareFiltered;

	int size = strlen(filename) + 1;
	_filename = (char *)malloc( size );
	Assert( _filename );

	Q_snprintf( _filename, size, "%s", filename );

	_bProcedural = false;

	if ( Q_stristr( filename, ".pic" ) )
	{
		_bProcedural = true;
	}

	_id = 0;
	_uploaded = false;
	_color = Color(255, 255, 255, 255);
	_pos[0] = _pos[1] = 0;
	_valid = true;
	_wide = 0;
	_tall = 0;
	nFrameCache = 0;
	_rotation = 0;

	ForceUpload();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
Bitmap::~Bitmap()
{
	Evict();

	if ( _filename )
	{
		free( _filename );
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void Bitmap::GetSize(int &wide, int &tall)
{
	wide = 0;
	tall = 0;

	if ( !_valid )
		return;

	// if a size has not been set, get it from the texture
	if ( 0 == _wide && 0 ==_tall )
	{
		g_pSurface->DrawGetTextureSize(_id, _wide, _tall);

	}
	wide = _wide;
	tall = _tall;
}

//-----------------------------------------------------------------------------
// Purpose: size of the bitmap
//-----------------------------------------------------------------------------
void Bitmap::GetContentSize(int &wide, int &tall)
{
	GetSize(wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: ignored
//-----------------------------------------------------------------------------
void Bitmap::SetSize(int x, int y)
{
//	AssertMsg( _filtered, "Bitmap::SetSize called on non-hardware filtered texture.  Bitmap can't be scaled; you don't want to be calling this." );
	_wide = x;
	_tall = y;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void Bitmap::SetPos(int x, int y)
{
	_pos[0] = x;
	_pos[1] = y;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void Bitmap::SetColor(Color col)
{
	_color = col;
}

//-----------------------------------------------------------------------------
// Purpose: returns the file name of the bitmap
//-----------------------------------------------------------------------------
const char *Bitmap::GetName()
{
	return _filename;
}

//-----------------------------------------------------------------------------
// Purpose: Renders the loaded image, uploading it if necessary
//			Assumes a valid image is always returned from uploading
//-----------------------------------------------------------------------------
void Bitmap::Paint()
{
	if ( !_valid )
		return;

	// if we don't have an _id then lets make one
	if ( !_id )
	{
		_id = g_pSurface->CreateNewTextureID();
	}
	
	// if we have not uploaded yet, lets go ahead and do so
	if ( !_uploaded )
	{
		ForceUpload();
	}
	
	// set the texture current, set the color, and draw the biatch
	g_pSurface->DrawSetColor( _color[0], _color[1], _color[2], _color[3] );
	g_pSurface->DrawSetTexture( _id );

	if ( _wide == 0 )
	{
		GetSize( _wide, _tall);
	}

	if ( _rotation == ROTATED_UNROTATED )
	{
		g_pSurface->DrawTexturedRect(_pos[0], _pos[1], _pos[0] + _wide, _pos[1] + _tall);
	}
	else
	{
		vgui::Vertex_t verts[4];
		verts[0].m_Position.Init( 0, 0 );
		verts[1].m_Position.Init( _wide, 0 );
		verts[2].m_Position.Init( _wide, _tall );
		verts[3].m_Position.Init( 0, _tall );
		
		switch ( _rotation )
		{
		case ROTATED_CLOCKWISE_90:
			verts[0].m_TexCoord.Init( 1, 0 );
			verts[1].m_TexCoord.Init( 1, 1 );
			verts[2].m_TexCoord.Init( 0, 1 );
			verts[3].m_TexCoord.Init( 0, 0 );
			break;

		case ROTATED_ANTICLOCKWISE_90:
			verts[0].m_TexCoord.Init( 0, 1 );
			verts[1].m_TexCoord.Init( 0, 0 );
			verts[2].m_TexCoord.Init( 1, 0 );
			verts[3].m_TexCoord.Init( 1, 1 );
			break;

		case ROTATED_FLIPPED:
			verts[0].m_TexCoord.Init( 1, 1 );
			verts[1].m_TexCoord.Init( 0, 1 );	
			verts[2].m_TexCoord.Init( 0, 0 );
			verts[3].m_TexCoord.Init( 1, 0 );
			break;

		default:
		case ROTATED_UNROTATED:
			break;
		}
		
		g_pSurface->DrawTexturedPolygon( 4, verts );
	}
}

//-----------------------------------------------------------------------------
// Purpose: ensures the bitmap has been uploaded
//-----------------------------------------------------------------------------
void Bitmap::ForceUpload()
{
	if ( !_valid || _uploaded )
		return;

	if ( !_id )
	{
		_id = g_pSurface->CreateNewTextureID( _bProcedural );
	}

	if ( !_bProcedural )
	{
		g_pSurface->DrawSetTextureFile( _id, _filename, _filtered, false );
	}

	_uploaded = true;
	_valid = g_pSurface->IsTextureIDValid( _id );
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
HTexture Bitmap::GetID()
{
	return _id;
}

bool Bitmap::Evict()
{
	if ( _id != 0 )
	{
		g_pSurface->DestroyTextureID( _id );
		// purposely not resetting _valid to match existing silly logic
		// either a Paint() or ForceUpload() will re-establish
		_id = 0;
		_uploaded = false;
		return true;
	}
	return false;
}

int Bitmap::GetNumFrames()
{
	if ( !_valid )
		return 0;

	return g_pSurface->GetTextureNumFrames( _id );
}

void Bitmap::SetFrame( int nFrame )
{
	if ( !_valid )
		return;

	// the frame cache is critical to cheapen the cost of this call
	g_pSurface->DrawSetTextureFrame( _id, nFrame, &nFrameCache );
}





