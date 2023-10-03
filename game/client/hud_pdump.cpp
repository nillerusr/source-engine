//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud_pdump.h"
#include "iclientmode.h"
#include "predictioncopy.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include "vgui_int.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

CPDumpPanel *GetPDumpPanel()
{
	return GET_FULLSCREEN_HUDELEMENT( CPDumpPanel );
}

DECLARE_HUDELEMENT_FLAGS( CPDumpPanel, HUDELEMENT_SS_FULLSCREEN_ONLY );

CPDumpPanel::CPDumpPanel( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass( NULL, "HudPredictionDump" ), m_nCurrentIndex( 0 )
{
	vgui::Panel *pParent = GetFullscreenClientMode()->GetViewport();
	SetParent( pParent );

	SetProportional( false );
	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
}

CPDumpPanel::~CPDumpPanel()
{
}

void CPDumpPanel::ApplySettings( KeyValues *inResourceData )
{
	SetProportional( false );

	BaseClass::ApplySettings( inResourceData );
}

void CPDumpPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	SetProportional( false );

	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundEnabled( false );

	int screenWide, screenTall;
	VGui_GetTrueScreenSize(screenWide, screenTall);
	SetBounds(0, 0, screenWide, screenTall);
	// Make sure we sort above everyone else
	SetZPos( 100 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPDumpPanel::ShouldDraw()
{
	if ( m_DumpEntityInfo.Count() == 0 )
		return false;

	return CHudElement::ShouldDraw();
}

static char const *pchButtonFields[]=
{
	"m_nOldButtons",
	"m_nButtons",
	"m_afButtonLast",
	"m_afButtonPressed",
	"m_afButtonReleased",
	"m_afButtonForced",
};

static bool IsButtonField( char const *fieldname )
{
	for ( int i =0 ; i < ARRAYSIZE( pchButtonFields ); ++i )
	{
		if ( !Q_stricmp( fieldname, pchButtonFields[ i ] ) )
			return true;
	}
	return false;
}

struct buttonname_t
{
	int			nBit;
	char const	*pchName;
};

#define DECLARE_BUTTON_NAME( x ) { IN_##x, #x }

static buttonname_t g_ButtonNames[] =
{
	DECLARE_BUTTON_NAME( ATTACK ),
	DECLARE_BUTTON_NAME( JUMP ),
	DECLARE_BUTTON_NAME( DUCK ),
	DECLARE_BUTTON_NAME( FORWARD ),
	DECLARE_BUTTON_NAME( BACK ),
	DECLARE_BUTTON_NAME( USE ),
	DECLARE_BUTTON_NAME( CANCEL ),
	DECLARE_BUTTON_NAME( LEFT ),
	DECLARE_BUTTON_NAME( RIGHT ),
	DECLARE_BUTTON_NAME( MOVELEFT ),
	DECLARE_BUTTON_NAME( MOVERIGHT ),
	DECLARE_BUTTON_NAME( ATTACK2 ),
	DECLARE_BUTTON_NAME( RUN ),
	DECLARE_BUTTON_NAME( RELOAD ),
	DECLARE_BUTTON_NAME( ALT1 ),
	DECLARE_BUTTON_NAME( ALT2 ),
	DECLARE_BUTTON_NAME( SCORE ),
	DECLARE_BUTTON_NAME( SPEED),
	DECLARE_BUTTON_NAME( WALK ),
	DECLARE_BUTTON_NAME( ZOOM ),
	DECLARE_BUTTON_NAME( WEAPON1 ),
	DECLARE_BUTTON_NAME( WEAPON2 ),
	DECLARE_BUTTON_NAME( BULLRUSH ),
	DECLARE_BUTTON_NAME( GRENADE1 ),
	DECLARE_BUTTON_NAME( GRENADE2 ),
	DECLARE_BUTTON_NAME( LOOKSPIN ),
};


static char const *GetButtonFieldValue( char const *value, char *buf, size_t bufsize )
{
	buf[ 0 ] = 0;
	char *pchDataStart = Q_strstr( value, "(" );
	if ( !pchDataStart )
		return value;

	int bits = Q_atoi( pchDataStart + 1 );

	// Assign button bits
	bool first = true;
	for ( int i = 0; i < ARRAYSIZE( g_ButtonNames ); ++i )
	{
		int mask = (1<<i);
		if ( bits & mask )
		{
			if ( !first )
			{
				Q_strncat( buf, ",", bufsize, COPY_ALL_CHARACTERS );
			}
			Q_strncat( buf, g_ButtonNames[ i ].pchName, bufsize, COPY_ALL_CHARACTERS );
			first = false;
		}
	}

	Q_strlower( buf );
	return buf;
}

static char const *CleanupZeros( char const *value, char *buf, size_t bufsize )
{
	char *out = buf;
	while ( *value )
	{
		if ( *value != '.' )
		{
			*out++ = *value++;
			continue;
		}

		// Found a . now see if next run of characters until space or ')' is all zeroes
		char const *next = value + 1;
		while ( *next && *next == '0' )
			++next;
		if ( *next == ' ' || *next == ')' )
		{
			// Don't write the . or the zeroes, just put value at the terminator
			value = next;
		}
		else
		{
			*out++ = *value++;
		}
	}

	*out = 0;
	return buf;
}

void CPDumpPanel::DumpComparision( const char *classname, const char *fieldname, const char *fieldtype,
	bool networked, bool noterrorchecked, bool differs, bool withintolerance, const char *value )
{
	if ( fieldname == NULL )
		return;

	DumpInfo slot;

	slot.index = m_nCurrentIndex++;

	Q_snprintf( slot.classname, sizeof( slot.classname ), "%s", classname );
	slot.networked = networked;

	char bv[ DUMP_STRING_SIZE ];

	if ( IsButtonField( fieldname ) )
	{
		value = GetButtonFieldValue( value, bv, sizeof( bv ) );
	}
	else
	{
		value = CleanupZeros( value, bv, sizeof( bv ) );
	}

	Q_snprintf( slot.fieldstring, sizeof( slot.fieldstring ), "%s %s",
		fieldname,
		value );

	slot.differs = differs;
	slot.withintolerance = withintolerance;
	slot.noterrorchecked = noterrorchecked;

	m_DumpEntityInfo.InsertNoSort( slot );
}

//-----------------------------------------------------------------------------
// Purpose: Callback function for dumping entity info to screen
// Input  : *classname - 
//			*fieldname - 
//			*fieldtype - 
//			networked - 
//			noterrorchecked - 
//			differs - 
//			withintolerance - 
//			*value - 
// Output : static void
//-----------------------------------------------------------------------------
static void DumpComparision( const char *classname, const char *fieldname, const char *fieldtype,
	bool networked, bool noterrorchecked, bool differs, bool withintolerance, const char *value )
{
	CPDumpPanel *pPanel = GetPDumpPanel();
	if ( !pPanel )
		return;

	pPanel->DumpComparision( classname, fieldname, fieldtype, networked, noterrorchecked, differs, withintolerance, value );
}

//-----------------------------------------------------------------------------
// Purpose: Lookup color to use for data
// Input  : networked - 
//			errorchecked - 
//			differs - 
//			withintolerance - 
//			r - 
//			g - 
//			b - 
//			a - 
// Output : static void
//-----------------------------------------------------------------------------
void CPDumpPanel::PredictionDumpColor( bool legend, bool predictable, bool networked, bool errorchecked, bool differs, bool withintolerance,
	int& r, int& g, int& b, int& a )
{
	if ( !legend && !predictable )
	{
		r = 150;
		g = 180;
		b = 150;
		a = 255;
		return;
	}

	r = 255;
	g = 255;
	b = 255;
	a = 255;

	if ( networked )
	{
		if ( errorchecked )
		{
			r = 180;
			g = 180;
			b = 225;
		}
		else
		{
			r = 150;
			g = 180;
			b = 150;
		}
	}

	if ( differs )
	{
		if ( withintolerance )
		{
			r = 255;
			g = 255;
			b = 0;
			a = 255;
		}
		else
		{
			if ( !networked )
			{
				r = 180;
				g = 180;
				b = 100;
				a = 255;
			}
			else
			{
				r = 255;
				g = 0;
				b = 0;
				a = 255;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Dump entity data to screen
// Input  : *ent - 
//			last_predicted - 
//-----------------------------------------------------------------------------
void CPDumpPanel::DumpEntity( C_BaseEntity *ent, int commands_acknowledged )
{
	if ( IsXbox() )
	{
		return;
	}

#ifdef NO_ENTITY_PREDICTION
	return;
#else
	Assert( ent );

	const byte *original_state_data = NULL;	
	const byte *predicted_state_data	= NULL;
	
	bool data_type_original		= TD_OFFSET_PACKED;
	bool data_type_predicted	= TD_OFFSET_PACKED;

	if ( ent->GetPredictable() )
	{
		original_state_data		= (const byte *)ent->GetOriginalNetworkDataObject();	
		predicted_state_data	= (const byte *)ent->GetPredictedFrame( commands_acknowledged - 1 );	
	}
	else
	{
		// Compare against self so that we're just dumping data to screen
		original_state_data = ( const byte * )ent;
		data_type_original = TD_OFFSET_NORMAL;
		predicted_state_data = original_state_data;
		data_type_predicted = data_type_original;
	}

	Assert( original_state_data );
	Assert( predicted_state_data );

	Clear();

	CPredictionCopy datacompare( PC_EVERYTHING, 
		(byte *)original_state_data, data_type_original, 
		predicted_state_data, data_type_predicted, 
		CPredictionCopy::TRANSFERDATA_ERRORCHECK_DESCRIBE,
		::DumpComparision );
	// Don't spew debugging info
	m_nCurrentIndex = 0;
	datacompare.TransferData( "", ent->entindex(), ent->GetPredDescMap() );
	m_hDumpEntity = ent;
	m_DumpEntityInfo.RedoSort();
#endif
}

void CPDumpPanel::Clear()
{
	m_DumpEntityInfo.RemoveAll();
}

void CPDumpPanel::Paint()
{
	C_BaseEntity *ent = m_hDumpEntity;
	if ( !ent )
	{
		Clear();
		return;
	}

	bool bPredictable = ent->GetPredictable();

	// Now output the strings
	int x[5];
	x[0] = 20;
	int columnwidth = 375;
	int numcols = GetWide() / columnwidth;
	int i;

	numcols = clamp( numcols, 1, 5 );

	for ( i = 0; i < numcols; i++ )
	{
		if ( i == 0 )
		{
			x[i] = 20;
		}
		else
		{
			x[i] = x[ i-1 ] + columnwidth - 20;
		}
	}

	int nFontTweak = -7;

	int c = m_DumpEntityInfo.Count();
	int fonttall = vgui::surface()->GetFontTall( m_FontSmall ) + nFontTweak;
	int fonttallMedium = vgui::surface()->GetFontTall( m_FontMedium ) + nFontTweak;
	int fonttallBig = vgui::surface()->GetFontTall( m_FontBig ) + nFontTweak;

	char currentclass[ 128 ];
	currentclass[ 0 ] = 0;

	int starty = 15;
	int y = starty;

	int col = 0;

	int r = 255;
	int g = 255;
	int b = 255;
	int a = 255;

	char classextra[ 32 ];
	classextra[ 0 ] = 0;
	char classprefix[ 32 ];
	Q_strncpy( classprefix, "class ", sizeof( classprefix ) );
	const char *classname = ent->GetClassname();
	if ( !classname[ 0 ] )
	{
		classname = typeid( *ent ).name();
		Q_strncpy( classextra, " (classmap missing)", sizeof( classextra ) );
		classprefix[ 0 ] = 0;
	}

	char sz[ 512 ];
	wchar_t szconverted[ 1024 ];

	surface()->DrawSetTextFont( m_FontBig );
	surface()->DrawSetTextColor( Color( 255, 255, 255, 255 ) );
	surface()->DrawSetTextPos( x[ col ] - 10, y - fonttallBig - 2 );
	Q_snprintf( sz, sizeof( sz ), "entity # %i: %s%s%s", ent->entindex(), classprefix, classname, classextra );
	g_pVGuiLocalize->ConvertANSIToUnicode( sz, szconverted, sizeof(szconverted)  );
	surface()->DrawPrintText( szconverted, wcslen( szconverted ) );

	for ( i = 0; i < c; i++ )
	{
		DumpInfo *slot = &m_DumpEntityInfo[ i ];

		if ( stricmp( slot->classname, currentclass ) )
		{
			y += 2;

			surface()->DrawSetTextFont( m_FontMedium );
			surface()->DrawSetTextColor( Color( 0, 255, 100, 255 ) );
			surface()->DrawSetTextPos( x[ col ] - 10, y );
			Q_snprintf( sz, sizeof( sz ), "%s", slot->classname );
			g_pVGuiLocalize->ConvertANSIToUnicode( sz, szconverted, sizeof(szconverted)  );
			surface()->DrawPrintText( szconverted, wcslen( szconverted ) );

			y += fonttallMedium;
			Q_strncpy( currentclass, slot->classname, sizeof( currentclass ) );
		}

	
		PredictionDumpColor( false, bPredictable, slot->networked, !slot->noterrorchecked, slot->differs, slot->withintolerance,
			r, g, b, a );

		surface()->DrawSetTextFont( m_FontSmall );
		surface()->DrawSetTextColor( Color( r, g, b, a ) );
		surface()->DrawSetTextPos( x[ col ], y );
		Q_snprintf( sz, sizeof( sz ), "%s", slot->fieldstring );
		g_pVGuiLocalize->ConvertANSIToUnicode( sz, szconverted, sizeof(szconverted)  );
		surface()->DrawPrintText( szconverted, wcslen( szconverted ) );

		y += fonttall;

		if ( y >= GetTall() - fonttall - starty )
		{
			y = starty;
			col++;
			if ( col >= numcols )
				break;
		}
	}

	surface()->DrawSetTextFont( m_FontSmall );


	// Figure how far over the legend needs to be.
	const char *pFirstAndLongestString = "Not networked, no differences";
	g_pVGuiLocalize->ConvertANSIToUnicode( pFirstAndLongestString, szconverted, sizeof(szconverted)  );
	int textSizeWide, textSizeTall;
	surface()->GetTextSize( m_FontSmall, szconverted, textSizeWide, textSizeTall );


	// Draw a legend now
	int xpos = ScreenWidth() - textSizeWide - 5;
	y = ScreenHeight() - 7 * fonttall - 80;

	// Not networked, no differences
	PredictionDumpColor( true, bPredictable, false, false, false, false, r, g, b, a );


	surface()->DrawSetTextColor( Color( r, g, b, a ) );
	surface()->DrawSetTextPos( xpos, y );
	Q_strncpy( sz, pFirstAndLongestString, sizeof( sz ) );
	g_pVGuiLocalize->ConvertANSIToUnicode( sz, szconverted, sizeof(szconverted)  );
	surface()->DrawPrintText( szconverted, wcslen( szconverted ) );

	y += fonttall;

	// Networked, no error check
	PredictionDumpColor( true, bPredictable, true, false, false, false, r, g, b, a );

	surface()->DrawSetTextColor( Color( r, g, b, a ) );
	surface()->DrawSetTextPos( xpos, y );
	Q_strncpy( sz, "Networked, not checked", sizeof( sz ) );
	g_pVGuiLocalize->ConvertANSIToUnicode( sz, szconverted, sizeof(szconverted)  );
	surface()->DrawPrintText( szconverted, wcslen( szconverted ) );

	y += fonttall;

	// Networked, with error check
	PredictionDumpColor( true, bPredictable, true, true, false, false, r, g, b, a );

	surface()->DrawSetTextColor( Color( r, g, b, a ) );
	surface()->DrawSetTextPos( xpos, y );
	Q_strncpy( sz, "Networked, error checked", sizeof( sz ) );
	g_pVGuiLocalize->ConvertANSIToUnicode( sz, szconverted, sizeof(szconverted)  );
	surface()->DrawPrintText( szconverted, wcslen( szconverted ) );

	y += fonttall;

	// Differs, but within tolerance
	PredictionDumpColor( true, bPredictable, true, true, true, true, r, g, b, a );

	surface()->DrawSetTextColor( Color( r, g, b, a ) );
	surface()->DrawSetTextPos( xpos, y );
	Q_strncpy( sz, "Differs, but within tolerance", sizeof( sz ) );
	g_pVGuiLocalize->ConvertANSIToUnicode( sz, szconverted, sizeof(szconverted)  );
	surface()->DrawPrintText( szconverted, wcslen( szconverted ) );

	y += fonttall;

	// Differs, not within tolerance, but not networked
	PredictionDumpColor( true, bPredictable, false, true, true, false, r, g, b, a );

	surface()->DrawSetTextColor( Color( r, g, b, a ) );
	surface()->DrawSetTextPos( xpos, y );
	Q_strncpy( sz, "Differs, but not networked", sizeof( sz ) );
	g_pVGuiLocalize->ConvertANSIToUnicode( sz, szconverted, sizeof(szconverted)  );
	surface()->DrawPrintText( szconverted, wcslen( szconverted ) );

	y += fonttall;

	// Differs, networked, not within tolerance
	PredictionDumpColor( true, bPredictable, true, true, true, false, r, g, b, a );

	surface()->DrawSetTextColor( Color( r, g, b, a ) );
	surface()->DrawSetTextPos( xpos, y );
	Q_strncpy( sz, "Differs, networked", sizeof( sz ) );
	g_pVGuiLocalize->ConvertANSIToUnicode( sz, szconverted, sizeof(szconverted)  );
	surface()->DrawPrintText( szconverted, wcslen( szconverted ) );

	y += fonttall;
}