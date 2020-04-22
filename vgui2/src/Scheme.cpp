//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include <stdio.h>
#include <math.h>

#include <vgui/VGUI.h>
#include <vgui/IScheme.h>
#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/IPanel.h>
#include <vgui/ISystem.h>
#include <vstdlib/IKeyValuesSystem.h>
#include <vgui/IVGui.h>

#include "tier1/utlvector.h"
#include "tier1/utlrbtree.h"
#include "tier1/utldict.h"
#include "VGUI_Border.h"
#include "ScalableImageBorder.h"
#include "ImageBorder.h"
#include "vgui_internal.h"
#include "bitmap.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;
#define FONT_ALIAS_NAME_LENGTH 64

//-----------------------------------------------------------------------------
// Purpose: Implementation of global scheme interface
//-----------------------------------------------------------------------------
class CScheme : public IScheme
{
public:
	CScheme();

	// gets a string from the default settings section
	virtual const char *GetResourceString(const char *stringName);

	// returns a pointer to an existing border
	virtual IBorder *GetBorder(const char *borderName);

	// returns a pointer to an existing font
	virtual HFont GetFont(const char *fontName, bool proportional);

	// m_pkvColors
	virtual Color GetColor( const char *colorName, Color defaultColor);


	void Shutdown( bool full );
	void LoadFromFile( VPANEL sizingPanel, const char *filename, const char *tag, KeyValues *inKeys );

	// Gets at the scheme's name
	const char *GetName() { return tag; }
	const char *GetFileName() { return fileName; }

	char const *GetFontName( const HFont& font );

	void ReloadFontGlyphs();

	VPANEL		GetSizingPanel() { return m_SizingPanel; }

	void SpewFonts();

	bool GetFontRange( const char *fontname, int &nMin, int &nMax );
	void SetFontRange( const char *fontname, int nMin, int nMax );
	
	// Get the number of borders
	virtual int GetBorderCount() const;

	// Get the border at the given index
	virtual IBorder *GetBorderAtIndex( int iIndex );

	// Get the number of fonts
	virtual int GetFontCount() const;

	// Get the font at the given index
	virtual HFont GetFontAtIndex( int iIndex );
	
	// Get color data
	virtual const KeyValues *GetColorData() const;

private:
	const char *LookupSchemeSetting(const char *pchSetting);
	const char *GetMungedFontName( const char *fontName, const char *scheme, bool proportional);
	void LoadFonts();
	void LoadBorders();
	HFont FindFontInAliasList( const char *fontName );
	int GetMinimumFontHeightForCurrentLanguage();

	char fileName[256];
	char tag[64];

	KeyValues *m_pData;
	KeyValues *m_pkvBaseSettings;
	KeyValues *m_pkvColors;
	int		   m_nColorCount;

	struct SchemeBorder_t
	{
		IBorder *border;
		int borderSymbol;
		bool bSharedBorder;
	};
	CUtlVector<SchemeBorder_t> m_BorderList;
	IBorder  *m_pBaseBorder;	// default border to use if others not found
	KeyValues *m_pkvBorders;
#pragma pack(1)
	struct fontalias_t
	{
		CUtlSymbol _trueFontName;
		unsigned short _font : 15;
		unsigned short m_bProportional : 1;
	};
#pragma pack()
	friend struct fontalias_t;

	CUtlDict< fontalias_t, int >	m_FontAliases;
	VPANEL m_SizingPanel;
	int			m_nScreenWide;
	int			m_nScreenTall;

	struct fontrange_t
	{
		int _min;
		int _max;
	};
	CUtlDict< fontrange_t, int >	m_FontRanges;
};


//-----------------------------------------------------------------------------
// Purpose: Implementation of global scheme interface
//-----------------------------------------------------------------------------
class CSchemeManager : public ISchemeManager
{
public:
	CSchemeManager();
	~CSchemeManager();

	// loads a scheme from a file
	// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
	// tag is friendly string representing the name of the loaded scheme
	virtual HScheme LoadSchemeFromFile(const char *fileName, const char *tag);
	// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
	virtual HScheme LoadSchemeFromFileEx( VPANEL sizingPanel, const char *fileName, const char *tag);

	// reloads the schemes from the file
	virtual void ReloadSchemes();
	virtual void ReloadFonts();

	// returns a handle to the default (first loaded) scheme
	virtual HScheme GetDefaultScheme();

	// returns a handle to the scheme identified by "tag"
	virtual HScheme GetScheme(const char *tag);

	// returns a pointer to an image
	virtual IImage *GetImage(const char *imageName, bool hardwareFiltered);
	virtual HTexture GetImageID(const char *imageName, bool hardwareFiltered);

	virtual IScheme *GetIScheme( HScheme scheme );

	virtual void Shutdown( bool full );

	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	virtual int GetProportionalScaledValue(int normalizedValue);
	virtual int GetProportionalNormalizedValue(int scaledValue);

	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	virtual int GetProportionalScaledValueEx( HScheme scheme, int normalizedValue );
	virtual int GetProportionalNormalizedValueEx( HScheme scheme, int scaledValue );

	virtual bool DeleteImage( const char *pImageName );

	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	int GetProportionalScaledValueEx( CScheme *pScheme, int normalizedValue );
	int GetProportionalNormalizedValueEx( CScheme *pScheme, int scaledValue );

	void SpewFonts();

private:

	int GetProportionalScaledValue_( int rootWide, int rootTall, int normalizedValue );
	int GetProportionalNormalizedValue_( int rootWide, int rootTall, int scaledValue );

	// Search for already-loaded schemes
	HScheme FindLoadedScheme(const char *fileName);

	CUtlVector<CScheme *> m_Schemes;

	static const char *s_pszSearchString;
	struct CachedBitmapHandle_t
	{
		Bitmap *pBitmap;
	};
	static bool BitmapHandleSearchFunc(const CachedBitmapHandle_t &, const CachedBitmapHandle_t &);
	CUtlRBTree<CachedBitmapHandle_t, int> m_Bitmaps;
};

const char *CSchemeManager::s_pszSearchString = NULL;

