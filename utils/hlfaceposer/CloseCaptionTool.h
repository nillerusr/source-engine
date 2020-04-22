//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CLOSECAPTIONTOOL_H
#define CLOSECAPTIONTOOL_H
#ifdef _WIN32
#pragma once
#endif

#include <mxtk/mx.h>
#include "utlvector.h"
#include "faceposertoolwindow.h"
#include "iclosecaptionmanager.h"

class CCloseCaptionItem;
struct WorkUnitParams;

class CloseCaptionTool : public mxWindow, public IFacePoserToolWindow, public ICloseCaptionManager
{
public:
	// Construction
						CloseCaptionTool( mxWindow *parent );
						~CloseCaptionTool( void );

	// ICloseCaptionManager
	virtual void Reset( void );
	virtual void Process( char const *tokenname, float duration, int languageid );
	virtual bool LookupUnicodeText( int languageId, char const *token, wchar_t *outbuf, size_t count );
	virtual bool LookupStrippedUnicodeText( int languageId, char const *token, wchar_t *outbuf, size_t count );

	// End ICloseCaptionManager

	virtual void		Think( float dt );

	virtual int			handleEvent( mxEvent *event );
	virtual void		redraw( void );
	virtual bool		PaintBackground();

	enum
	{
		CCFONT_NORMAL = 0,
		CCFONT_ITALIC,
		CCFONT_BOLD,
		CCFONT_ITALICBOLD
	};

	static int GetFontNumber( bool bold, bool italic );

private:
	void	ComputeStreamWork( CChoreoWidgetDrawHelper &helper, int available_width, CCloseCaptionItem *item );
	void	DrawStream( CChoreoWidgetDrawHelper &helper, RECT &rcText, CCloseCaptionItem *item );
	bool	SplitCommand( const wchar_t **in, wchar_t *cmd, wchar_t *args ) const;

	void	ParseCloseCaptionStream( const wchar_t *in, int available_width );

	void	DumpWork( CCloseCaptionItem *item );

	void AddWorkUnit( 
		CCloseCaptionItem *item,	
		WorkUnitParams& params );

	CUtlVector< CCloseCaptionItem * > m_Items;

	HFONT m_hFonts[ 4 ];  // normal, italic, bold, bold + italic

	int									m_nLastItemCount;
};

extern CloseCaptionTool	*g_pCloseCaptionTool;

#endif // CLOSECAPTIONTOOL_H
