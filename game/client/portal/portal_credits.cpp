//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include "hud_numericdisplay.h"
#include <vgui_controls/Panel.h>
#include "hud.h"
#include "hud_suitpower.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include <vgui_controls/AnimationController.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include "KeyValues.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct portalcreditname_t
{
	char szCreditName[256];
	char szFontName[256];
	wchar_t szLyricLine[256];
	float flYPos;
	float flXPos;
	bool bActive;
	float flTime;
	float flTimeAdd;
	float flTimeStart;
	float flTimeEnd;
	float flTimeInit;
	int iAsciiIndex;
	bool bReset;
	int iXOffset;
	int iSlot;
};

#define CREDITS_FILE "scripts/credits.txt"
#define CREDITS_FILE_PORTAL "scripts/credits.txt"

enum
{
	LOGO_FADEIN = 0,
	LOGO_FADEHOLD,
	LOGO_FADEOUT,
	LOGO_FADEOFF,
};

#define CREDITS_LOGO 1
#define CREDITS_INTRO 2
#define CREDITS_OUTRO 3
#define CREDITS_OUTRO_PORTAL 4

bool g_bPortalRollingCredits = false;

int g_iPortalCreditsPixelHeight = 0;

//-----------------------------------------------------------------------------
// Purpose: Shows the flashlight icon
//-----------------------------------------------------------------------------
class CHudPortalCredits : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudPortalCredits, vgui::Panel );

public:
	CHudPortalCredits( const char *pElementName );
	virtual void Init( void );
	virtual void LevelShutdown( void );

	int GetStringPixelWidth ( wchar_t *pString, vgui::HFont hFont );
	int GetPixelWidth( char *pString, vgui::HFont hFont );

	void MsgFunc_CreditsPortalMsg( bf_read &msg );
	void MsgFunc_LogoTimeMsg( bf_read &msg );

	virtual bool	ShouldDraw( void ) 
	{ 
		g_bPortalRollingCredits = IsActive();

		if ( g_bPortalRollingCredits && m_iCreditsType == CREDITS_INTRO )
			 g_bPortalRollingCredits = false;

		return IsActive(); 
	}

protected:
	virtual void Paint();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

private:

	void	Clear();

	void ReadNames( KeyValues *pKeyValue );
	void ReadLyrics( KeyValues *pKeyValue );
	void ReadAscii( KeyValues *pKeyValue );
	void ReadParams( KeyValues *pKeyValue );
	void PrepareCredits( const char *pKeyName );
	void DrawOutroCreditsName( void );
	void DrawPortalOutroCreditsName( void );
	void DrawPortalOutroCreditsLyrics( void );
	void DrawIntroCreditsName( void );
	void DrawPortalAsciiArt( void );
	void DrawLogo( void );

	void PrepareLogo( float flTime );
	void PrepareOutroCredits( void );
	void PreparePortalOutroCredits( void );
	void PrepareIntroCredits( void );

	float FadeBlend( float fadein, float fadeout, float hold, float localTime );

	CPanelAnimationVar( vgui::HFont, m_hTextFont, "TextFont", "Default" );
	CPanelAnimationVar( Color, m_TextColor, "TextColor", "FgColor" );

	CUtlVector<portalcreditname_t> m_CreditsList;

	CUtlVector<portalcreditname_t> m_LyricsList;

	CUtlVector<portalcreditname_t> m_AsciiList;

	char  m_szCreditPrint[256];
	char  m_szBorderH[512];

	float m_flScrollTime;
	float m_flSeparation;
	float m_flFadeTime;
	float m_flCursorBlinkTime;
	bool  m_bLastOneInPlace;
	int   m_Alpha;
	char  m_szAsciiArtFont[256];

	int   m_iCreditsType;
	int	  m_iLogoState;
	int   m_iCurrentAsciiArt;

	int   m_iCurrentLowY;
	int   m_iYOffset;
	int   m_iYOffsetNames;
	int   m_iYDrawOffset;
	int   m_iXOffset;
	int   m_iXOffsetNames;
	int   m_iLongestName;
	
	float m_flFadeInTime;
	float m_flFadeHoldTime;
	float m_flFadeOutTime;
	float m_flNextStartTime;
	float m_flPauseBetweenWaves;
	float m_flLastPaintTime;
	float m_flLastBlinkTime;
	float m_flCurrentDelayTime;
	float m_flNameCharTime;
	float m_flScrollCreditsStart;
	float m_flSongStartTime;


	//Screen adjustment properties 
	int m_iScreenXOffset;
	int m_iScreenYOffset;
	int m_iScreenWidthAdjustment;
	int m_iScreenHeightAdjustment;

	//Ascii Art positioning
	int m_iAASeparation;
	int m_iAAScreenXOffset;
	int m_iAAScreenYOffset;

	
	float m_flLogoTimeMod;
	float m_flLogoTime;
	float m_flLogoDesiredLength;

	float m_flX;
	float m_flY;

	bool m_bStartSong;
	float m_flLyricsStartTime;

	Color m_cColor;
};	

void CHudPortalCredits::PrepareCredits( const char *pKeyName )
{
	Clear();

	KeyValues *pKV= new KeyValues( "CreditsFile" );

	if (m_iCreditsType == CREDITS_OUTRO_PORTAL)
	{
		if ( !pKV->LoadFromFile( filesystem, CREDITS_FILE_PORTAL, "MOD" ) )
		{
			pKV->deleteThis();
	
			Assert( !"env_portal_credits couldn't be initialized!" );
			return;
		}
	}
	else
	{
		if ( !pKV->LoadFromFile( filesystem, CREDITS_FILE, "MOD" ) )
		{
			pKV->deleteThis();
	
			Assert( !"env_portal_credits couldn't be initialized!" );
			return;
		}
	}

	KeyValues *pKVSubkey = pKV->FindKey( pKeyName );

	ReadNames( pKVSubkey );

	pKVSubkey = pKV->FindKey( "CreditsParams" );

	ReadParams( pKVSubkey );

	pKVSubkey = pKV->FindKey( "OutroSongLyrics" );

	ReadLyrics( pKVSubkey );

	pKVSubkey = pKV->FindKey( "OutroAsciiArt" );

	ReadAscii( pKVSubkey );

	pKV->deleteThis();
}

using namespace vgui;

DECLARE_HUDELEMENT( CHudPortalCredits );
DECLARE_HUD_MESSAGE( CHudPortalCredits, CreditsPortalMsg );
DECLARE_HUD_MESSAGE( CHudPortalCredits, LogoTimeMsg );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudPortalCredits::CHudPortalCredits( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudCredits" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
}