//-----------------------------------------------------------------------------
// Purpose: search function for stored bitmaps
//-----------------------------------------------------------------------------
bool CSchemeManager::BitmapHandleSearchFunc(const CachedBitmapHandle_t &lhs, const CachedBitmapHandle_t &rhs)
{
	// a NULL bitmap indicates to use the search string instead
	if (lhs.pBitmap && rhs.pBitmap)
	{
		return stricmp(lhs.pBitmap->GetName(), rhs.pBitmap->GetName()) > 0;
	}
	else if (lhs.pBitmap)
	{
		return stricmp(lhs.pBitmap->GetName(), s_pszSearchString) > 0;
	}
	return stricmp(s_pszSearchString, rhs.pBitmap->GetName()) > 0;
}



CSchemeManager g_Scheme;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CSchemeManager, ISchemeManager, VGUI_SCHEME_INTERFACE_VERSION, g_Scheme);


namespace vgui
{
vgui::ISchemeManager *g_pScheme = &g_Scheme;
} // namespace vgui

CON_COMMAND( vgui_spew_fonts, "" )
{
	g_Scheme.SpewFonts();
}
 
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSchemeManager::CSchemeManager()
{
	// 0th element is null, since that would be an invalid handle
	CScheme *nullScheme = new CScheme();
	m_Schemes.AddToTail(nullScheme);
	m_Bitmaps.SetLessFunc(&BitmapHandleSearchFunc);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSchemeManager::~CSchemeManager()
{
	int i;
	for ( i = 0; i < m_Schemes.Count(); i++ )
	{
		delete m_Schemes[i];
	}
	m_Schemes.RemoveAll();

	for ( i = 0; i < m_Bitmaps.MaxElement(); i++ )
	{
		if (m_Bitmaps.IsValidIndex(i))
		{
			delete m_Bitmaps[i].pBitmap;
		}
	}
	m_Bitmaps.RemoveAll();

	Shutdown( false );
}

//-----------------------------------------------------------------------------
// Purpose: Reload the fonts in all schemes
//-----------------------------------------------------------------------------
void CSchemeManager::ReloadFonts()
{
	for (int i = 1; i < m_Schemes.Count(); i++)
	{
		m_Schemes[i]->ReloadFontGlyphs();
	}
}

//-----------------------------------------------------------------------------
// Converts the handle into an interface
//-----------------------------------------------------------------------------
IScheme *CSchemeManager::GetIScheme( HScheme scheme )
{
	if ( scheme >= (unsigned long)m_Schemes.Count() )
	{
		AssertOnce( !"Invalid scheme requested." );
		return NULL;
	}
	else
	{
		return m_Schemes[scheme];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSchemeManager::Shutdown( bool full )
{
	// Full shutdown kills the null scheme
	for( int i = full ? 0 : 1; i < m_Schemes.Count(); i++ )
	{
		m_Schemes[i]->Shutdown( full );
	}

	if ( full )
	{
		m_Schemes.RemoveAll();
	}
}

//-----------------------------------------------------------------------------
// Purpose: loads a scheme from disk
//-----------------------------------------------------------------------------
HScheme CSchemeManager::FindLoadedScheme(const char *fileName)
{
	// Find the scheme in the list of already loaded schemes
	for (int i = 1; i < m_Schemes.Count(); i++)
	{
		char const *schemeFileName = m_Schemes[i]->GetFileName();
		if (!stricmp(schemeFileName, fileName))
			return i;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CScheme::CScheme()
{
	fileName[ 0 ] = 0;
	tag[ 0 ] = 0;

	m_pData = NULL;
	m_pkvBaseSettings = NULL;
	m_pkvColors = NULL;
	m_nColorCount = 0;

	m_pBaseBorder = NULL;	// default border to use if others not found
	m_pkvBorders = NULL;
	m_SizingPanel = 0;
	m_nScreenWide = -1;
	m_nScreenTall = -1;
}

// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
HScheme  CSchemeManager::LoadSchemeFromFileEx( VPANEL sizingPanel, const char *fileName, const char *tag)
{
	// Look to see if we've already got this scheme...
	HScheme hScheme = FindLoadedScheme(fileName);
	if (hScheme != 0)
	{
		CScheme *pScheme = static_cast< CScheme * >( GetIScheme( hScheme ) );
		if ( IsPC() && pScheme )
		{
			pScheme->ReloadFontGlyphs();
		}
		return hScheme;
	}

	KeyValues *data;
	data = new KeyValues("Scheme");

	data->UsesEscapeSequences( true );	// VGUI uses this
	
	// Look first in game directory
	bool result = data->LoadFromFile( g_pFullFileSystem, fileName, "GAME" );
	if ( !result )
	{
		// look in any directory
		result = data->LoadFromFile( g_pFullFileSystem, fileName, NULL );
	}

	if (!result)
	{
		data->deleteThis();
		return 0;
	}
	
	if ( IsX360() )
	{
		data->ProcessResolutionKeys( g_pSurface->GetResolutionKey() );
	}
	if ( IsPC() )
	{
		ConVarRef cl_hud_minmode( "cl_hud_minmode", true );
		if ( cl_hud_minmode.IsValid() && cl_hud_minmode.GetBool() )
		{
			data->ProcessResolutionKeys( "_minmode" );
		}
	}
	if( g_pIVgui->GetVRMode() )
	{
		data->ProcessResolutionKeys( "_vrmode" );
	}

	CScheme *newScheme = new CScheme();
	newScheme->LoadFromFile( sizingPanel, fileName, tag, data );

	return m_Schemes.AddToTail(newScheme);
}

//-----------------------------------------------------------------------------
// Purpose: loads a scheme from disk
//-----------------------------------------------------------------------------
HScheme CSchemeManager::LoadSchemeFromFile(const char *fileName, const char *tag)
{
	return LoadSchemeFromFileEx( 0, fileName, tag );
}

//-----------------------------------------------------------------------------
// Purpose: Table of scheme file entries, translation from old goldsrc schemes to new src schemes
//-----------------------------------------------------------------------------
struct SchemeEntryTranslation_t
{
	const char *pchNewEntry;
	const char *pchOldEntry;
	const char *pchDefaultValue;
};
SchemeEntryTranslation_t g_SchemeTranslation[] =
{
	{ "Border.Bright",					"BorderBright",		"200 200 200 196" },	// the lit side of a control
	{ "Border.Dark"						"BorderDark",		"40 40 40 196" },		// the dark/unlit side of a control
	{ "Border.Selection"				"BorderSelection",	"0 0 0 196" },			// the additional border color for displaying the default/selected button

	{ "Button.TextColor",				"ControlFG",		"White" },
	{ "Button.BgColor",					"ControlBG",		"Blank" },
	{ "Button.ArmedTextColor",			"ControlFG" },
	{ "Button.ArmedBgColor",			"ControlBG" },
	{ "Button.DepressedTextColor",		"ControlFG" },
	{ "Button.DepressedBgColor",		"ControlBG" },
	{ "Button.FocusBorderColor",		"0 0 0 255" },

	{ "CheckButton.TextColor",			"BaseText" },
	{ "CheckButton.SelectedTextColor",	"BrightControlText" },
	{ "CheckButton.BgColor",			"CheckBgColor" },
	{ "CheckButton.Border1",  			"CheckButtonBorder1" },
	{ "CheckButton.Border2",  			"CheckButtonBorder2" },
	{ "CheckButton.Check",				"CheckButtonCheck" },

	{ "ComboBoxButton.ArrowColor",		"LabelDimText" },
	{ "ComboBoxButton.ArmedArrowColor",	"MenuButton/ArmedArrowColor" },
	{ "ComboBoxButton.BgColor",			"MenuButton/ButtonBgColor" },
	{ "ComboBoxButton.DisabledBgColor",	"ControlBG" },

	{ "Frame.TitleTextInsetX",			NULL,		"32" },
	{ "Frame.ClientInsetX",				NULL,		"8" },
	{ "Frame.ClientInsetY",				NULL,		"6" },
	{ "Frame.BgColor",					"BgColor" },
	{ "Frame.OutOfFocusBgColor",		"BgColor" },
	{ "Frame.FocusTransitionEffectTime",NULL,		"0" },
	{ "Frame.TransitionEffectTime",		NULL,		"0" },
	{ "Frame.AutoSnapRange",			NULL,		"8" },
	{ "FrameGrip.Color1",				"BorderBright" },
	{ "FrameGrip.Color2",				"BorderSelection" },
	{ "FrameTitleButton.FgColor",		"TitleButtonFgColor" },
	{ "FrameTitleButton.BgColor",		"TitleButtonBgColor" },
	{ "FrameTitleButton.DisabledFgColor",	"TitleButtonDisabledFgColor" },
	{ "FrameTitleButton.DisabledBgColor",	"TitleButtonDisabledBgColor" },
	{ "FrameSystemButton.FgColor",		"TitleBarBgColor" },
	{ "FrameSystemButton.BgColor",		"TitleBarBgColor" },
	{ "FrameSystemButton.Icon",			"TitleBarIcon" },
	{ "FrameSystemButton.DisabledIcon",	"TitleBarDisabledIcon" },
	{ "FrameTitleBar.Font",				NULL,		"Default" },
	{ "FrameTitleBar.TextColor",		"TitleBarFgColor" },
	{ "FrameTitleBar.BgColor",			"TitleBarBgColor" },
	{ "FrameTitleBar.DisabledTextColor","TitleBarDisabledFgColor" },
	{ "FrameTitleBar.DisabledBgColor",	"TitleBarDisabledBgColor" },

	{ "GraphPanel.FgColor",				"BrightControlText" },
	{ "GraphPanel.BgColor",				"WindowBgColor" },

	{ "Label.TextDullColor",			"LabelDimText" },
	{ "Label.TextColor",				"BaseText" },
	{ "Label.TextBrightColor",			"BrightControlText" },
	{ "Label.SelectedTextColor",		"BrightControlText" },
	{ "Label.BgColor",					"LabelBgColor" },
	{ "Label.DisabledFgColor1",			"DisabledFgColor1" },
	{ "Label.DisabledFgColor2",			"DisabledFgColor2" },

	{ "ListPanel.TextColor",				"WindowFgColor" },
	{ "ListPanel.TextBgColor",				"Menu/ArmedBgColor" },
	{ "ListPanel.BgColor",					"ListBgColor" },
	{ "ListPanel.SelectedTextColor",		"ListSelectionFgColor" },
	{ "ListPanel.SelectedBgColor",			"Menu/ArmedBgColor" },
	{ "ListPanel.SelectedOutOfFocusBgColor","SelectionBG2" },
	{ "ListPanel.EmptyListInfoTextColor",	"LabelDimText" },
	{ "ListPanel.DisabledTextColor",		"LabelDimText" },
	{ "ListPanel.DisabledSelectedTextColor","ListBgColor" },

	{ "Menu.TextColor",					"Menu/FgColor" },
	{ "Menu.BgColor",					"Menu/BgColor" },
	{ "Menu.ArmedTextColor",			"Menu/ArmedFgColor" },
	{ "Menu.ArmedBgColor",				"Menu/ArmedBgColor" },
	{ "Menu.TextInset",					NULL,		"6" },

	{ "Panel.FgColor",					"FgColor" },
	{ "Panel.BgColor",					"BgColor" },

	{ "ProgressBar.FgColor",				"BrightControlText" },
	{ "ProgressBar.BgColor",				"WindowBgColor" },

	{ "PropertySheet.TextColor",			"FgColorDim" },
	{ "PropertySheet.SelectedTextColor",	"BrightControlText" },
	{ "PropertySheet.TransitionEffectTime",	NULL,		"0" },

	{ "RadioButton.TextColor",			"FgColor" },
	{ "RadioButton.SelectedTextColor",	"BrightControlText" },

	{ "RichText.TextColor",				"WindowFgColor" },
	{ "RichText.BgColor",				"WindowBgColor" },
	{ "RichText.SelectedTextColor",		"SelectionFgColor" },
	{ "RichText.SelectedBgColor",		"SelectionBgColor" },

	{ "ScrollBar.Wide",					NULL,		"19" },

	{ "ScrollBarButton.FgColor",			"DimBaseText" },
	{ "ScrollBarButton.BgColor",			"ControlBG" },
	{ "ScrollBarButton.ArmedFgColor",		"BaseText" },
	{ "ScrollBarButton.ArmedBgColor",		"ControlBG" },
	{ "ScrollBarButton.DepressedFgColor",	"BaseText" },
	{ "ScrollBarButton.DepressedBgColor",	"ControlBG" },

	{ "ScrollBarSlider.FgColor",				"ScrollBarSlider/ScrollBarSliderFgColor" },
	{ "ScrollBarSlider.BgColor",				"ScrollBarSlider/ScrollBarSliderBgColor" },

	{ "SectionedListPanel.HeaderTextColor",	"SectionTextColor" },
	{ "SectionedListPanel.HeaderBgColor",	"BuddyListBgColor" },
	{ "SectionedListPanel.DividerColor",	"SectionDividerColor" },
	{ "SectionedListPanel.TextColor",		"BuddyButton/FgColor1" },
	{ "SectionedListPanel.BrightTextColor",	"BuddyButton/ArmedFgColor1" },
	{ "SectionedListPanel.BgColor",			"BuddyListBgColor" },
	{ "SectionedListPanel.SelectedTextColor",			"BuddyButton/ArmedFgColor1" },
	{ "SectionedListPanel.SelectedBgColor",				"BuddyButton/ArmedBgColor" },
	{ "SectionedListPanel.OutOfFocusSelectedTextColor",	"BuddyButton/ArmedFgColor2" },
	{ "SectionedListPanel.OutOfFocusSelectedBgColor",	"SelectionBG2" },

	{ "Slider.NobColor",			"SliderTickColor" },
	{ "Slider.TextColor",			"Slider/SliderFgColor" },
	{ "Slider.TrackColor",			"SliderTrackColor"},
	{ "Slider.DisabledTextColor1",	"DisabledFgColor1" },
	{ "Slider.DisabledTextColor2",	"DisabledFgColor2" },

	{ "TextEntry.TextColor",		"WindowFgColor" },
	{ "TextEntry.BgColor",			"WindowBgColor" },
	{ "TextEntry.CursorColor",		"TextCursorColor" },
	{ "TextEntry.DisabledTextColor","WindowDisabledFgColor" },
	{ "TextEntry.DisabledBgColor",	"ControlBG" },
	{ "TextEntry.SelectedTextColor","SelectionFgColor" },
	{ "TextEntry.SelectedBgColor",	"SelectionBgColor" },
	{ "TextEntry.OutOfFocusSelectedBgColor",	"SelectionBG2" },
	{ "TextEntry.FocusEdgeColor",	"BorderSelection" },

	{ "ToggleButton.SelectedTextColor",	"BrightControlText" },

	{ "Tooltip.TextColor",			"BorderSelection" },
	{ "Tooltip.BgColor",			"SelectionBG" },

	{ "TreeView.BgColor",			"ListBgColor" },

	{ "WizardSubPanel.BgColor",		"SubPanelBgColor" },
};

//-----------------------------------------------------------------------------
// Purpose: loads a scheme from from disk into memory
//-----------------------------------------------------------------------------
void CScheme::LoadFromFile( VPANEL sizingPanel, const char *inFilename, const char *inTag, KeyValues *inKeys )
{
	COM_TimestampedLog( "CScheme::LoadFromFile( %s )", inFilename );

	Q_strncpy(fileName, inFilename, sizeof(fileName) );
	
	m_SizingPanel = sizingPanel;

	m_pData = inKeys;
	m_pkvBaseSettings = m_pData->FindKey("BaseSettings", true);
	m_pkvColors = m_pData->FindKey("Colors", true);

	// override the scheme name with the tag name
	KeyValues *name = m_pData->FindKey("Name", true);
	name->SetString("Name", inTag);

	if ( inTag )
	{
		Q_strncpy( tag, inTag, sizeof( tag ) );
	}
	else
	{
		Assert( "You need to name the scheme!" );
		Q_strncpy( tag, "default", sizeof( tag ) );
	}

	// translate format from goldsrc scheme to new scheme
	for (int i = 0; i < ARRAYSIZE(g_SchemeTranslation); i++)
	{
		if (!m_pkvBaseSettings->FindKey(g_SchemeTranslation[i].pchNewEntry, false))
		{
			const char *pchColor;

			if (g_SchemeTranslation[i].pchOldEntry)
			{
				pchColor = LookupSchemeSetting(g_SchemeTranslation[i].pchOldEntry);
			}
			else
			{
				pchColor = g_SchemeTranslation[i].pchDefaultValue;
			}

			Assert( pchColor );

			m_pkvBaseSettings->SetString(g_SchemeTranslation[i].pchNewEntry, pchColor);
		}
	}

	// need to copy tag before loading fonts
	LoadFonts();
	LoadBorders();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CScheme::GetFontRange( const char *fontname, int &nMin, int &nMax )
{
	int i = m_FontRanges.Find( fontname );
	if ( i != m_FontRanges.InvalidIndex() )
	{
		nMin = m_FontRanges[i]._min;
		nMax = m_FontRanges[i]._max;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CScheme::SetFontRange( const char *fontname, int nMin, int nMax )
{
	int i = m_FontRanges.Find( fontname );
	if ( i != m_FontRanges.InvalidIndex() )
	{
		m_FontRanges[i]._min = nMin;
		m_FontRanges[i]._max = nMax;
		return;
	}

	// not already in our list
	int iNew = m_FontRanges.Insert( fontname );
	
	m_FontRanges[iNew]._min = nMin;
	m_FontRanges[iNew]._max = nMax;
}

//-----------------------------------------------------------------------------
// Purpose: adds all the font specifications to the surface
//-----------------------------------------------------------------------------
void CScheme::LoadFonts()
{
	bool bValid = false;
	char language[64];
	memset( language, 0, sizeof( language ) );

	// get our language
	if ( IsPC() )
	{
		bValid = vgui::g_pSystem->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Source\\Language", language, sizeof( language ) - 1 );
	}
	else
	{
		Q_strncpy( language, XBX_GetLanguageString(), sizeof( language ) );
		bValid = true;
	}

	if ( !bValid )
	{
		Q_strncpy( language, "english", sizeof( language ) );
	}

	// add our custom fonts
	for (KeyValues *kv = m_pData->FindKey("CustomFontFiles", true)->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		const char *fontFile = kv->GetString();
		if (fontFile && *fontFile)
		{
			g_pSurface->AddCustomFontFile( NULL, fontFile );
		}
		else
		{
			// we have a block to read
			int nRangeMin = 0, nRangeMax = 0;
			const char *pszName = NULL;
			bool bUseRange = false;

			for ( KeyValues *pData = kv->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
			{
				const char *pszKey = pData->GetName();
				if ( !Q_stricmp( pszKey, "font" ) )
				{
					fontFile = pData->GetString();
				}
				else if ( !Q_stricmp( pszKey, "name" ) )
				{
					pszName = pData->GetString();
				}
				else
				{
					// we must have a language
					if ( Q_stricmp( language, pszKey ) == 0 ) // matches the language we're running?
					{
						// get the range
						KeyValues *pRange = pData->FindKey( "range" );
						if ( pRange )
						{
							bUseRange = true;
							sscanf( pRange->GetString(), "%x %x", &nRangeMin, &nRangeMax );

							if ( nRangeMin > nRangeMax )
							{
								int nTemp = nRangeMin;
								nRangeMin = nRangeMax;
								nRangeMax = nTemp;
							}
						}
					}
				}
			}

			if ( fontFile && *fontFile )
			{
				g_pSurface->AddCustomFontFile( pszName, fontFile );

				if ( bUseRange )
				{
					SetFontRange( pszName, nRangeMin, nRangeMax );
				}
			}
		}
	}

	// add bitmap fonts
	for (KeyValues *kv = m_pData->FindKey("BitmapFontFiles", true)->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		const char *fontFile = kv->GetString();
		if (fontFile && *fontFile)
		{
			bool bSuccess = g_pSurface->AddBitmapFontFile( fontFile );
			if ( bSuccess )
			{
				// refer to the font by a user specified symbol
				g_pSurface->SetBitmapFontName( kv->GetName(), fontFile );
			}
		}
	}

	// create the fonts
	for (KeyValues *kv = m_pData->FindKey("Fonts", true)->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		for ( int i = 0; i < 2; i++ )
		{
			// create the base font
			bool proportionalFont = static_cast<bool>( i );
			const char *fontName = GetMungedFontName( kv->GetName(), tag, proportionalFont ); // first time it adds a normal font, and then a proportional one
			HFont font = g_pSurface->CreateFont();

			int j = m_FontAliases.Insert( fontName );
			m_FontAliases[j]._trueFontName = kv->GetName();
			m_FontAliases[j]._font = font;
			m_FontAliases[j].m_bProportional = proportionalFont;
		}
	}

	// load in the font glyphs
	ReloadFontGlyphs();
}

//-----------------------------------------------------------------------------
// Purpose: Reloads all scheme fonts
//-----------------------------------------------------------------------------
void CScheme::ReloadFontGlyphs()
{
	COM_TimestampedLog( "ReloadFontGlyphs(): Start" );

	// get our current resolution
	if ( m_SizingPanel != 0 )
	{
		g_pIPanel->GetSize( m_SizingPanel, m_nScreenWide, m_nScreenTall );
	}
	else
	{
		g_pSurface->GetScreenSize( m_nScreenWide, m_nScreenTall );
	}

	// check our language; some have minimum sizes
	int minimumFontHeight = GetMinimumFontHeightForCurrentLanguage();

	// add the data to all the fonts
	KeyValues *fonts = m_pData->FindKey("Fonts", true);
	FOR_EACH_DICT_FAST( m_FontAliases, i )
	{
		KeyValues *kv = fonts->FindKey( m_FontAliases[i]._trueFontName.String(), true );
	
		// walk through creating adding the first matching glyph set to the font
		for (KeyValues *fontdata = kv->GetFirstSubKey(); fontdata != NULL; fontdata = fontdata->GetNextKey())
		{
			// skip over fonts not meant for this resolution
			int fontYResMin = 0, fontYResMax = 0;
			sscanf(fontdata->GetString("yres", ""), "%d %d", &fontYResMin, &fontYResMax);
			if (fontYResMin)
			{
				if (!fontYResMax)
				{
					fontYResMax = fontYResMin;
				}
				// check the range
				if (m_nScreenTall < fontYResMin || m_nScreenTall > fontYResMax)
					continue;
			}

			int flags = 0;
			if (fontdata->GetInt( "italic" ))
			{
				flags |= ISurface::FONTFLAG_ITALIC;
			}
			if (fontdata->GetInt( "underline" ))
			{
				flags |= ISurface::FONTFLAG_UNDERLINE;
			}
			if (fontdata->GetInt( "strikeout" ))
			{
				flags |= ISurface::FONTFLAG_STRIKEOUT;
			}
			if (fontdata->GetInt( "symbol" ))
			{
				flags |= ISurface::FONTFLAG_SYMBOL;
			}
			if (fontdata->GetInt( "antialias" ) && g_pSurface->SupportsFeature(ISurface::ANTIALIASED_FONTS))
			{
				flags |= ISurface::FONTFLAG_ANTIALIAS;
			}
			if (fontdata->GetInt( "dropshadow" ) && g_pSurface->SupportsFeature(ISurface::DROPSHADOW_FONTS))
			{
				flags |= ISurface::FONTFLAG_DROPSHADOW;
			}
			if (fontdata->GetInt( "outline" ) && g_pSurface->SupportsFeature(ISurface::OUTLINE_FONTS))
			{
				flags |= ISurface::FONTFLAG_OUTLINE;
			}
			if (fontdata->GetInt( "custom" ))
			{
				flags |= ISurface::FONTFLAG_CUSTOM;
			}
			if (fontdata->GetInt( "bitmap" ))
			{
				flags |= ISurface::FONTFLAG_BITMAP;
			}
			if (fontdata->GetInt( "rotary" ))
			{
				flags |= ISurface::FONTFLAG_ROTARY;
			}
			if (fontdata->GetInt( "additive" ))
			{
				flags |= ISurface::FONTFLAG_ADDITIVE;
			}

			int tall = fontdata->GetInt( "tall" );
			int blur = fontdata->GetInt( "blur" );
			int scanlines = fontdata->GetInt( "scanlines" );
			float scalex = fontdata->GetFloat( "scalex", 1.0f );
			float scaley = fontdata->GetFloat( "scaley", 1.0f );

			// only grow this font if it doesn't have a resolution filter specified
			if ( ( !fontYResMin && !fontYResMax ) && m_FontAliases[i].m_bProportional )
			{
				tall = g_Scheme.GetProportionalScaledValueEx( this, tall );
				blur = g_Scheme.GetProportionalScaledValueEx( this, blur );
				scanlines = g_Scheme.GetProportionalScaledValueEx( this, scanlines ); 
				scalex = g_Scheme.GetProportionalScaledValueEx( this, scalex * 10000.0f ) * 0.0001f;
				scaley = g_Scheme.GetProportionalScaledValueEx( this, scaley * 10000.0f ) * 0.0001f;
			}

			// clip the font size so that fonts can't be too big
			if ( tall > 127 )
			{
				tall = 127;
			}

			// check our minimum font height
			if ( tall < minimumFontHeight )
			{
				tall = minimumFontHeight;
			}
			
			if ( flags & ISurface::FONTFLAG_BITMAP )
			{
				// add the new set
				g_pSurface->SetBitmapFontGlyphSet(
					m_FontAliases[i]._font,
					g_pSurface->GetBitmapFontName( fontdata->GetString( "name" ) ), 
					scalex,
					scaley,
					flags);
			}
			else
			{
				int nRangeMin, nRangeMax;

				if ( GetFontRange( fontdata->GetString( "name" ), nRangeMin, nRangeMax ) )
				{
					// add the new set
					g_pSurface->SetFontGlyphSet(
						m_FontAliases[i]._font,
						fontdata->GetString( "name" ), 
						tall, 
						fontdata->GetInt( "weight" ), 
						blur,
						scanlines,
						flags,
						nRangeMin,
						nRangeMax);					
				}
				else
				{
					// add the new set
					g_pSurface->SetFontGlyphSet(
						m_FontAliases[i]._font,
						fontdata->GetString( "name" ), 
						tall, 
						fontdata->GetInt( "weight" ), 
						blur,
						scanlines,
						flags);
				}
			}

			// don't add any more
			break;
		}
	}

	COM_TimestampedLog( "ReloadFontGlyphs(): End" );
}

//-----------------------------------------------------------------------------
// Purpose: create the Border objects from the scheme data
//-----------------------------------------------------------------------------
void CScheme::LoadBorders()
{
	m_pkvBorders = m_pData->FindKey("Borders", true);
	{for ( KeyValues *kv = m_pkvBorders->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		if (kv->GetDataType() == KeyValues::TYPE_STRING)
		{
			// it's referencing another border, ignore for now
		}
		else
		{
			int i = m_BorderList.AddToTail();

			IBorder *border = NULL;
			const char *pszBorderType = kv->GetString( "bordertype", NULL );
			if ( pszBorderType && pszBorderType[0] )
			{
				if ( !stricmp(pszBorderType,"image") )
				{
					border = new ImageBorder();
				}
				else if ( !stricmp(pszBorderType,"scalable_image") )
				{
					border = new ScalableImageBorder();
				}
				else
				{
					Assert(0);
					// Fall back to the base border type. See below.
					pszBorderType = NULL;
				}
			}

			if ( !pszBorderType || !pszBorderType[0] )
			{
				border = new Border();
			}

			border->SetName(kv->GetName());
			border->ApplySchemeSettings(this, kv);

			m_BorderList[i].border = border;
			m_BorderList[i].bSharedBorder = false;
			m_BorderList[i].borderSymbol = kv->GetNameSymbol();
		}
	}}

	// iterate again to get the border references
	for ( KeyValues *kv = m_pkvBorders->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		if (kv->GetDataType() == KeyValues::TYPE_STRING)
		{
			// it's referencing another border, find it
			Border *border = (Border *)GetBorder(kv->GetString());
//			const char *str = kv->GetName();
			Assert(border);

			// add an entry that just references the existing border
			int i = m_BorderList.AddToTail();
			m_BorderList[i].border = border;
			m_BorderList[i].bSharedBorder = true;
			m_BorderList[i].borderSymbol = kv->GetNameSymbol();
		}
	}
	
	m_pBaseBorder = GetBorder("BaseBorder");
}

void CScheme::SpewFonts( void )
{
	Msg( "Scheme: %s (%s)\n", GetName(), GetFileName() );
	FOR_EACH_DICT_FAST( m_FontAliases, i )
	{
		const fontalias_t& FontAlias = m_FontAliases[ i ];
		uint32 Font = FontAlias._font;
		const char *szFontName = g_pSurface->GetFontName( Font );
		const char *szFontFamilyName = g_pSurface->GetFontFamilyName( Font );
		const char *szTrueFontName = FontAlias._trueFontName.String();
		const char *szFontAlias = m_FontAliases.GetElementName( i );

		Msg( "  %2d: HFont:0x%8.8x, %s, %s, font:%s, tall:%d(%d). %s\n", 
			i, 
			Font,
			szTrueFontName ? szTrueFontName : "??", 
			szFontAlias ? szFontAlias : "??",
			szFontName ? szFontName : "??", 
			g_pSurface->GetFontTall( Font ),
			g_pSurface->GetFontTallRequested( Font ),
			szFontFamilyName ? szFontFamilyName : "" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: reloads the scheme from the file
//-----------------------------------------------------------------------------
void CSchemeManager::ReloadSchemes()
{
	int count = m_Schemes.Count();
	Shutdown( false );
	
	// reload the scheme
	for (int i = 1; i < count; i++)
	{
		LoadSchemeFromFile(m_Schemes[i]->GetFileName(), m_Schemes[i]->GetName());
	}
}

//-----------------------------------------------------------------------------
// Purpose: kills all the schemes
//-----------------------------------------------------------------------------
void CScheme::Shutdown( bool full )
{
	for (int i = 0; i < m_BorderList.Count(); i++)
	{
		// delete if it's not shared
		if (!m_BorderList[i].bSharedBorder)
		{
			IBorder *border = m_BorderList[i].border;
			delete border;
		}
	}

	m_pBaseBorder = NULL;
	m_BorderList.RemoveAll();
	m_pkvBorders = NULL;

	if (full && m_pData)
	{
		m_pData->deleteThis();
		m_pData = NULL;
		delete this;
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns a handle to the default (first loaded) scheme
//-----------------------------------------------------------------------------
HScheme CSchemeManager::GetDefaultScheme()
{
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: returns a handle to the scheme identified by "tag"
//-----------------------------------------------------------------------------
HScheme CSchemeManager::GetScheme(const char *tag)
{
	for (int i=1;i<m_Schemes.Count();i++)
	{
		if ( !stricmp(tag,m_Schemes[i]->GetName()) )
		{
			return i;
		}
	}
	return 1; // default scheme
}

int CSchemeManager::GetProportionalScaledValue_( int rootWide, int rootTall, int normalizedValue )
{
	int proH, proW;
	g_pSurface->GetProportionalBase( proW, proH );
	double scale = (double)rootTall / (double)proH;

	return (int)( normalizedValue * scale );
}

int CSchemeManager::GetProportionalNormalizedValue_( int rootWide, int rootTall, int scaledValue )
{
	int proH, proW;
	g_pSurface->GetProportionalBase( proW, proH );
	float scale = (float)rootTall / (float)proH;

	return (int)( scaledValue / scale );
}

//-----------------------------------------------------------------------------
// Purpose: converts a value into proportional mode
//-----------------------------------------------------------------------------
int CSchemeManager::GetProportionalScaledValue(int normalizedValue)
{
	int wide, tall;
	g_pSurface->GetScreenSize( wide, tall );
	return GetProportionalScaledValue_( wide, tall, normalizedValue );
}

//-----------------------------------------------------------------------------
// Purpose: converts a value out of proportional mode
//-----------------------------------------------------------------------------
int CSchemeManager::GetProportionalNormalizedValue(int scaledValue)
{
	int wide, tall;
	g_pSurface->GetScreenSize( wide, tall );
	return GetProportionalNormalizedValue_( wide, tall, scaledValue );
}

// gets the proportional coordinates for doing screen-size independant panel layouts
// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
int CSchemeManager::GetProportionalScaledValueEx( CScheme *pScheme, int normalizedValue )
{
	VPANEL sizing = pScheme->GetSizingPanel();
	if ( !sizing )
	{
		return GetProportionalScaledValue( normalizedValue );
	}

	int w, h;
	g_pIPanel->GetSize( sizing, w, h );
	return GetProportionalScaledValue_( w, h, normalizedValue );
}

int CSchemeManager::GetProportionalNormalizedValueEx( CScheme *pScheme, int scaledValue )
{
	VPANEL sizing = pScheme->GetSizingPanel();
	if ( !sizing )
	{
		return GetProportionalNormalizedValue( scaledValue );
	}

	int w, h;
	g_pIPanel->GetSize( sizing, w, h );
	return GetProportionalNormalizedValue_( w, h, scaledValue );
}

int CSchemeManager::GetProportionalScaledValueEx( HScheme scheme, int normalizedValue )
{
	IScheme *pscheme = GetIScheme( scheme );
	if ( !pscheme )
	{
		Assert( 0 );
		return GetProportionalScaledValue( normalizedValue );
	}

	CScheme *p = static_cast< CScheme * >( pscheme );
	return GetProportionalScaledValueEx( p, normalizedValue );
}

int CSchemeManager::GetProportionalNormalizedValueEx( HScheme scheme, int scaledValue )
{
	IScheme *pscheme = GetIScheme( scheme );
	if ( !pscheme )
	{
		Assert( 0 );
		return GetProportionalNormalizedValue( scaledValue );
	}

	CScheme *p = static_cast< CScheme * >( pscheme );
	return GetProportionalNormalizedValueEx( p, scaledValue );
}

void CSchemeManager::SpewFonts( void )
{
	for ( int i = 1; i < m_Schemes.Count(); i++ )
	{
		m_Schemes[i]->SpewFonts();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CScheme::GetResourceString(const char *stringName)
{
	return m_pkvBaseSettings->GetString(stringName);
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to an image
//-----------------------------------------------------------------------------
IImage *CSchemeManager::GetImage(const char *imageName, bool hardwareFiltered)
{
	if ( !imageName || strlen(imageName) <= 0 ) // frame icons and the like are in the scheme file and may not be defined, so if this is null then fail silently
	{
		return NULL; 
	}

	// set up to search for the bitmap
	CachedBitmapHandle_t searchBitmap;
	searchBitmap.pBitmap = NULL;

	// Prepend 'vgui/'. Resource files try to load images assuming they live in the vgui directory.
	// Used to do this in Bitmap::Bitmap, moved so that the s_pszSearchString is searching for the
	// filename with 'vgui/' already added.
	char szFileName[MAX_PATH];

	//if ( Q_IsAbsolutePath(imageName) )
	//{
	//	Q_strncpy( szFileName, imageName, sizeof(szFileName) );
	//}
	//else
	{
		if ( Q_stristr( imageName, ".pic" ) )
		{
			Q_snprintf( szFileName, sizeof(szFileName), "%s", imageName );
		}
		else
		{
			Q_snprintf( szFileName, sizeof(szFileName), "vgui/%s", imageName );
		}
	}

	s_pszSearchString = szFileName;
	int i = m_Bitmaps.Find( searchBitmap );
	if (m_Bitmaps.IsValidIndex( i ) )
	{
		return m_Bitmaps[i].pBitmap;
	}

	// couldn't find the image, try and load it
	CachedBitmapHandle_t hBitmap = { new Bitmap( szFileName, hardwareFiltered ) };
	m_Bitmaps.Insert( hBitmap );
	return hBitmap.pBitmap;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
HTexture CSchemeManager::GetImageID(const char *imageName, bool hardwareFiltered)
{
	IImage *img = GetImage(imageName, hardwareFiltered);
	return ((Bitmap *)img)->GetID();
}

//-----------------------------------------------------------------------------
// Delete a managed image
//-----------------------------------------------------------------------------
bool CSchemeManager::DeleteImage( const char *pImageName )
{
	if ( !pImageName )
	{
		// nothing to do
		return false;
	}

	// set up to search for the bitmap
	CachedBitmapHandle_t searchBitmap;
	searchBitmap.pBitmap = NULL;

	// Prepend 'vgui/'. Resource files try to load images assuming they live in the vgui directory.
	// Used to do this in Bitmap::Bitmap, moved so that the s_pszSearchString is searching for the
	// filename with 'vgui/' already added.
	char szFileName[256];
	if ( Q_stristr( pImageName, ".pic" ) )
	{
		Q_snprintf( szFileName, sizeof(szFileName), "%s", pImageName );
	}
	else
	{
		Q_snprintf( szFileName, sizeof(szFileName), "vgui/%s", pImageName );
	}
	s_pszSearchString = szFileName;

	int i = m_Bitmaps.Find( searchBitmap );
	if ( !m_Bitmaps.IsValidIndex( i ) )
	{
		// not found
		return false;
	}
		
	// no way to know if eviction occured, assume it does
	m_Bitmaps[i].pBitmap->Evict();
	delete  m_Bitmaps[i].pBitmap;	
	m_Bitmaps.RemoveAt( i );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to an existing border
//-----------------------------------------------------------------------------
IBorder *CScheme::GetBorder(const char *borderName)
{
	int symbol = KeyValuesSystem()->GetSymbolForString(borderName);
	for (int i = 0; i < m_BorderList.Count(); i++)
	{
		if (m_BorderList[i].borderSymbol == symbol)
		{
			return m_BorderList[i].border;
		}
	}

	return m_pBaseBorder;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of borders
//-----------------------------------------------------------------------------
int CScheme::GetBorderCount() const
{
	return m_BorderList.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Get the border at the given index
//-----------------------------------------------------------------------------
IBorder *CScheme::GetBorderAtIndex( int iIndex )
{
	if ( !m_BorderList.IsValidIndex( iIndex ) )
		return NULL;

	return m_BorderList[ iIndex ].border;
}

//-----------------------------------------------------------------------------
// Finds a font in the alias list
//-----------------------------------------------------------------------------
HFont CScheme::FindFontInAliasList( const char *fontName )
{
	int i = m_FontAliases.Find( fontName );
	if ( i != m_FontAliases.InvalidIndex() )
	{
		return m_FontAliases[i]._font;
	}

	// No dice
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : font - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CScheme::GetFontName( const HFont& font )
{
	for (int i = m_FontAliases.Count(); --i >= 0; )
	{
		HFont fnt = (HFont)m_FontAliases[i]._font;
		if ( fnt == font )
			return m_FontAliases[i]._trueFontName.String();
	}

	return "<Unknown font>";
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to an existing font, proportional=false means use default
//-----------------------------------------------------------------------------
HFont CScheme::GetFont( const char *fontName, bool proportional )
{
	// First look in the list of aliases...
	return FindFontInAliasList( GetMungedFontName( fontName, tag, proportional ) );
}

//-----------------------------------------------------------------------------
// Purpose:Get the number of fonts
//-----------------------------------------------------------------------------
int CScheme::GetFontCount() const
{
	return m_FontAliases.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Get the font at the given index
//-----------------------------------------------------------------------------
HFont CScheme::GetFontAtIndex( int iIndex )
{
	if ( !m_FontAliases.IsValidIndex( iIndex ) )
		return INVALID_FONT;
	
	return m_FontAliases[ iIndex ]._font;
}

//-----------------------------------------------------------------------------
// Purpose: returns a char string of the munged name this font is stored as in the font manager
//-----------------------------------------------------------------------------
const char *CScheme::GetMungedFontName( const char *fontName, const char *scheme, bool proportional )
{
	static char mungeBuffer[ 64 ];
	if ( scheme )
	{
		Q_snprintf( mungeBuffer, sizeof( mungeBuffer ), "%s%s-%s", fontName, scheme, proportional ? "p" : "no" );
	}
	else
	{ 
		Q_snprintf( mungeBuffer, sizeof( mungeBuffer ), "%s-%s", fontName, proportional ? "p" : "no" ); // we don't want the "(null)" snprintf appends
	}
	return mungeBuffer;
}

//-----------------------------------------------------------------------------
// Purpose: Gets a color from the scheme file
//-----------------------------------------------------------------------------
Color CScheme::GetColor(const char *colorName, Color defaultColor)
{
	const char *pchT = LookupSchemeSetting(colorName);
	if (!pchT)
		return defaultColor;

	int r = 0, g = 0, b = 0, a = 0;
	if (sscanf(pchT, "%d %d %d %d", &r, &g, &b, &a) >= 3)
		return Color(r, g, b, a);

	return defaultColor;
}

//-----------------------------------------------------------------------------
// Purpose: Get the color at the given index
//-----------------------------------------------------------------------------
const KeyValues *CScheme::GetColorData() const
{
	return m_pkvColors;
}

//-----------------------------------------------------------------------------
// Purpose: recursively looks up a setting
//-----------------------------------------------------------------------------
const char *CScheme::LookupSchemeSetting(const char *pchSetting)
{
	// try parse out the color
	int r, g, b, a = 0;
	int res = sscanf(pchSetting, "%d %d %d %d", &r, &g, &b, &a);
	if (res >= 3)
	{
		return pchSetting;
	}

	// check the color area first
	const char *colStr = m_pkvColors->GetString(pchSetting, NULL);
	if (colStr)
		return colStr;

	// check base settings
	colStr = m_pkvBaseSettings->GetString(pchSetting, NULL);
	if (colStr)
	{
		return LookupSchemeSetting(colStr);
	}

	return pchSetting;
}

//-----------------------------------------------------------------------------
// Purpose: gets the minimum font height for the current language
//-----------------------------------------------------------------------------
int CScheme::GetMinimumFontHeightForCurrentLanguage()
{
	char language[64];
	bool bValid;
	if ( IsPC() )
	{
		bValid = vgui::g_pSystem->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Source\\Language", language, sizeof(language)-1 );
	}
	else
	{
		Q_strncpy( language, XBX_GetLanguageString(), sizeof( language ) );
		bValid = true;
	}

	if ( bValid )
	{
		if (!stricmp(language, "korean")
			|| !stricmp(language, "tchinese")
			|| !stricmp(language, "schinese")
			|| !stricmp(language, "japanese"))
		{
			// the bitmap-based fonts for these languages simply don't work with a pt. size of less than 9 (13 pixels)
			return 13;
		}

		if ( !stricmp(language, "thai" ) )
		{
			// thai has problems below 18 pts
			return 18;
		}
	}

	// no special minimum height required
	return 0;
}
