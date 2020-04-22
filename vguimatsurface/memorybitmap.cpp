//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <vgui/ISurface.h>
#include "memorybitmap.h"
#include <string.h>
#include <stdlib.h>
#include "MatSystemSurface.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "bitmap/imageformat.h"
#include "vtf/vtf.h"
#include "KeyValues.h"
#include "TextureDictionary.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern CMatSystemSurface g_MatSystemSurface;

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : *filename - image file to load 
//-----------------------------------------------------------------------------
MemoryBitmap::MemoryBitmap(unsigned char *texture,int wide, int tall)
{
	_texture=texture;
	_uploaded = false;
	_color = Color(255, 255, 255, 255);
	_pos[0] = _pos[1] = 0;
	_valid = true;
	_w = wide;
	_h = tall;
	m_iTextureID = -1;

	ForceUpload(texture,wide,tall);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
MemoryBitmap::~MemoryBitmap()
{
	// Free the old texture ID.
	if ( m_iTextureID != -1 )
	{
		TextureDictionary()->DestroyTexture( m_iTextureID );
		m_iTextureID = -1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void MemoryBitmap::GetSize(int &wide, int &tall)
{
	wide = 0;
	tall = 0;
	
	if (!_valid)
		return;

	g_MatSystemSurface.DrawGetTextureSize(m_iTextureID, wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: size of the bitmap
//-----------------------------------------------------------------------------
void MemoryBitmap::GetContentSize(int &wide, int &tall)
{
	GetSize(wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: ignored
//-----------------------------------------------------------------------------
void MemoryBitmap::SetSize(int x, int y)
{
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void MemoryBitmap::SetPos(int x, int y)
{
	_pos[0] = x;
	_pos[1] = y;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void MemoryBitmap::SetColor(Color col)
{
	_color = col;
}


//-----------------------------------------------------------------------------
// Purpose: returns the file name of the bitmap
//-----------------------------------------------------------------------------
const char *MemoryBitmap::GetName()
{
	return "MemoryBitmap";
}


//-----------------------------------------------------------------------------
// Purpose: Renders the loaded image, uploading it if necessary
//			Assumes a valid image is always returned from uploading
//-----------------------------------------------------------------------------
void MemoryBitmap::Paint()
{
	if (!_valid)
		return;
	
	// if we have not uploaded yet, lets go ahead and do so
	if (!_uploaded)
	{
		ForceUpload(_texture,_w,_h);
	}
	
	//set the texture current, set the color, and draw the biatch
	g_MatSystemSurface.DrawSetTexture(m_iTextureID);
	g_MatSystemSurface.DrawSetColor(_color[0], _color[1], _color[2], _color[3]);

	int wide, tall;
	GetSize(wide, tall);
	g_MatSystemSurface.DrawTexturedRect( _pos[0], _pos[1], _pos[0] + wide, _pos[1] + tall);
}


//-----------------------------------------------------------------------------
// Purpose: ensures the bitmap has been uploaded
//-----------------------------------------------------------------------------
void MemoryBitmap::ForceUpload(unsigned char *texture,int wide, int tall)
{
	_texture=texture;
	bool sizechanged = ( _w != wide || _h != tall );
	_w = wide;
	_h = tall;

	if (!_valid)
		return;

	if(_w==0 || _h==0)
		return;

	// Not our first time through and the size changed, destroy and recreate texture id...
	if ( m_iTextureID != -1 && sizechanged )
	{
		TextureDictionary()->DestroyTexture( m_iTextureID );
		m_iTextureID = -1;
	}

	if ( m_iTextureID == -1 )
	{
		m_iTextureID = g_MatSystemSurface.CreateNewTextureID( true );
	}

	g_MatSystemSurface.DrawSetTextureRGBA( m_iTextureID, texture, wide, tall, true, true );

	_uploaded = true;
	_valid = g_MatSystemSurface.IsTextureIDValid(m_iTextureID);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
HTexture MemoryBitmap::GetID()
{
	return m_iTextureID;
}


