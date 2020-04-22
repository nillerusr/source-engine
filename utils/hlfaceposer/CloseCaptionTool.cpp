//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include <stdio.h>
#include "hlfaceposer.h"
#include "CloseCaptionTool.h"
#include "choreowidgetdrawhelper.h"
#include <vgui/ILocalize.h>

extern vgui::ILocalize *g_pLocalize;

using namespace vgui;

CloseCaptionTool *g_pCloseCaptionTool = 0;

#define STREAM_FONT			"Tahoma"
#define STREAM_POINTSIZE	12
#define	STREAM_LINEHEIGHT ( STREAM_POINTSIZE + 2 )
#define STREAM_WEIGHT		FW_NORMAL

#define CAPTION_LINGER_TIME	1.5f
// FIXME: Yahn, what is this, it's coded up as a DELAY before when the closed caption is displayed. That seems odd.
#define CAPTION_PREDISPLAY_TIME	0.0f // 0.5f




// A work unit is a pre-processed chunk of CC text to display
// Any state changes (font/color/etc) cause a new work unit to be precomputed
// Moving onto a new line also causes a new Work Unit
// The width and height are stored so that layout can be quickly recomputed each frame
class CCloseCaptionWorkUnit
{
public:
	CCloseCaptionWorkUnit();
	~CCloseCaptionWorkUnit();

	void	SetWidth( int w );
	int		GetWidth() const;

	void	SetHeight( int h );
	int		GetHeight() const;

	void	SetPos( int x, int y );
	void	GetPos( int& x, int &y ) const;

	void	SetBold( bool bold );
	bool	GetBold() const;

	void	SetItalic( bool ital );
	bool	GetItalic() const;

	void	SetStream( const wchar_t *stream );
	const wchar_t	*GetStream() const;

	void	SetColor( COLORREF clr );
	COLORREF GetColor() const;

	int		GetFontNumber() const
	{
		return CloseCaptionTool::GetFontNumber( m_bBold, m_bItalic );
	}
	
	void Dump()
	{
		char buf[ 2048 ];
		g_pLocalize->ConvertUnicodeToANSI( GetStream(), buf, sizeof( buf ) );

		Msg( "x = %i, y = %i, w = %i h = %i text %s\n", m_nX, m_nY, m_nWidth, m_nHeight, buf );
	}

private:

	int				m_nX;
	int				m_nY;
	int				m_nWidth;
	int				m_nHeight;

	bool			m_bBold;
	bool			m_bItalic;
	wchar_t			*m_pszStream;
	COLORREF		m_Color;
};

CCloseCaptionWorkUnit::CCloseCaptionWorkUnit() :
	m_nWidth(0),
	m_nHeight(0),
	m_bBold(false),
	m_bItalic(false),
	m_pszStream(0),
	m_Color( RGB( 255, 255, 255 ) )
{
}

CCloseCaptionWorkUnit::~CCloseCaptionWorkUnit()
{
	delete[] m_pszStream;
	m_pszStream = NULL;
}

void CCloseCaptionWorkUnit::SetWidth( int w )
{
	m_nWidth = w;
}

int CCloseCaptionWorkUnit::GetWidth() const
{
	return m_nWidth;
}

void CCloseCaptionWorkUnit::SetHeight( int h )
{
	m_nHeight = h;
}

int CCloseCaptionWorkUnit::GetHeight() const
{
	return m_nHeight;
}

void CCloseCaptionWorkUnit::SetPos( int x, int y )
{
	m_nX = x;
	m_nY = y;
}

void CCloseCaptionWorkUnit::GetPos( int& x, int &y ) const
{
	x = m_nX;
	y = m_nY;
}

void CCloseCaptionWorkUnit::SetBold( bool bold )
{
	m_bBold = bold;
}

bool CCloseCaptionWorkUnit::GetBold() const
{
	return m_bBold;
}

void CCloseCaptionWorkUnit::SetItalic( bool ital )
{
	m_bItalic = ital;
}

bool CCloseCaptionWorkUnit::GetItalic() const
{
	return m_bItalic;
}

