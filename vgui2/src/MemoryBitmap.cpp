//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#if !defined(_STATIC_LINKED) || defined(_VGUI_DLL)

#include <vgui/ISurface.h>

#include "Memorybitmap.h"
#include "vgui_internal.h"

#include <string.h>
#include <stdlib.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : *filename - image file to load 
//-----------------------------------------------------------------------------
MemoryBitmap::MemoryBitmap(unsigned char *texture,int wide, int tall)
{
	_texture=texture;
	_id = 0;
	_uploaded = false;
	_color = Color(255, 255, 255, 255);
	_pos[0] = _pos[1] = 0;
	_valid = true;
	_w = wide;
	_h = tall;
	ForceUpload(texture,wide,tall);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
MemoryBitmap::~MemoryBitmap()
{
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

	g_pSurface->DrawGetTextureSize(_id, wide, tall);
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

	// if we don't have an _id then lets make one
	if (!_id)
	{
		_id = g_pSurface->CreateNewTextureID( true );
	}
	
	// if we have not uploaded yet, lets go ahead and do so
	if (!_uploaded)
	{
		ForceUpload(_texture,_w,_h);
	}
	
	//set the texture current, set the color, and draw the biatch
	g_pSurface->DrawSetTexture(_id);
	g_pSurface->DrawSetColor(_color[0], _color[1], _color[2], _color[3]);

	int wide, tall;
	GetSize(wide, tall);
	g_pSurface->DrawTexturedRect(_pos[0], _pos[1], _pos[0] + wide, _pos[1] + tall);
}


//-----------------------------------------------------------------------------
// Purpose: ensures the bitmap has been uploaded
//-----------------------------------------------------------------------------
void MemoryBitmap::ForceUpload(unsigned char *texture,int wide, int tall)
{
	_texture=texture;
	_w = wide;
	_h = tall;

	if (!_valid)
		return;

//	if (_uploaded)
//		return;

	if(_w==0 || _h==0)
		return;
	
	if (!_id)
	{
		_id = g_pSurface->CreateNewTextureID( true );
	}
/*	drawSetTextureRGBA(IE->textureID,static_cast<const char *>(lpvBits), w, h);
*/
	g_pSurface->DrawSetTextureRGBA(_id, _texture, _w, _h, false, true);
	_uploaded = true;

	_valid = g_pSurface->IsTextureIDValid(_id);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
HTexture MemoryBitmap::GetID()
{
	return _id;
}

#endif // _STATIC_LINKED && _VGUI_DLL

