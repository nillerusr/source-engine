//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __UIAVATARIMAGE_H__
#define __UIAVATARIMAGE_H__

#include "vgui/IImage.h"

#ifdef _X360

typedef vgui::IImage CGameUiAvatarImage;

#else

#include "steam/steam_api.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CGameUiAvatarImage : public vgui::IImage
{
public:
	CGameUiAvatarImage( void );

	// Call this to set the steam ID associated with the avatar
	bool SetAvatarSteamID( CSteamID steamIDUser );
	void ClearAvatarSteamID( void );

	// Call to Paint the image
	// Image will draw within the current panel context at the specified position
	virtual void Paint( void );

	// Set the position of the image
	virtual void SetPos(int x, int y)
	{
		m_nX = x;
		m_nY = y;
	}

	// Gets the size of the content
	virtual void GetContentSize(int &wide, int &tall)
	{
		wide = m_nWide;
		tall = m_nTall;
	}

	// Get the size the image will actually draw in (usually defaults to the content size)
	virtual void GetSize(int &wide, int &tall)
	{
		GetContentSize( wide, tall );
	}

	// Sets the size of the image
	virtual void SetSize(int wide, int tall)	
	{ 
		m_nWide = wide; 
		m_nTall = tall; 
	}

	// Set the draw color 
	virtual void SetColor(Color col)			
	{ 
		m_Color = col; 
	}

	virtual void SetRotation( int )
	{
		( void ) 0;		// Not implemented
	}

	virtual bool Evict() { return false; }

	virtual int GetNumFrames() { return 0; }
	virtual void SetFrame( int nFrame ) {}

	virtual vgui::HTexture GetID() { return m_iTextureID; }

	bool	IsValid( void ) { return m_bValid; }

	float	GetFetchedTime() const { return m_flFetchedTime; }

protected:
	void InitFromRGBA( const byte *rgba, int width, int height );

private:
	Color m_Color;
	int m_iTextureID;
	int m_nX, m_nY, m_nWide, m_nTall;
	bool m_bValid;
	float m_flFetchedTime;
};

#endif // !_X360



#endif // __UIAVATARIMAGE_H__