void CCloseCaptionWorkUnit::SetStream( const wchar_t *stream )
{
	delete[] m_pszStream;
	m_pszStream = NULL;

	int len = wcslen( stream );
	Assert( len < 4096 );
	m_pszStream = new wchar_t[ len + 1 ];
	wcsncpy( m_pszStream, stream, len );
	m_pszStream[ len ] = L'\0';
}

const wchar_t *CCloseCaptionWorkUnit::GetStream() const
{
	return m_pszStream ? m_pszStream : L"";
}

void CCloseCaptionWorkUnit::SetColor( COLORREF clr )
{
	m_Color = clr;
}

COLORREF CCloseCaptionWorkUnit::GetColor() const
{
	return m_Color;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCloseCaptionItem
{
public:
	CCloseCaptionItem( 
		wchar_t	*stream,
		float timetolive,
		float predisplay,
		bool valid
	) :
		m_flTimeToLive( 0.0f ),
		m_bValid( false ),
		m_nTotalWidth( 0 ),
		m_nTotalHeight( 0 ),
		m_bSizeComputed( false )
	{
		SetStream( stream );
		SetTimeToLive( timetolive );
		SetPreDisplayTime( CAPTION_PREDISPLAY_TIME + predisplay );
		m_bValid = valid;
		m_bSizeComputed = false;
	}

	CCloseCaptionItem( const CCloseCaptionItem& src )
	{
		SetStream( src.m_szStream );
		m_flTimeToLive = src.m_flTimeToLive;
		m_bValid = src.m_bValid;
	}

	~CCloseCaptionItem( void )
	{
		while ( m_Work.Count() > 0 )
		{
			CCloseCaptionWorkUnit *unit = m_Work[ 0 ];
			m_Work.Remove( 0 );
			delete unit;
		}
	}

	void SetStream( const wchar_t *stream)
	{
		wcsncpy( m_szStream, stream, sizeof( m_szStream ) / sizeof( wchar_t ) );
	}

	const wchar_t *GetStream() const
	{
		return m_szStream;
	}

	void SetTimeToLive( float ttl )
	{
		m_flTimeToLive = ttl;
	}

	float GetTimeToLive( void ) const
	{
		return m_flTimeToLive;
	}

	bool IsValid() const
	{
		return m_bValid;
	}

	void	SetHeight( int h )
	{
		m_nTotalHeight = h;
	}
	int		GetHeight() const
	{
		return m_nTotalHeight;
	}
	void	SetWidth( int w )
	{
		m_nTotalWidth = w;
	}
	int		GetWidth() const
	{
		return m_nTotalWidth;
	}

	void	AddWork( CCloseCaptionWorkUnit *unit )
	{
		m_Work.AddToTail( unit );
	}

	int		GetNumWorkUnits() const
	{
		return m_Work.Count();
	}

	CCloseCaptionWorkUnit *GetWorkUnit( int index )
	{
		Assert( index >= 0 && index < m_Work.Count() );

		return m_Work[ index ];
	}

	void		SetSizeComputed( bool computed )
	{
		m_bSizeComputed = computed;
	}

	bool		GetSizeComputed() const
	{
		return m_bSizeComputed;
	}

	void		SetPreDisplayTime( float t )
	{
		m_flPreDisplayTime = t;
	}

	float		GetPreDisplayTime() const
	{
		return m_flPreDisplayTime;
	}
private:
	wchar_t				m_szStream[ 256 ];

	float				m_flPreDisplayTime;
	float				m_flTimeToLive;
	bool				m_bValid;
	int					m_nTotalWidth;
	int					m_nTotalHeight;

	bool				m_bSizeComputed;

	CUtlVector< CCloseCaptionWorkUnit * >	m_Work;
};

ICloseCaptionManager *closecaptionmanager = NULL;

CloseCaptionTool::CloseCaptionTool( mxWindow *parent )
: IFacePoserToolWindow( "CloseCaptionTool", "Close Caption" ), mxWindow( parent, 0, 0, 0, 0 )
{
	m_nLastItemCount = -1;
	closecaptionmanager = this;

	m_hFonts[ CCFONT_NORMAL ] = CreateFont(
		-STREAM_POINTSIZE, 
		0,
		0,
		0,
		STREAM_WEIGHT,
		FALSE,
		FALSE,
		FALSE,
		DEFAULT_CHARSET,
		OUT_TT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY,
		DEFAULT_PITCH,
		STREAM_FONT );

	m_hFonts[ CCFONT_ITALIC ] = CreateFont(
		-STREAM_POINTSIZE, 
		0,
		0,
		0,
		STREAM_WEIGHT,
		TRUE,
		FALSE,
		FALSE,
		DEFAULT_CHARSET,
		OUT_TT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY,
		DEFAULT_PITCH,
		STREAM_FONT );
	
	m_hFonts[ CCFONT_BOLD ] = CreateFont(
		-STREAM_POINTSIZE, 
		0,
		0,
		0,
		700,
		FALSE,
		FALSE,
		FALSE,
		DEFAULT_CHARSET,
		OUT_TT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY,
		DEFAULT_PITCH,
		STREAM_FONT );

	m_hFonts[ CCFONT_ITALICBOLD ] = CreateFont(
		-STREAM_POINTSIZE, 
		0,
		0,
		0,
		700,
		TRUE,
		FALSE,
		FALSE,
		DEFAULT_CHARSET,
		OUT_TT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY,
		DEFAULT_PITCH,
		STREAM_FONT );
}

CloseCaptionTool::~CloseCaptionTool( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void CloseCaptionTool::Think( float dt )
{
	int c = m_Items.Count();
	int i;

	// Pass one decay all timers
	for ( i = 0 ; i < c ; ++i )
	{
		CCloseCaptionItem *item = m_Items[ i ];

		float predisplay = item->GetPreDisplayTime();
		if ( predisplay > 0.0f )
		{
			predisplay -= dt;
			predisplay = max( 0.0f, predisplay );
			item->SetPreDisplayTime( predisplay );
		}
		else
		{
			// remove time from actual playback
			float ttl = item->GetTimeToLive();
			ttl -= dt;
			ttl = max( 0.0f, ttl );
			item->SetTimeToLive( ttl );
		}
	}

	// Pass two, remove from head until we get to first item with time remaining
	bool foundfirstnondeletion = false;
	for ( i = 0 ; i < c ; ++i )
	{
		CCloseCaptionItem *item = m_Items[ i ];

		// Skip items not yet showing...
		float predisplay = item->GetPreDisplayTime();
		if ( predisplay > 0.0f )
		{
			continue;
		}

		float ttl = item->GetTimeToLive();
		if ( ttl > 0.0f )
		{
			foundfirstnondeletion = true;
			continue;
		}

		// Skip the remainder of the items after we find the first/oldest active item
		if ( foundfirstnondeletion )
		{
			continue;
		}

		delete item;
		m_Items.Remove( i );
		--i;
		--c;
	}

	if ( m_Items.Count() != m_nLastItemCount )
	{
		redraw();
	}
	m_nLastItemCount = m_Items.Count();
}

struct VisibleStreamItem
{
	int					height;
	CCloseCaptionItem	*item;
};

void CloseCaptionTool::redraw()
{
	if ( !ToolCanDraw() )
		return;

	CChoreoWidgetDrawHelper drawHelper( this );
	HandleToolRedraw( drawHelper );

	RECT rcOutput;
	drawHelper.GetClientRect( rcOutput );

	RECT rcText = rcOutput;
	drawHelper.DrawFilledRect( RGB( 0, 0, 0 ), rcText );
	drawHelper.DrawOutlinedRect( RGB( 200, 245, 150 ), PS_SOLID, 2, rcText );
	InflateRect( &rcText, -4, 0 );

	int avail_width = rcText.right - rcText.left;

	int totalheight = 0;
	int i;
	CUtlVector< VisibleStreamItem > visibleitems;
	int c = m_Items.Count();
	for  ( i = 0; i < c; i++ )
	{
		CCloseCaptionItem *item = m_Items[ i ];

		// Not ready for display yet.
		if ( item->GetPreDisplayTime() > 0.0f )
		{
			continue;
		}

		if ( !item->GetSizeComputed() )
		{
			ComputeStreamWork( drawHelper, avail_width, item );
		}
			
		int itemheight = item->GetHeight();

		totalheight += itemheight;

		VisibleStreamItem si;
		si.height = itemheight;
		si.item = item;

		visibleitems.AddToTail( si );
	}

	rcText.bottom -= 2;
	rcText.top = rcText.bottom - totalheight;

	// Now draw them
	c = visibleitems.Count();
	for ( i = 0; i < c; i++ )
	{
		VisibleStreamItem *si = &visibleitems[ i ];

		int height = si->height;
		CCloseCaptionItem *item = si->item;

		rcText.bottom = rcText.top + height;

		DrawStream( drawHelper, rcText, item );

		OffsetRect( &rcText, 0, height );

		if ( rcText.top >= rcOutput.bottom )
			break;
	}
}

int	CloseCaptionTool::handleEvent( mxEvent *event )
{
	//MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	int iret = 0;

	if ( HandleToolEvent( event ) )
	{
		return iret;
	}

	return iret;
}

bool CloseCaptionTool::PaintBackground()
{
	redraw();
	return false;
}

void CloseCaptionTool::Reset( void )
{
	while ( m_Items.Count() > 0 )
	{
		CCloseCaptionItem *i = m_Items[ 0 ];
		delete i;
		m_Items.Remove( 0 );
	}
}

void CloseCaptionTool::Process( char const *tokenname, float duration, int languageid )
{
	bool valid = true;
	wchar_t stream[ 256 ];
	if ( !LookupUnicodeText( languageid, tokenname, stream, sizeof( stream ) / sizeof( wchar_t ) ) )
	{
		valid = false;
		g_pLocalize->ConvertANSIToUnicode( va( "--> Missing Caption[%s]", tokenname ), stream, sizeof( stream ) );
	}

	if ( !wcsncmp( stream, L"!!!", wcslen( L"!!!" ) ) )
	{
		// It's in the text file, but hasn't been translated...
		valid = false;
	}

	// Nothing to do...
	if ( wcslen( stream ) == 0 )
	{
		return;
	}

	float delay = 0.0f;

	wchar_t phrase[ 1024 ];
	wchar_t *out = phrase;

	for ( const wchar_t *curpos = stream; curpos && *curpos != L'\0'; ++curpos )
	{
		wchar_t cmd[ 256 ];
		wchar_t args[ 256 ];

		if ( SplitCommand( &curpos, cmd, args ) )
		{
			if ( !wcscmp( cmd, L"delay" ) )
			{

				// End current phrase
				*out = L'\0';

				if ( wcslen( phrase ) > 0 )
				{
					CCloseCaptionItem *item = new CCloseCaptionItem( phrase, duration + CAPTION_LINGER_TIME, delay, valid );
					m_Items.AddToTail( item );
				}

				// Start new phrase
				out = phrase;

				// Delay must be positive
				delay = max( 0.0f, (float)wcstod( args, NULL ) );

				continue;
			}
		}

		*out++ = *curpos;
	}

	// End final phrase, if any
	*out = L'\0';
	if ( wcslen( phrase ) > 0 )
	{
		CCloseCaptionItem *item = new CCloseCaptionItem( phrase, duration + CAPTION_LINGER_TIME, delay, valid );
		m_Items.AddToTail( item );
	}
}

bool CloseCaptionTool::LookupUnicodeText( int languageId, char const *token, wchar_t *outbuf, size_t count )
{
	wchar_t *outstr = g_pLocalize->Find( token );
	if ( !outstr )
	{
		wcsncpy( outbuf, L"<can't find entry>", count );
		return false;
	}

	wcsncpy( outbuf, outstr, count );

	return true;
}

bool CloseCaptionTool::LookupStrippedUnicodeText( int languageId, char const *token, wchar_t *outbuf, size_t count )
{
	wchar_t *outstr = g_pLocalize->Find( token );
	if ( !outstr )
	{
		wcsncpy( outbuf, L"<can't find entry>", count );
		return false;
	}

	const wchar_t *curpos = outstr;
	wchar_t *out = outbuf;
	size_t outlen = 0;
	
	for ( ; 
		curpos && *curpos != L'\0' && outlen < count;
		++curpos )
	{
		wchar_t cmd[ 256 ];
		wchar_t args[ 256 ];

		if ( SplitCommand( &curpos, cmd, args ) )
		{
			continue;
		}

		*out++ = *curpos;
		++outlen;
	}

	*out = L'\0';

	return true;
}

bool CloseCaptionTool::SplitCommand( wchar_t const **ppIn, wchar_t *cmd, wchar_t *args ) const
{
	const wchar_t *in = *ppIn;
	const wchar_t *oldin = in;

	if ( in[0] != L'<' )
	{
		*ppIn += ( oldin - in );
		return false;
	}

	args[ 0 ] = 0;
	cmd[ 0 ]= 0;
	wchar_t *out = cmd;
	in++;
	while ( *in != L'\0' && *in != L':' && *in != L'>' && !isspace( *in ) )
	{
		*out++ = *in++;
	}
	*out = L'\0';

	if ( *in != L':' )
	{
		*ppIn += ( in - oldin );
		return true;
	}

	in++;
	out = args;
	while ( *in != L'\0' && *in != L'>' )
	{
		*out++ = *in++;
	}
	*out = L'\0';

	//if ( *in == L'>' )
	//	in++;

	*ppIn += ( in - oldin );
	return true;
}

struct WorkUnitParams
{
	WorkUnitParams()
	{
		Q_memset( stream, 0, sizeof( stream ) );
		out = stream;
		x = 0;
		y = 0;
		width = 0;
		bold = italic = false;
		clr = RGB( 255, 255, 255 );
		newline = false;
	}

	~WorkUnitParams()
	{
	}

	void Finalize()
	{
		*out = L'\0';
	}

	void Next()
	{
		// Restart output
		Q_memset( stream, 0, sizeof( stream ) );
		out = stream;

		x += width;

		width = 0;
		// Leave bold, italic and color alone!!!

		if ( newline )
		{
			newline = false;
			x = 0;
			y += STREAM_LINEHEIGHT;
		}
	}

	int GetFontNumber()
	{
		return CloseCaptionTool::GetFontNumber( bold, italic );
	}
		
	wchar_t	stream[ 1024 ];
	wchar_t	*out;

	int		x;
	int		y;
	int		width;
	bool	bold;
	bool	italic;
	COLORREF clr;
	bool	newline;
};

void CloseCaptionTool::AddWorkUnit( CCloseCaptionItem *item,
	WorkUnitParams& params )
{
	params.Finalize();

	if ( wcslen( params.stream ) > 0 )
	{
		CCloseCaptionWorkUnit *wu = new CCloseCaptionWorkUnit();

		wu->SetStream( params.stream );
		wu->SetColor( params.clr );
		wu->SetBold( params.bold );
		wu->SetItalic( params.italic );
		wu->SetWidth( params.width );
		wu->SetHeight( STREAM_LINEHEIGHT );
		wu->SetPos( params.x, params.y );


		int curheight = item->GetHeight();
		int curwidth = item->GetWidth();

		curheight = max( curheight, params.y + wu->GetHeight() );
		curwidth = max( curwidth, params.x + params.width );

		item->SetHeight( curheight );
		item->SetWidth( curwidth );

		// Add it
		item->AddWork( wu );

		params.Next();
	}
}

void CloseCaptionTool::ComputeStreamWork( CChoreoWidgetDrawHelper &helper, int available_width, CCloseCaptionItem *item )
{
	// Start with a clean param block
	WorkUnitParams params;

	const wchar_t *curpos = item->GetStream();
	
	CUtlVector< COLORREF > colorStack;

	for ( ; curpos && *curpos != L'\0'; ++curpos )
	{
		wchar_t cmd[ 256 ];
		wchar_t args[ 256 ];

		if ( SplitCommand( &curpos, cmd, args ) )
		{
			if ( !wcscmp( cmd, L"cr" ) )
			{
				params.newline = true;
				AddWorkUnit( item, params);
			}
			else if ( !wcscmp( cmd, L"clr" ) )
			{
				AddWorkUnit( item, params );

				if ( args[0] == 0 && colorStack.Count()>= 2)
				{
					colorStack.Remove( colorStack.Count() - 1 );
					params.clr = colorStack[ colorStack.Count() - 1 ];
				}
				else
				{
					int r = 0, g = 0, b = 0;
					COLORREF newcolor;
					if ( 3 == swscanf( args, L"%i,%i,%i", &r, &g, &b ) )
					{
						newcolor = RGB( r, g, b );
						colorStack.AddToTail( newcolor );
						params.clr = colorStack[ colorStack.Count() - 1 ];
					}
				}
			}
			else if ( !wcscmp( cmd, L"playerclr" ) )
			{
				AddWorkUnit( item, params );

				if ( args[0] == 0 && colorStack.Count()>= 2)
				{
					colorStack.Remove( colorStack.Count() - 1 );
					params.clr = colorStack[ colorStack.Count() - 1 ];
				}
				else
				{
					// player and npc color selector
					// e.g.,. 255,255,255:200,200,200
					int pr = 0, pg = 0, pb = 0, nr = 0, ng = 0, nb = 0;
					COLORREF newcolor;
					if ( 6 == swscanf( args, L"%i,%i,%i:%i,%i,%i", &pr, &pg, &pb, &nr, &ng, &nb ) )
					{
						// FIXME:  nothing in .vcds is ever from the player...
						newcolor = /*item->IsFromPlayer()*/ false ? RGB( pr, pg, pb ) : RGB( nr, ng, nb );
						colorStack.AddToTail( newcolor );
						params.clr = colorStack[ colorStack.Count() - 1 ];
					}
				}
			}
			else if ( !wcscmp( cmd, L"I" ) )
			{
				AddWorkUnit( item, params );
				params.italic = !params.italic;
			}
			else if ( !wcscmp( cmd, L"B" ) )
			{
				AddWorkUnit( item, params );
				params.bold = !params.bold;
			}

			continue;
		}

		HFONT useF = m_hFonts[ params.GetFontNumber() ];
		
		int w = helper.CalcTextWidthW( useF, L"%c", *curpos ); 

		if ( ( params.x + params.width ) + w > available_width )
		{
			params.newline = true;
			AddWorkUnit( item, params );
		}
		*params.out++ = *curpos;
		params.width += w;
	}

	// Add the final unit.
	params.newline = true;
	AddWorkUnit( item, params );

	item->SetSizeComputed( true );

	// DumpWork( item );
}

void CloseCaptionTool::	DumpWork( CCloseCaptionItem *item )
{
	int c = item->GetNumWorkUnits();
	for ( int i = 0 ; i < c; ++i )
	{
		CCloseCaptionWorkUnit *wu = item->GetWorkUnit( i );
		wu->Dump();
	}
}

void CloseCaptionTool::DrawStream( CChoreoWidgetDrawHelper &helper, RECT &rcText, CCloseCaptionItem *item )
{
	int c = item->GetNumWorkUnits();

	RECT rcOut;
	rcOut.left = rcText.left;

	for ( int i = 0 ; i < c; ++i )
	{
		int x = 0;
		int y = 0;

		CCloseCaptionWorkUnit *wu = item->GetWorkUnit( i );
	
		HFONT useF = m_hFonts[ wu->GetFontNumber() ];

		wu->GetPos( x, y );

		rcOut.left = rcText.left + x;
		rcOut.right = rcOut.left + wu->GetWidth();
		rcOut.top = rcText.top + y;
		rcOut.bottom = rcOut.top + wu->GetHeight();

		COLORREF useColor = wu->GetColor();

		if ( !item->IsValid() )
		{
			useColor = RGB( 255, 255, 255 );
			rcOut.right += 2;
			helper.DrawFilledRect( RGB( 100, 100, 40 ), rcOut );
		}

		helper.DrawColoredTextW( useF, useColor,
				rcOut, L"%s", wu->GetStream() );

	}
}

int CloseCaptionTool::GetFontNumber( bool bold, bool italic )
{
	if ( bold || italic )
	{
		if( bold && italic )
		{
			return CloseCaptionTool::CCFONT_ITALICBOLD;
		}

		if ( bold )
		{
			return CloseCaptionTool::CCFONT_BOLD;
		}

		if ( italic )
		{
			return CloseCaptionTool::CCFONT_ITALIC;
		}
	}

	return CloseCaptionTool::CCFONT_NORMAL;
}