void CHudPortalCredits::LevelShutdown()
{
	Clear();
}

void CHudPortalCredits::Clear( void )
{
	SetActive( false );
	m_CreditsList.RemoveAll();
	m_LyricsList.RemoveAll();
	m_bLastOneInPlace = false;
	m_Alpha = m_TextColor[3];
	m_iLogoState = LOGO_FADEOFF;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudPortalCredits::Init()
{
	HOOK_HUD_MESSAGE( CHudPortalCredits, CreditsPortalMsg );
	HOOK_HUD_MESSAGE( CHudPortalCredits, LogoTimeMsg );
	SetActive( false );
}

void CHudPortalCredits::ReadNames( KeyValues *pKeyValue )
{
	if ( pKeyValue == NULL )
	{
		Assert( !"CHudPortalCredits couldn't be initialized!" );
		return;
	}

	// Now try and parse out each act busy anim
	KeyValues *pKVNames = pKeyValue->GetFirstSubKey();
	
	char *cTmp;

	while ( pKVNames )
	{
		portalcreditname_t Credits;
		V_strcpy_safe( Credits.szCreditName, pKVNames->GetName() );
		V_strcpy_safe( Credits.szFontName, pKeyValue->GetString( Credits.szCreditName, "Default" ) );

		Credits.flTimeInit = 0.00;

		cTmp=Q_strstr(Credits.szCreditName,"[");
		if (!(cTmp == NULL)) {
			Credits.flTimeInit = Q_atof(cTmp+1);
		} 

		cTmp=Q_strstr(Credits.szCreditName,"]");
		if (!(cTmp == NULL)) {
			Q_strcpy(Credits.szCreditName,cTmp+1);
		}

		m_CreditsList.AddToTail( Credits );
		pKVNames = pKVNames->GetNextKey();
	}
}

void CHudPortalCredits::ReadLyrics( KeyValues *pKeyValue )
{
	if ( pKeyValue == NULL )
	{
		Assert( !"CHudPortalCredits couldn't be initialized!" );
		return;
	}

	KeyValues *pKVNames = pKeyValue->GetFirstSubKey();

	char *cTmp;

	int iHeight = 0;
	int iXOffsetTemp = 0;
	bool bNextOnSameLine = false;
	bool bNoY = false;

	char tmpstr[255] = "";

	float flTotalTime = 0.00;
	
	while ( pKVNames )
	{
		bNoY = false;
		portalcreditname_t Credits;
		vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
		vgui::HFont m_hTFont = vgui::scheme()->GetIScheme(scheme)->GetFont( "CreditsOutroText", true );
		V_strcpy_safe( Credits.szCreditName, pKVNames->GetName());
		V_strcpy_safe( Credits.szFontName, pKeyValue->GetString( Credits.szCreditName, "Default" ) );

		Credits.flTimeInit = 0.00;

		cTmp=Q_strstr(Credits.szCreditName,"[");
		if (!(cTmp == NULL)) {
			Credits.flTimeInit = Q_atof(cTmp+1);
		} 

		cTmp=Q_strstr(Credits.szCreditName,"]");
		if (!(cTmp == NULL)) {
			Q_strcpy(Credits.szCreditName,cTmp+1);
		}

		Credits.iAsciiIndex = 0;
		
		cTmp=Q_strstr(Credits.szCreditName,"<<<");
		if (!(cTmp == NULL)) {
			Credits.iAsciiIndex = Q_atoi(cTmp+3);
		} 

		cTmp=Q_strstr(Credits.szCreditName,">>>");
		if (!(cTmp == NULL)) {
			Q_strcpy(Credits.szCreditName,cTmp+3);
		}

		if (Q_strcmp("&",Credits.szCreditName)==0) //clear screen code
		{
			iHeight = 0;
			Credits.bReset = true;
			Q_strcpy(Credits.szCreditName," ");
		}
		else Credits.bReset = false;

		Credits.flYPos = iHeight;
		Credits.bActive = false;

		if (bNextOnSameLine)
		{
			Credits.iXOffset=iXOffsetTemp;
			//GetPixelWidth( tmpstr, m_hTFont )
			bNextOnSameLine = false;
		}
		else 
		{
			iXOffsetTemp=0;
			Credits.iXOffset = 0;
		}

		if (Credits.szCreditName[0] == '*')
		{
			bNextOnSameLine = true;
			Q_strcpy(tmpstr,&Credits.szCreditName[1]);
			Q_strcpy(Credits.szCreditName,tmpstr);
			bNoY = true;
		}

		if ((!(Q_strcmp(" ",Credits.szCreditName)==0)) && !bNoY) 
		{
			iHeight += surface()->GetFontTall ( m_hTFont ) + m_flSeparation;
			if (Q_strcmp("^",Credits.szCreditName)==0) 
			{
				Q_strcpy(Credits.szCreditName," ");
			}
		}
		
		if ( Credits.szCreditName[0] == '#' )
		{
			g_pVGuiLocalize->ConstructString( Credits.szLyricLine, sizeof(Credits.szLyricLine), g_pVGuiLocalize->Find(Credits.szCreditName), 0 );
			if (bNextOnSameLine)
			{
				iXOffsetTemp+=GetStringPixelWidth( Credits.szLyricLine, m_hTFont );
			}
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( Credits.szCreditName, Credits.szLyricLine, sizeof( Credits.szLyricLine ) );
			if (bNextOnSameLine)
			{
				iXOffsetTemp+=GetStringPixelWidth( Credits.szLyricLine, m_hTFont );
			}
		}

		Credits.flTimeStart = flTotalTime;
		flTotalTime+=Credits.flTimeInit;
		Credits.flTimeEnd = flTotalTime;


		m_LyricsList.AddToTail( Credits );
		pKVNames = pKVNames->GetNextKey();
	}

	g_iPortalCreditsPixelHeight = iHeight;

}


void CHudPortalCredits::ReadAscii( KeyValues *pKeyValue )
{
	if ( pKeyValue == NULL )
	{
		Assert( !"CHudPortalCredits couldn't be initialized!" );
		return;
	}

	KeyValues *pKVNames = pKeyValue->GetFirstSubKey();

	char *cTmp;
	
	while ( pKVNames )
	{
		portalcreditname_t Credits;
		//Q_strcpy( Credits.szCreditName, pKVNames->GetName());
		//Q_strcpy( Credits.szFontName, pKeyValue->GetString( Credits.szCreditName, "Default" ) );

		V_strcpy_safe( Credits.szFontName, pKVNames->GetName() );
		V_strcpy_safe( Credits.szCreditName, pKeyValue->GetString( Credits.szFontName, "Default" ) );

		Credits.flTimeInit = 0.00;

		cTmp=Q_strstr(Credits.szCreditName,"[");
		if (!(cTmp == NULL)) {
			Credits.flTimeInit = Q_atof(cTmp+1);
		} 

		cTmp=Q_strstr(Credits.szCreditName,"]");
		if (!(cTmp == NULL)) {
			Q_strcpy(Credits.szCreditName,cTmp+1);
		}

		m_AsciiList.AddToTail( Credits );
		pKVNames = pKVNames->GetNextKey();
	}
}

void CHudPortalCredits::ReadParams( KeyValues *pKeyValue )
{
		if ( pKeyValue == NULL )
	{
		Assert( !"CHudPortalCredits couldn't be initialized!" );
		return;
	}

	m_flScrollTime = pKeyValue->GetFloat( "scrolltime", 57 );
	m_flSeparation = pKeyValue->GetFloat( "separation", 5 );

	m_flFadeInTime = pKeyValue->GetFloat( "fadeintime", 1 );
	m_flFadeHoldTime = pKeyValue->GetFloat( "fadeholdtime", 3 );
	m_flFadeOutTime = pKeyValue->GetFloat( "fadeouttime", 2 );
	m_flNextStartTime = pKeyValue->GetFloat( "nextfadetime", 2 );
	m_flPauseBetweenWaves = pKeyValue->GetFloat( "pausebetweenwaves", 2 );

	m_flCursorBlinkTime = pKeyValue->GetFloat( "cursorblinktime", 0.3 );

	m_flLogoTimeMod = pKeyValue->GetFloat( "logotime", 2 );

	m_flScrollCreditsStart = pKeyValue->GetFloat( "scrollcreditsstart", 6 );
	m_flSongStartTime = pKeyValue->GetFloat( "songstarttime", 6.85 );

	m_iScreenXOffset = pKeyValue->GetInt( "screenxoffset", 0 );
	m_iScreenYOffset = pKeyValue->GetInt( "screenyoffset", 0 );
	m_iScreenWidthAdjustment = pKeyValue->GetInt( "screenwidthadjustment", 0 );
	m_iScreenHeightAdjustment = pKeyValue->GetInt( "screenheightadjustment", 0 );

	V_strcpy_safe( m_szAsciiArtFont, pKeyValue->GetString( "asciiartfont", "Default" ) );

	m_iAAScreenXOffset = pKeyValue->GetInt( "aascreenxoffset", 0 );
	m_iAAScreenYOffset = pKeyValue->GetInt( "aascreenyoffset", 0 );
	m_iAASeparation = pKeyValue->GetInt( "aaseparation", 0 );



	m_flX = pKeyValue->GetFloat( "posx", 2 );
	m_flY = pKeyValue->GetFloat( "posy", 2 );

	m_cColor = pKeyValue->GetColor( "color" );
}

int CHudPortalCredits::GetStringPixelWidth( wchar_t *pString, vgui::HFont hFont )
{
	int iLength = 0;

	for ( wchar_t *wch = pString; *wch != 0; wch++ )
	{
		iLength += surface()->GetCharacterWidth( hFont, *wch );
	}

	return iLength;
}

int CHudPortalCredits::GetPixelWidth( char *pString, vgui::HFont hFont )
{
	int iLength = 0;
	int iStrLen = Q_strlen(pString);

	for ( int i=0; i<iStrLen; i++ )
	{
		iLength += surface()->GetCharacterWidth( hFont, pString[i] );
	}

	return iLength;
}


void CHudPortalCredits::DrawOutroCreditsName( void )
{
	if ( m_CreditsList.Count() == 0 )
		 return;

	// fill the screen
	int iWidth, iTall;
	GetHudSize(iWidth, iTall);

	iWidth = static_cast<int>( static_cast<float>(iTall) * (4.0f/3.0f) );

	SetSize( iWidth, iTall );

	for ( int i = 0; i < m_CreditsList.Count(); i++ )
	{
		portalcreditname_t *pCredit = &m_CreditsList[i];

		if ( pCredit == NULL )
			 continue;

		vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
		vgui::HFont m_hTFont = vgui::scheme()->GetIScheme(scheme)->GetFont( pCredit->szFontName, true );

		int iFontTall = surface()->GetFontTall ( m_hTFont );

		if ( pCredit->flYPos < -iFontTall ||  pCredit->flYPos > iTall )
		{
			pCredit->bActive = false;
		}
		else
		{
			pCredit->bActive = true;
		}

		Color cColor = m_TextColor;

		//HACKHACK
		//Last one stays on screen and fades out
		if ( i == m_CreditsList.Count()-1 )
		{
			if ( m_bLastOneInPlace == false )
			{
				pCredit->flYPos -= gpGlobals->frametime * ( (float)g_iPortalCreditsPixelHeight / m_flScrollTime );
		
				if ( (int)pCredit->flYPos + ( iFontTall / 2 ) <= iTall / 2 )
				{
					m_bLastOneInPlace = true;
					m_flFadeTime = gpGlobals->curtime + 10.0f;
				}
			}
			else
			{
				if ( m_flFadeTime <= gpGlobals->curtime )
				{
					if ( m_Alpha > 0 )
					{
						m_Alpha -= gpGlobals->frametime * ( m_flScrollTime * 2 );

						if ( m_Alpha <= 0 )
						{
							pCredit->bActive = false;
							engine->ClientCmd( "creditsdone" );
						}
					}
				}

				cColor[3] = MAX( 0, m_Alpha );
			}
		}
		else
		{
			pCredit->flYPos -= gpGlobals->frametime * ( (float)g_iPortalCreditsPixelHeight / m_flScrollTime );

		}
		
		if ( pCredit->bActive == false )
			 continue;
			
		surface()->DrawSetTextFont( m_hTFont );
		surface()->DrawSetTextColor( cColor[0], cColor[1], cColor[2], cColor[3]  );
		
		wchar_t unicode[256];
		
		if ( pCredit->szCreditName[0] == '#' )
		{
			g_pVGuiLocalize->ConstructString( unicode, sizeof(unicode), g_pVGuiLocalize->Find(pCredit->szCreditName), 0 );
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( pCredit->szCreditName, unicode, sizeof( unicode ) );
		}

		int iStringWidth = GetStringPixelWidth( unicode, m_hTFont ); 

		surface()->DrawSetTextPos( ( iWidth / 2 ) - ( iStringWidth / 2 ), pCredit->flYPos );
		surface()->DrawUnicodeString( unicode );
	}
}


void CHudPortalCredits::DrawPortalOutroCreditsName( void )
{

	if ( m_CreditsList.Count() == 0 )
		 return;

	static int iCounter=0;
	static bool bCursor = false;

	// fill the screen
	int iWidth, iTall;
	GetHudSize(iWidth, iTall);

	iWidth = static_cast<int>( static_cast<float>(iTall) * (4.0f/3.0f) );

	iWidth += m_iScreenWidthAdjustment;
	iTall += m_iScreenHeightAdjustment;
	SetSize( iWidth+m_iScreenXOffset, iTall+m_iScreenYOffset );

	if (gpGlobals->curtime - m_flLastPaintTime > 0.05)
	{
		m_flLastPaintTime = gpGlobals->curtime;
		iCounter += 100;
		if (iCounter> (int)m_flScrollTime && m_iYOffset<(m_CreditsList.Count()-1))
		{
			bCursor = !bCursor;
			iCounter = 0;
			m_iXOffset += 1;
	
			if (m_iXOffset > Q_strlen(m_CreditsList[m_iYOffset].szCreditName) && m_iYOffset<(m_CreditsList.Count()-1))
			{
				m_iXOffset = 1;
				m_iYOffset += 1;
	
				vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
				vgui::HFont m_hTFont = vgui::scheme()->GetIScheme(scheme)->GetFont( m_CreditsList[m_iYOffset].szFontName, true );
				int iFontTall = surface()->GetFontTall ( m_hTFont )+ m_flSeparation;
	
				if ( m_CreditsList[m_iYOffset].flYPos +iFontTall > iTall )
				{
					for ( int i = 0; i < m_CreditsList.Count(); i++ )
					{
						portalcreditname_t *pCredit = &m_CreditsList[i];
						if ( pCredit == NULL )
							continue;
						vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
						vgui::HFont m_hTFont = vgui::scheme()->GetIScheme(scheme)->GetFont( pCredit->szFontName, true );
						int iFontTall = surface()->GetFontTall ( m_hTFont )+ m_flSeparation;
						pCredit->flYPos -= iFontTall;
					}
				}
			}
		}
	}

	for ( int i = 0; i < m_CreditsList.Count(); i++ )
	{
		portalcreditname_t *pCredit = &m_CreditsList[i];

		if ( pCredit == NULL )
			 continue;

		vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
		vgui::HFont m_hTFont = vgui::scheme()->GetIScheme(scheme)->GetFont( pCredit->szFontName, true );

		int iFontTall = surface()->GetFontTall ( m_hTFont );

		if ( pCredit->flYPos < -iFontTall ||  pCredit->flYPos > iTall )
		{
			pCredit->bActive = false;
		}
		else
		{
			if (i <= m_iYOffset)
				pCredit->bActive = true;
		}

		Color cColor = m_TextColor;

		//HACKHACK
		//Last one stays on screen and fades out
		if ( i == m_CreditsList.Count()-1 )
		{
			if ( m_bLastOneInPlace == false )
			{
				pCredit->flYPos -= gpGlobals->frametime * ( (float)g_iPortalCreditsPixelHeight / m_flScrollTime );
		
				if ( (int)pCredit->flYPos + ( iFontTall / 2 ) <= iTall / 2 )
				{
					m_bLastOneInPlace = true;
					m_flFadeTime = gpGlobals->curtime + 10.0f;
				}
			}
			else
			{
				if ( m_flFadeTime <= gpGlobals->curtime )
				{
					if ( m_Alpha > 0 )
					{
						m_Alpha -= gpGlobals->frametime * ( m_flScrollTime * 2 );

						if ( m_Alpha <= 0 )
						{
							pCredit->bActive = false;
							engine->ClientCmd( "creditsdone" );
						}
					}
				}

				cColor[3] = MAX( 0, m_Alpha );
			}
		}
		else
		{
			//m_iYOffset += (int) (gpGlobals->frametime * ( (float)g_iPortalCreditsPixelHeight / m_flScrollTime ));
			//m_iYOffset += (int) (gpGlobals->frametime * ( (float)g_iPortalCreditsPixelHeight / 1000 ));
			//iCounter += 1;
			//pCredit->flYPos -= gpGlobals->frametime * ( (float)g_iPortalCreditsPixelHeight / m_flScrollTime );
		}
		
		if ( pCredit->bActive == false )
			 continue;
			
		surface()->DrawSetTextFont( m_hTFont );
		surface()->DrawSetTextColor( cColor[0], cColor[1], cColor[2], cColor[3]  );
		
		wchar_t unicode[256];

		char tmpstr[256];
		if (m_iYOffset == i)
		{
			char tmpstr2[256];
			Q_strncpy( tmpstr2, pCredit->szCreditName, m_iXOffset );
			if (bCursor)
				Q_snprintf( tmpstr, 256, "%s_",tmpstr2);
			else
				Q_snprintf( tmpstr, 256, "%s",tmpstr2);
		}
		else
		{
			Q_strncpy( tmpstr, pCredit->szCreditName, Q_strlen( pCredit->szCreditName )+1 );
		}

		if ( tmpstr[0] == '#' )
		{
			g_pVGuiLocalize->ConstructString( unicode, sizeof(unicode), g_pVGuiLocalize->Find(tmpstr), 0 );
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( tmpstr, unicode, sizeof( unicode ) );
		}

		// int iStringWidth = GetStringPixelWidth( unicode, m_hTFont ); 
		// surface()->DrawSetTextPos( ( iWidth / 2 ) - ( iStringWidth / 2 ), pCredit->flYPos );
		surface()->DrawSetTextColor( m_cColor[0], m_cColor[1], m_cColor[2], 255 );
		surface()->DrawSetTextPos( 20+m_iScreenXOffset, pCredit->flYPos + m_iScreenYOffset );
		surface()->DrawUnicodeString( unicode );
	}
}

void CHudPortalCredits::DrawPortalAsciiArt( void )
{
	if ( m_AsciiList.Count() == 0 )
		return;

	if (m_iCurrentAsciiArt == 0)
		return;

	//get the screen stats
	int iWidth, iTall;
	GetHudSize(iWidth, iTall);

	iWidth = static_cast<int>( static_cast<float>(iTall) * (4.0f/3.0f) );

	iWidth += m_iScreenWidthAdjustment;
	iTall += m_iScreenHeightAdjustment;
	SetSize( iWidth+m_iScreenXOffset, iTall+m_iScreenYOffset );
	int ift = 0;

	for ( int i = 0; i < 20; i++ )
	{
		portalcreditname_t *pCredit = &m_AsciiList[((m_iCurrentAsciiArt - 1) *20)+i];

		if ( pCredit == NULL )
			 continue;

		vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
		//vgui::HFont m_hTFont = vgui::scheme()->GetIScheme(scheme)->GetFont( pCredit->szFontName, true );
		vgui::HFont m_hTFont = vgui::scheme()->GetIScheme(scheme)->GetFont( m_szAsciiArtFont, true );

		int iFontTall = surface()->GetFontTall ( m_hTFont );
		if (ift == 0) {
			ift = iFontTall;
			ift+=m_iAASeparation;
		}
		iFontTall+= (int) m_flSeparation;

		Color cColor = m_TextColor;

		surface()->DrawSetTextFont( m_hTFont );
		surface()->DrawSetTextColor( cColor[0], cColor[1], cColor[2], cColor[3]  );

		wchar_t unicode[256];
		char tmpstr[256];
		Q_strncpy( tmpstr, pCredit->szCreditName, Q_strlen( pCredit->szCreditName )+1 );

		//Q_strncpy( tmpstr, pCredit->szFontName, Q_strlen( pCredit->szFontName )+1 );

		//if ( tmpstr[0] == '#' )
		//{
		//	g_pVGuiLocalize->ConstructString( unicode, sizeof(unicode), g_pVGuiLocalize->Find(tmpstr), 0 );
		//}
		//else
		//{
			g_pVGuiLocalize->ConvertANSIToUnicode( tmpstr, unicode, sizeof( unicode ) );
		//}
		surface()->DrawSetTextColor( m_cColor[0], m_cColor[1], m_cColor[2], cColor[3] );
		//surface()->DrawSetTextPos( 20+(iWidth/2), 10+(iTall/2)+(i*iFontTall) );
		surface()->DrawSetTextPos( 20+(iWidth/2)+m_iAAScreenXOffset, 10+(iTall/2)+m_iAAScreenYOffset+(i*ift) );
		surface()->DrawUnicodeString( unicode );
	}

}


void CHudPortalCredits::DrawPortalOutroCreditsLyrics( void )
{
	if ( m_LyricsList.Count() == 0 )
		 return;

	static bool bCursor = false;
	static int iLastNameY = 0;
	
	if (m_flLyricsStartTime == -1) m_flLyricsStartTime = gpGlobals->curtime;

	//get the screen stats
	int iWidth, iTall;
	GetHudSize(iWidth, iTall);

	iWidth = static_cast<int>( static_cast<float>(iTall) * (4.0f/3.0f) );

	iWidth += m_iScreenWidthAdjustment;
	iTall += m_iScreenHeightAdjustment;
	SetSize( iWidth+m_iScreenXOffset, iTall+m_iScreenYOffset );

	if (gpGlobals->curtime - m_flLastBlinkTime > m_flCursorBlinkTime)
	{
		bCursor = !bCursor;
		m_flLastBlinkTime = gpGlobals->curtime;
	}


	float flCurTime = gpGlobals->curtime - m_flLyricsStartTime;

	if (m_bStartSong && flCurTime>= m_flSongStartTime)
	{
		surface()->PlaySound( "music/portal_still_alive.mp3" );
		m_bStartSong=false;
		//engine->ClientCmd( "play music/portal_still_alive.mp3" );
	}

	//Draw Lyrics
	bool bBorders = false;
	for ( int i = 0; i < m_LyricsList.Count(); i++ )
	{
		portalcreditname_t *pCredit = &m_LyricsList[i];

		if ( pCredit == NULL )
			 continue;

		if (pCredit->flTimeStart > flCurTime) break;
		if (pCredit->flTimeStart <= flCurTime && pCredit->flTimeEnd > flCurTime)
		{
			m_iYOffset = i;
			if (pCredit->bReset) 
			{
				m_iCurrentLowY = i;
			}
			m_iXOffset = (int) (((flCurTime-pCredit->flTimeStart) / pCredit->flTimeInit)*Q_wcslen(pCredit->szLyricLine))+1;
			if ( m_iXOffset<1 ) 
				m_iXOffset = 1;
			else if ( m_iXOffset>Q_wcslen(pCredit->szLyricLine) )
				m_iXOffset = Q_wcslen(pCredit->szLyricLine);
		}

		vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
		vgui::HFont m_hTFont = vgui::scheme()->GetIScheme(scheme)->GetFont( pCredit->szFontName, true );

		int iFontTall = surface()->GetFontTall ( m_hTFont )+ (int) m_flSeparation;

		if ( pCredit->flYPos < -iFontTall ||  pCredit->flYPos > iTall || i<m_iCurrentLowY)
		{
			pCredit->bActive = false;
		}
		else
		{
			if (i <= m_iYOffset)
				pCredit->bActive = true;
		}

		Color cColor = m_TextColor;

		if ( pCredit->bActive == false )
			 continue;

		if (pCredit->iAsciiIndex>0)
		{
			m_iCurrentAsciiArt = pCredit->iAsciiIndex; 
		}
		surface()->DrawSetTextFont( m_hTFont );
		surface()->DrawSetTextColor( cColor[0], cColor[1], cColor[2], cColor[3]  );
		
		if (!bBorders)
		{
			wchar_t bunicode[512];
			wchar_t bVunicode[2];
			wchar_t bVunicode2[3];

			int iBorderEndX  = GetPixelWidth( m_szBorderH, m_hTFont );

			g_pVGuiLocalize->ConvertANSIToUnicode( m_szBorderH, bunicode, sizeof( bunicode ) );
			g_pVGuiLocalize->ConvertANSIToUnicode( "|", bVunicode, sizeof( bVunicode ) );
			g_pVGuiLocalize->ConvertANSIToUnicode( "||", bVunicode2, sizeof( bVunicode2 ) );
			int iBorderPipeWidth = GetPixelWidth( "||", m_hTFont);
			surface()->DrawSetTextColor( m_cColor[0], m_cColor[1], m_cColor[2], cColor[3] );
			surface()->DrawSetTextPos( 5+m_iScreenXOffset, 10+m_iScreenYOffset);
			surface()->DrawUnicodeString( bunicode );
			surface()->DrawSetTextPos( (iWidth/2)+m_iScreenXOffset, 10+m_iScreenYOffset);
			surface()->DrawUnicodeString( bunicode );
			surface()->DrawSetTextPos( 5+m_iScreenXOffset, (iTall - (iFontTall*2))+m_iScreenYOffset);
			surface()->DrawUnicodeString( bunicode );
			surface()->DrawSetTextPos( (iWidth/2)+m_iScreenXOffset, (iTall/2)+m_iScreenYOffset);
			surface()->DrawUnicodeString( bunicode );

			int iBorderY = 10+iFontTall;
			while (iBorderY < (iTall - (iFontTall*3)))
			{
				surface()->DrawSetTextPos( 5+m_iScreenXOffset, iBorderY+m_iScreenYOffset);
				surface()->DrawUnicodeString( bVunicode );
				surface()->DrawSetTextPos( iBorderEndX+m_iScreenXOffset, iBorderY+m_iScreenYOffset);
				if (iBorderY<(iTall/2))
				{
					surface()->DrawUnicodeString( bVunicode2 );
					surface()->DrawSetTextPos( (iWidth-iBorderPipeWidth)+m_iScreenXOffset, iBorderY+m_iScreenYOffset);
					surface()->DrawUnicodeString( bVunicode );
				}
				else
				{
					surface()->DrawUnicodeString( bVunicode );
				}
				iBorderY+=iFontTall;
			}
			bBorders = true;
		}

		wchar_t tmpstr[256];
		memset( tmpstr, 0, sizeof(tmpstr));
		if (m_iYOffset == i)
		{
			Q_wcsncpy( tmpstr, pCredit->szLyricLine, m_iXOffset * sizeof( wchar_t ) );
			if (bCursor)
				tmpstr[m_iXOffset - 1] = '_';
			
		}
		else
		{
			Q_wcsncpy( tmpstr, pCredit->szLyricLine, (Q_wcslen( pCredit->szLyricLine ) + 1) * sizeof( wchar_t ));
		}

		surface()->DrawSetTextColor( m_cColor[0], m_cColor[1], m_cColor[2], cColor[3] );
		surface()->DrawSetTextPos( 20+pCredit->iXOffset+m_iScreenXOffset, 30+pCredit->flYPos+m_iScreenYOffset );
		surface()->DrawUnicodeString( tmpstr );
	}

	DrawPortalAsciiArt();
	
	//Draw Credits
	int iCreditsListCount = m_CreditsList.Count();
	for ( int i = iCreditsListCount-1; i >=0; i-- )
	{
		portalcreditname_t *pCredit = &m_CreditsList[i];

		if ( pCredit == NULL )
			 continue;

		if (pCredit->flYPos < 0) break;

		vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
		vgui::HFont m_hTFont = vgui::scheme()->GetIScheme(scheme)->GetFont( pCredit->szFontName, true );
		int iFontTall = surface()->GetFontTall ( m_hTFont );

		if (pCredit->flTimeStart <= flCurTime && pCredit->flTimeEnd > flCurTime)
		{
			if (iLastNameY!=i)
			{
				int iYDelta = iFontTall + (int) m_flSeparation;			
				for (int j = 0; j <i; j++)
				{
					m_CreditsList[j].flYPos -= (float) iYDelta;
				}
				m_CreditsList[i].flYPos = (iTall/2) - iYDelta;
				iLastNameY=i;
			}
			m_iYOffsetNames = i;
			m_iXOffsetNames = (int) (((flCurTime-pCredit->flTimeStart) / pCredit->flTimeInit)*Q_strlen(pCredit->szCreditName))+1;
			if (m_iXOffsetNames<1) m_iXOffsetNames = 1;
			else if (m_iXOffsetNames>Q_strlen(pCredit->szCreditName) ) m_iXOffsetNames = Q_strlen(pCredit->szCreditName);
		}


		//if ( pCredit->flYPos < -iFontTall ||  pCredit->flYPos > iTall)
		if ( pCredit->flYPos < 20 ||  pCredit->flYPos > (iTall/2))
		{
			pCredit->bActive = false;
		}
		else
		{
			if (i <= m_iYOffsetNames)
				pCredit->bActive = true;
		}

		Color cColor = m_TextColor;

		if ( pCredit->bActive == false )
			 continue;
			
		surface()->DrawSetTextFont( m_hTFont );
		surface()->DrawSetTextColor( cColor[0], cColor[1], cColor[2], cColor[3]  );
		
		wchar_t unicode[256];

		char tmpstr[256];
		if (m_iYOffsetNames == i)
		{
			char tmpstr2[256];
			Q_strncpy( tmpstr2, pCredit->szCreditName, m_iXOffsetNames );
			if (!bCursor)
				Q_snprintf( tmpstr, 256, "%s_",tmpstr2);
			else
				Q_snprintf( tmpstr, 256, "%s",tmpstr2);
		}
		else
		{
			Q_strncpy( tmpstr, pCredit->szCreditName, Q_strlen( pCredit->szCreditName )+1 );
		}

		if ( tmpstr[0] == '#' )
		{
			g_pVGuiLocalize->ConstructString( unicode, sizeof(unicode), g_pVGuiLocalize->Find(tmpstr), 0 );
		}
		else
		{			
			g_pVGuiLocalize->ConvertANSIToUnicode( tmpstr, unicode, sizeof( unicode ) );
		}

		// int iStringWidth = GetStringPixelWidth( unicode, m_hTFont ); 
		// surface()->DrawSetTextPos( ( iWidth / 2 ) - ( iStringWidth / 2 ), pCredit->flYPos );
		surface()->DrawSetTextColor( m_cColor[0], m_cColor[1], m_cColor[2], cColor[3] );
		surface()->DrawSetTextPos( (iWidth/2)+10+m_iScreenXOffset, pCredit->flYPos+m_iScreenYOffset );
		surface()->DrawUnicodeString( unicode );

	}
}


void CHudPortalCredits::DrawLogo( void )
{
	if( m_iLogoState == LOGO_FADEOFF )
	{
		SetActive( false );
		return;
	}

	switch( m_iLogoState )
	{
		case LOGO_FADEIN:
		{
			float flDeltaTime = ( m_flFadeTime - gpGlobals->curtime );

			m_Alpha = MAX( 0, RemapValClamped( flDeltaTime, 5.0f, 0, -128, 255 ) );

			if ( flDeltaTime <= 0.0f )
			{
				m_iLogoState = LOGO_FADEHOLD;
				m_flFadeTime = gpGlobals->curtime + m_flLogoDesiredLength;
			}

			break;
		}

		case LOGO_FADEHOLD:
		{
			if ( m_flFadeTime <= gpGlobals->curtime )
			{
				m_iLogoState = LOGO_FADEOUT;
				m_flFadeTime = gpGlobals->curtime + 2.0f;
			}
			break;
		}

		case LOGO_FADEOUT:
		{
			float flDeltaTime = ( m_flFadeTime - gpGlobals->curtime );

			m_Alpha = RemapValClamped( flDeltaTime, 0.0f, 2.0f, 0, 255 );

			if ( flDeltaTime <= 0.0f )
			{
				m_iLogoState = LOGO_FADEOFF;
				SetActive( false );
			}

			break;
		}
	}

	// fill the screen
	int iWidth, iTall;
	GetHudSize(iWidth, iTall);

	iWidth = static_cast<int>( static_cast<float>(iTall) * (4.0f/3.0f) );

	iWidth += m_iScreenWidthAdjustment;
	iTall += m_iScreenHeightAdjustment;

	SetSize( iWidth, iTall );

	char szLogoFont[64];

	if ( IsXbox() )
	{
		Q_snprintf( szLogoFont, sizeof( szLogoFont ), "WeaponIcons_Small" );
	}
	else if ( hl2_episodic.GetBool() )
	{
		Q_snprintf( szLogoFont, sizeof( szLogoFont ), "ClientTitleFont" );
	}
	else
	{
		Q_snprintf( szLogoFont, sizeof( szLogoFont ), "WeaponIcons" );
	}

	vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
	vgui::HFont m_hTFont = vgui::scheme()->GetIScheme(scheme)->GetFont( szLogoFont );

	int iFontTall = surface()->GetFontTall ( m_hTFont );

	Color cColor = m_TextColor;
	cColor[3] = m_Alpha;
				
	surface()->DrawSetTextFont( m_hTFont );
	surface()->DrawSetTextColor( cColor[0], cColor[1], cColor[2], cColor[3]  );
	
	wchar_t unicode[256];
	g_pVGuiLocalize->ConvertANSIToUnicode( "HALF-LIFE'", unicode, sizeof( unicode ) );

	int iStringWidth = GetStringPixelWidth( unicode, m_hTFont ); 

	surface()->DrawSetTextPos( ( iWidth / 2 ) - ( iStringWidth / 2 ), ( iTall / 2 ) - ( iFontTall / 2 ) );
	surface()->DrawUnicodeString( unicode );

	//Adrian: This should really be exposed.
	if ( hl2_episodic.GetBool() )
	{
		g_pVGuiLocalize->ConvertANSIToUnicode( "== episode one==", unicode, sizeof( unicode ) );

		iStringWidth = GetStringPixelWidth( unicode, m_hTFont ); 

		surface()->DrawSetTextPos( ( iWidth / 2 ) - ( iStringWidth / 2 ), ( iTall / 2 ) + ( iFontTall / 2 ));
		surface()->DrawUnicodeString( unicode );
	}

	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CHudPortalCredits::FadeBlend( float fadein, float fadeout, float hold, float localTime )
{
	float fadeTime = fadein + hold;
	float fadeBlend;

	if ( localTime < 0 )
		return 0;

	if ( localTime < fadein )
	{
		fadeBlend = 1 - ((fadein - localTime) / fadein);
	}
	else if ( localTime > fadeTime )
	{
		if ( fadeout > 0 )
			fadeBlend = 1 - ((localTime - fadeTime) / fadeout);
		else
			fadeBlend = 0;
	}
	else
		fadeBlend = 1;

	if ( fadeBlend < 0 )
		 fadeBlend = 0;

	return fadeBlend;
}

void CHudPortalCredits::DrawIntroCreditsName( void )
{
	if ( m_CreditsList.Count() == 0 )
		 return;
	
	// fill the screen
	int iWidth, iTall;
	GetHudSize(iWidth, iTall);

	iWidth = static_cast<int>( static_cast<float>(iTall) * (4.0f/3.0f) );

	SetSize( iWidth, iTall );

	for ( int i = 0; i < m_CreditsList.Count(); i++ )
	{
		portalcreditname_t *pCredit = &m_CreditsList[i];

		if ( pCredit == NULL )
			 continue;

		if ( pCredit->bActive == false )
			 continue;
				
		vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
		vgui::HFont m_hTFont = vgui::scheme()->GetIScheme(scheme)->GetFont( pCredit->szFontName );

		float localTime = gpGlobals->curtime - pCredit->flTimeStart;

		surface()->DrawSetTextFont( m_hTFont );
		surface()->DrawSetTextColor( m_cColor[0], m_cColor[1], m_cColor[2], FadeBlend( m_flFadeInTime, m_flFadeOutTime, m_flFadeHoldTime + pCredit->flTimeAdd, localTime ) * m_cColor[3] );
		
		wchar_t unicode[256];
		g_pVGuiLocalize->ConvertANSIToUnicode( pCredit->szCreditName, unicode, sizeof( unicode ) );

		surface()->DrawSetTextPos( XRES( pCredit->flXPos ), YRES( pCredit->flYPos ) );
		surface()->DrawUnicodeString( unicode );
		
		if ( m_flLogoTime > gpGlobals->curtime )
			 continue;
		
		if ( pCredit->flTime - m_flNextStartTime <= gpGlobals->curtime )
		{
			if ( m_CreditsList.IsValidIndex( i + 3 ) )
			{
				portalcreditname_t *pNextCredits = &m_CreditsList[i + 3];

				if ( pNextCredits && pNextCredits->flTime == 0.0f )
				{
					pNextCredits->bActive = true;

					if ( i < 3 )
					{
						pNextCredits->flTimeAdd = ( i + 1.0f );
						pNextCredits->flTime = gpGlobals->curtime + m_flFadeInTime + m_flFadeOutTime + m_flFadeHoldTime + pNextCredits->flTimeAdd;
					}
					else
					{
						pNextCredits->flTimeAdd = m_flPauseBetweenWaves;
						pNextCredits->flTime = gpGlobals->curtime + m_flFadeInTime + m_flFadeOutTime + m_flFadeHoldTime + pNextCredits->flTimeAdd;
					}

					pNextCredits->flTimeStart = gpGlobals->curtime;

					pNextCredits->iSlot = pCredit->iSlot;
				}
			}
		}

		if ( pCredit->flTime <= gpGlobals->curtime )
		{
			pCredit->bActive = false;

			if ( i == m_CreditsList.Count()-1 )
			{
				Clear();
			}
		}
	}
}

void CHudPortalCredits::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetVisible( ShouldDraw() );

	SetBgColor( Color(0, 0, 0, 0) );
}

void CHudPortalCredits::Paint()
{
	if ( m_iCreditsType == CREDITS_LOGO )
	{
		DrawLogo();
	}
	else if ( m_iCreditsType == CREDITS_INTRO )
	{
		DrawIntroCreditsName();
	}
	else if ( m_iCreditsType == CREDITS_OUTRO )
	{
		DrawOutroCreditsName();
	}
	else if ( m_iCreditsType == CREDITS_OUTRO_PORTAL )
	{
		//DrawPortalOutroCreditsName();
		DrawPortalOutroCreditsLyrics();
	}

}

void CHudPortalCredits::PrepareLogo( float flTime )
{
	m_Alpha = 0;
	m_flLogoDesiredLength = flTime;
	m_flFadeTime = gpGlobals->curtime + 5.0f;
	m_iLogoState = LOGO_FADEIN;
	SetActive( true );
}

void CHudPortalCredits::PrepareOutroCredits( void )
{
	PrepareCredits( "OutroCreditsNames" );
	
	if ( m_CreditsList.Count() == 0 )
		 return;

	// fill the screen
	int iWidth, iTall;
	GetHudSize(iWidth, iTall);

	iWidth = static_cast<int>( static_cast<float>(iTall) * (4.0f/3.0f) );

	SetSize( iWidth, iTall );

	int iHeight = iTall;

	for ( int i = 0; i < m_CreditsList.Count(); i++ )
	{
		portalcreditname_t *pCredit = &m_CreditsList[i];

		if ( pCredit == NULL )
			 continue;

		vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
		vgui::HFont m_hTFont = vgui::scheme()->GetIScheme(scheme)->GetFont( pCredit->szFontName, true );

		pCredit->flYPos = iHeight;
		pCredit->bActive = false;

		iHeight += surface()->GetFontTall ( m_hTFont ) + m_flSeparation;
	}

	SetActive( true );

	g_iPortalCreditsPixelHeight = iHeight;
}

void CHudPortalCredits::PreparePortalOutroCredits( void )
{
	m_iCurrentLowY = 0;
	m_bStartSong = true;
	m_flLyricsStartTime = -1;

	m_iCurrentAsciiArt = 0;
	
	PrepareCredits( "OutroCreditsNames" );
	
	if ( m_CreditsList.Count() == 0 )
		 return;

	// fill the screen
	int iWidth, iTall;
	GetHudSize(iWidth, iTall);

	iWidth = static_cast<int>( static_cast<float>(iTall) * (4.0f/3.0f) );

	iWidth += m_iScreenWidthAdjustment;
	iTall += m_iScreenHeightAdjustment;
	SetSize( iWidth+m_iScreenXOffset, iTall +m_iScreenYOffset );

	m_iYOffset = 0;
	m_iXOffset = 1;
	m_iYOffsetNames = 0;
	m_iXOffsetNames = 1;

	//int iHeight = iTall;
	int iHeight = -1;
	int iTotalNameLen = 0;

	
	//Prepare Credit Names
	for ( int i = 0; i < m_CreditsList.Count(); i++ )
	{
		portalcreditname_t *pCredit = &m_CreditsList[i];



		if ( pCredit == NULL )
			 continue;

		vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
		vgui::HFont m_hTFont = vgui::scheme()->GetIScheme(scheme)->GetFont( pCredit->szFontName, true );

		if (iHeight == -1) iHeight = (iTall/2) - (surface()->GetFontTall ( m_hTFont ) + m_flSeparation);
		else iHeight += surface()->GetFontTall ( m_hTFont ) + m_flSeparation;

		if (i==0)
		{
			Q_strcpy(m_szBorderH,"-");
			while(GetPixelWidth( m_szBorderH, m_hTFont )< ((iWidth/2)-GetPixelWidth( "---", m_hTFont )))
			{
				int iCurrentLength = Q_strlen(m_szBorderH);
				if (iCurrentLength < sizeof(m_szBorderH))
				{
					m_szBorderH[iCurrentLength] = '-';
				}
				else
				{
					break;
				}
			}
		}


		pCredit->flYPos = iHeight;
		pCredit->bActive = false;

		iTotalNameLen += Q_strlen(pCredit->szCreditName);

	}

	m_flNameCharTime = m_flScrollTime / iTotalNameLen;

	float flStart = m_flScrollCreditsStart;
	for ( int i = 0; i < m_CreditsList.Count(); i++ )
	{
		m_CreditsList[i].flTimeStart=flStart;
		m_CreditsList[i].flTimeInit = Q_strlen(m_CreditsList[i].szCreditName)*m_flNameCharTime;
		flStart += m_CreditsList[i].flTimeInit;
		m_CreditsList[i].flTimeEnd=flStart;
	}
	
	SetActive( true );

	
	m_flScrollTime = 2;
	m_flLastPaintTime = gpGlobals->curtime;
	m_flLastBlinkTime = gpGlobals->curtime;
}

void CHudPortalCredits::PrepareIntroCredits( void )
{
	PrepareCredits( "IntroCreditsNames" );

	int iSlot = 0;

	for ( int i = 0; i < m_CreditsList.Count(); i++ )
	{
		portalcreditname_t *pCredit = &m_CreditsList[i];

		if ( pCredit == NULL )
			 continue;

		vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
		vgui::HFont m_hTFont = vgui::scheme()->GetIScheme(scheme)->GetFont( pCredit->szFontName );

		pCredit->flYPos = m_flY + ( iSlot * surface()->GetFontTall ( m_hTFont ) );
		pCredit->flXPos = m_flX;
				
		if ( i < 3 )
		{
			pCredit->bActive = true;
			pCredit->iSlot = iSlot;
			pCredit->flTime = gpGlobals->curtime + m_flFadeInTime + m_flFadeOutTime + m_flFadeHoldTime;
			pCredit->flTimeStart = gpGlobals->curtime;
			m_flLogoTime = pCredit->flTime + m_flLogoTimeMod;
		}
		else
		{
			pCredit->bActive = false;
			pCredit->flTime = 0.0f;
		}

		iSlot = ( iSlot + 1 ) % 3;
	}

	SetActive( true );
}

void CHudPortalCredits::MsgFunc_CreditsPortalMsg( bf_read &msg )
{
	m_iCreditsType = msg.ReadByte();

	switch ( m_iCreditsType )
	{
		case CREDITS_LOGO:
		{
			PrepareLogo( 5.0f );
			break;
		}
		case CREDITS_INTRO:
		{
			PrepareIntroCredits();
			break;
		}
		case CREDITS_OUTRO:
		{
			PrepareOutroCredits();
			break;
		}
		case CREDITS_OUTRO_PORTAL:
		{
			PreparePortalOutroCredits();
			break;
		}

	}
}

void CHudPortalCredits::MsgFunc_LogoTimeMsg( bf_read &msg )
{
	m_iCreditsType = CREDITS_LOGO;
	PrepareLogo( msg.ReadFloat() );
}


