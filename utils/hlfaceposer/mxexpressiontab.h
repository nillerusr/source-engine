//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef MXEXPRESSIONTAB_H
#define MXEXPRESSIONTAB_H
#ifdef _WIN32
#pragma once
#endif

#include "tabwindow.h"

class mxExpressionTab : public CTabWindow
{
public:
	mxExpressionTab( mxWindow *parent, int x, int y, int w, int h, int id = 0, int style = 0 )
		: CTabWindow( parent, x, y, w, h, id, style )
	{
	}

	virtual void	ShowRightClickMenu( int mx, int my );
	virtual int		getSelectedIndex () const;
};

extern mxExpressionTab	*g_pExpressionClass;

#endif // MXEXPRESSIONTAB_H
