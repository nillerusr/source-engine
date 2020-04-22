//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ENGINEBUGREPORTER_H
#define ENGINEBUGREPORTER_H
#ifdef _WIN32
#pragma once
#endif

namespace vgui
{
	class Panel;
};

abstract_class IEngineBugReporter
{
public:
	typedef enum
	{
		BR_AUTOSELECT  = 0,
		BR_PUBLIC,
		BR_INTERNAL,
	} BR_TYPE;

	virtual void		Init( void ) = 0;
	virtual void		Shutdown( void ) = 0;

	virtual void		InstallBugReportingUI( vgui::Panel *parent, BR_TYPE type ) = 0;
	virtual bool		ShouldPause() const = 0;

	virtual bool		IsVisible() const = 0; //< true iff the bug panel is active and on screen right now
};

extern IEngineBugReporter *bugreporter;

#endif // ENGINEBUGREPORTER_H
