//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef LOGPREVIEW_H
#define LOGPREVIEW_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmehandle.h"

class CDmElement;
class CDmeClip;
class CDmeFilmClip;
class CDmeChannel;

enum LogPreviewChannelType_t
{
	LOG_PREVIEW_VALUE = 0,
	LOG_PREVIEW_BALANCE,
	LOG_PREVIEW_MULTILEVEL,
	LOG_PREVIEW_FLEX_CHANNEL_COUNT,

	LOG_PREVIEW_POSITION = 0,
	LOG_PREVIEW_ORIENTATION,
	LOG_PREVIEW_TRANSFORM_CHANNEL_COUNT,

	LOG_PREVIEW_MAX_CHANNEL_COUNT = 3,
};

struct LogPreview_t
{
	LogPreview_t() :
		m_bDragging( false ),
		m_bActiveLog( false ),
		m_bSelected( false )
	{
	}

	bool IsEqual( const LogPreview_t& other )
	{
		if ( m_hControl != other.m_hControl )
			return false;
		for ( int i = 0; i < LOG_PREVIEW_MAX_CHANNEL_COUNT; ++i )
		{
			if ( m_hChannels[ i ] != other.m_hChannels[ i ] )
				return false;
		}
		if ( m_hOwner != other.m_hOwner )
			return false;
		if ( m_hShot != other.m_hShot )
			return false;
		if ( m_bDragging != other.m_bDragging )
			return false;
		if ( m_bActiveLog != other.m_bActiveLog )
			return false;
		if ( m_bSelected != other.m_bSelected )
			return false;

		return true;
	}

	CDmeHandle< CDmElement >		m_hControl;  // The animation set control
	CDmeHandle< CDmeChannel >		m_hChannels[ LOG_PREVIEW_MAX_CHANNEL_COUNT ];
	CDmeHandle< CDmeClip >			m_hOwner;
	CDmeHandle< CDmeFilmClip >		m_hShot;

	bool							m_bDragging : 1;
	bool							m_bActiveLog : 1;
	bool							m_bSelected : 1;
};
#endif // LOGPREVIEW_H
