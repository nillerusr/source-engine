//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef CLOSEDCAPTIONS_H
#define CLOSEDCAPTIONS_H
#ifdef _WIN32
#pragma once
#endif

#include "captioncompiler.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlsortvector.h"

FORWARD_DECLARE_HANDLE( memhandle_t );

typedef CUtlSortVector< CaptionLookup_t, CCaptionLookupLess > CaptionDictionary_t;
struct AsyncCaption_t
{
	AsyncCaption_t() : 
		m_DataBaseFile( UTL_INVAL_SYMBOL ),
		m_RequestedBlocks( 0, 0, BlockInfo_t::Less )
	{
		Q_memset( &m_Header, 0, sizeof( m_Header ) );
	}

	struct BlockInfo_t
	{
		int			fileindex;
		int			blocknum;
		memhandle_t handle;

		static bool Less( const BlockInfo_t& lhs, const BlockInfo_t& rhs )
		{
			if ( lhs.fileindex != rhs.fileindex )
				return lhs.fileindex < rhs.fileindex;

			return lhs.blocknum < rhs.blocknum;
		}
	};

	AsyncCaption_t& operator =( const AsyncCaption_t& rhs )
	{
		if ( this == &rhs )
			return *this;

		m_CaptionDirectory = rhs.m_CaptionDirectory;
		m_Header = rhs.m_Header;
		m_DataBaseFile = rhs.m_DataBaseFile;

		for ( int i = rhs.m_RequestedBlocks.FirstInorder(); i != rhs.m_RequestedBlocks.InvalidIndex(); i = rhs.m_RequestedBlocks.NextInorder( i ) )
		{
			m_RequestedBlocks.Insert( rhs.m_RequestedBlocks[ i ] );
		}

		return *this;
	}

	bool LoadFromFile( char const *pchFullPath );

	CUtlRBTree< BlockInfo_t, unsigned short >	m_RequestedBlocks;

	CaptionDictionary_t		m_CaptionDirectory;
	CompiledCaptionHeader_t	m_Header;
	CUtlSymbol				m_DataBaseFile;
};

#endif // CLOSEDCAPTIONS_H
