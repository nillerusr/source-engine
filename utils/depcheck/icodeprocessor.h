//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( ICODEPROCESSOR_H )
#define ICODEPROCESSOR_H
#ifdef _WIN32
#pragma once
#endif


class ICodeProcessor
{
public:
	virtual void		Process( const char *gamespecific, const char *root, const char *dsp, const char *config ) = 0;

	virtual void		SetQuiet( bool quiet ) = 0;
	virtual bool		GetQuiet( void ) const = 0;

	virtual void		SetLogFile( bool log ) = 0;
	virtual bool		GetLogFile( void ) const = 0;
};

extern ICodeProcessor *processor;

#endif // ICODEPROCESSOR_H