//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef ICOLORCORRECTIONTOOLS_H
#define ICOLORCORRECTIONTOOLS_H
#ifdef _WIN32
#pragma once
#endif

namespace vgui
{
	class Panel;
};

class IColorOperation;

abstract_class IColorCorrectionTools
{
public:
	virtual void		Init( void ) = 0;
	virtual void		Shutdown( void ) = 0;

	virtual void		InstallColorCorrectionUI( vgui::Panel *parent ) = 0;
	virtual bool		ShouldPause() const = 0;

	virtual void		GrabPreColorCorrectedFrame( int x, int y, int width, int height ) =  0;

	virtual void		UpdateColorCorrection( ) =  0;

	virtual void		SetFinalOperation( IColorOperation *pOp ) = 0;
};

extern IColorCorrectionTools *colorcorrectiontools;

#endif // COLORCORRECTIONTOOLS_H
