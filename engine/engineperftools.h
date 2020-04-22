//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ENGINEPERFTOOLS_H
#define ENGINEPERFTOOLS_H
#ifdef _WIN32
#pragma once
#endif

namespace vgui
{
	class Panel;
};

abstract_class IEnginePerfTools
{
public:
	virtual void		Init( void ) = 0;
	virtual void		Shutdown( void ) = 0;

	virtual void		InstallPerformanceToolsUI( vgui::Panel *parent ) = 0;
	virtual bool		ShouldPause() const = 0;
};

extern IEnginePerfTools *perftools;

#endif // ENGINEPERFTOOLS_H
