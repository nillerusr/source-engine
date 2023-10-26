//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ISCENETOKENPROCESSOR_H
#define ISCENETOKENPROCESSOR_H
#ifdef _WIN32
#pragma once
#endif

abstract_class ISceneTokenProcessor
{
public:
	virtual const char	*CurrentToken( void ) = 0;
	virtual bool		GetToken( bool crossline ) = 0;
	virtual bool		TokenAvailable( void ) = 0;
	virtual void		Error( const char *fmt, ... ) = 0;
};

#endif // ISCENETOKENPROCESSOR_H
