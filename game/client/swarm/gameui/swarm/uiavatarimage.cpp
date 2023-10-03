//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "UIGameData.h"
#include "UIAvatarImage.h"
#include "EngineInterface.h"
#include "vgui/ISurface.h"

#ifndef _X360
#include "steam/steam_api.h"
#endif

#ifndef _X360

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGameUiAvatarImage::CGameUiAvatarImage( void )
{
	m_bValid = false;
	m_flFetchedTime = 0.0f;
	m_iTextureID = ( -1 );
}

void CGameUiAvatarImage::ClearAvatarSteamID( void )
{
	m_bValid = false;
	m_flFetchedTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CGameUiAvatarImage::SetAvatarSteamID( CSteamID steamIDUser )
{
	ClearAvatarSteamID();

	if ( steamapicontext->SteamFriends() && steamapicontext->SteamUtils() )
	{
		int iAvatar = steamapicontext->SteamFriends()->GetFriendAvatar( steamIDUser, k_EAvatarSize64x64 );

		/*
		// See if it's in our list already
		*/

		uint32 wide, tall;
		if ( steamapicontext->SteamUtils()->GetImageSize( iAvatar, &wide, &tall ) )
		{
			bool bUseSteamImage = true;
			if ( wide == 0 || tall == 0 )
			{
				// attempt to handle rare data integrity issue, avatar got lost
				bUseSteamImage = false;
				// mock up solid white as 64x64
				wide = tall = 64;
			}			

			int cubImage = wide * tall * 4;
			byte *rgubDest = (byte*)_alloca( cubImage );
			if ( bUseSteamImage )
			{
				steamapicontext->SteamUtils()->GetImageRGBA( iAvatar, rgubDest, cubImage );
			}
			else
			{
				// solid white, avoids any issue with where the alpha channel is
				memset( rgubDest, 0xFF, cubImage );
			}
			InitFromRGBA( rgubDest, wide, tall );

			m_flFetchedTime = Plat_FloatTime();
		}
	}

	return m_bValid;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUiAvatarImage::InitFromRGBA( const byte *rgba, int width, int height )
{
	if ( m_iTextureID == -1 )
	{
		m_iTextureID = vgui::surface()->CreateNewTextureID( true );
	}

	vgui::surface()->DrawSetTextureRGBA( m_iTextureID, rgba, width, height );

	int screenWide, screenTall;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	m_nWide = width * ( ( (float) screenWide ) / 640.0f );
	m_nTall = height * ( ( (float) screenTall ) / 480.0f );
	m_Color = Color( 255, 255, 255, 255 );

	m_bValid = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUiAvatarImage::Paint( void )
{
	if ( m_bValid )
	{
		vgui::surface()->DrawSetColor( m_Color );
		vgui::surface()->DrawSetTexture( m_iTextureID );
		vgui::surface()->DrawTexturedRect( m_nX, m_nY, m_nX + m_nWide, m_nY + m_nTall );
	}
}

#endif // !_X360
