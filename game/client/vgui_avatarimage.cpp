//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include "vgui_avatarimage.h"
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif
#include "steam/steam_api.h"
#include "hud.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


DECLARE_BUILD_FACTORY( CAvatarImagePanel );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAvatarImage::CAvatarImage( void )
{
	m_iTextureID = -1;
	ClearAvatarSteamID();
	m_SourceArtSize = k_EAvatarSize32x32;
	m_pFriendIcon = NULL;
	m_nX = 0;
	m_nY = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAvatarImage::ClearAvatarSteamID( void ) 
{ 
	m_bValid = false; 
	m_bFriend = false;
	m_SteamID.Set( 0, k_EUniverseInvalid, k_EAccountTypeInvalid );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAvatarImage::SetAvatarSteamID( CSteamID steamIDUser )
{
	if ( m_steamIDUser == steamIDUser && m_bValid )
		return true;

	ClearAvatarSteamID();
	m_steamIDUser = steamIDUser;

	if ( steamapicontext->SteamFriends() && steamapicontext->SteamUtils() )
	{
		m_SteamID = steamIDUser;

		int iAvatar = steamapicontext->SteamFriends()->GetFriendAvatar( steamIDUser, m_SourceArtSize );

		/*
		// See if it's in our list already
		*/

		uint32 wide, tall;
		if ( steamapicontext->SteamUtils()->GetImageSize( iAvatar, &wide, &tall ) )
		{
			int cubImage = wide * tall * 4;
			byte *rgubDest = (byte*)_alloca( cubImage );
			steamapicontext->SteamUtils()->GetImageRGBA( iAvatar, rgubDest, cubImage );
			InitFromRGBA( rgubDest, wide, tall );

			/*
			// put it in the list
			RGBAImage *pRGBAImage = new RGBAImage( rgubDest, wide, tall );
			int iImageList = m_pImageList->AddImage( pRGBAImage );
			m_mapAvatarToIImageList.Insert( iAvatar, iImageList );
			*/
		}

		UpdateFriendStatus();
	}

	return m_bValid;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAvatarImage::UpdateFriendStatus( void )
{
	if ( !m_SteamID.IsValid() )
		return;

	if ( steamapicontext->SteamFriends() && steamapicontext->SteamUtils() )
	{
		m_bFriend = steamapicontext->SteamFriends()->HasFriend( m_SteamID, k_EFriendFlagImmediate );
		if ( m_bFriend && !m_pFriendIcon )
		{
			m_pFriendIcon = HudIcons().GetIcon( "ico_friend_indicator_avatar" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAvatarImage::InitFromRGBA( const byte *rgba, int width, int height )
{
	if ( m_iTextureID == -1 )
	{
		m_iTextureID = vgui::surface()->CreateNewTextureID( true );
	}

	vgui::surface()->DrawSetTextureRGBA( m_iTextureID, rgba, width, height );
	m_nWide = XRES(width);
	m_nTall = YRES(height);
	m_Color = Color( 255, 255, 255, 255 );

	m_bValid = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAvatarImage::Paint( void )
{
	if ( m_bValid )
	{
		if ( m_bFriend && m_pFriendIcon )
		{
			m_pFriendIcon->DrawSelf( m_nX, m_nY, m_nWide, m_nTall, m_Color );
		}

		vgui::surface()->DrawSetColor( m_Color );
		vgui::surface()->DrawSetTexture( m_iTextureID );
		vgui::surface()->DrawTexturedRect( m_nX + AVATAR_INDENT_X, m_nY + AVATAR_INDENT_Y, m_nX + AVATAR_INDENT_X + m_iAvatarWidth, m_nY + AVATAR_INDENT_Y + m_iAvatarHeight );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAvatarImagePanel::CAvatarImagePanel( vgui::Panel *parent, const char *name ) : vgui::ImagePanel( parent, name )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAvatarImagePanel::SetPlayer( C_BasePlayer *pPlayer )
{
	if ( pPlayer )
	{
		int iIndex = pPlayer->entindex();
		SetPlayerByIndex( iIndex );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAvatarImagePanel::SetPlayerByIndex( int iIndex )
{
	if ( iIndex && steamapicontext->SteamUtils() )
	{
		player_info_t pi;
		if ( engine->GetPlayerInfo(iIndex, &pi) )
		{
			if ( pi.friendsID )
			{
				CSteamID steamIDForPlayer( pi.friendsID, 1, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
				SetAvatarBySteamID( &steamIDForPlayer );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAvatarImagePanel::PaintBackground( void )
{
	vgui::IImage *pImage = GetImage();
	if ( pImage )
	{
		pImage->SetColor( GetDrawColor() );
		pImage->Paint();
	}
}

void CAvatarImagePanel::SetAvatarBySteamID( CSteamID *friendsID )
{
	if ( !GetImage() )
	{
		CAvatarImage *pImage = new CAvatarImage();
		SetImage( pImage );
	}

	// Indent the image. These are deliberately non-resolution-scaling.
	int iIndent = 2;
	GetImage()->SetPos( iIndent, iIndent );
	int wide = GetWide() - (iIndent*2);

	((CAvatarImage*)GetImage())->SetAvatarSize( ( wide > 32 ) ? k_EAvatarSize64x64 : k_EAvatarSize32x32 );
	((CAvatarImage*)GetImage())->SetAvatarSteamID( *friendsID );

	GetImage()->SetSize( wide, GetTall()-(iIndent*2) );
}