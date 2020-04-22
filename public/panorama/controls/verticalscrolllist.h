//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef VERTICALSCROLLLIST_H
#define VERTICALSCROLLLIST_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"
#include "panorama/controls/label.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: CVerticalScrollList
//-----------------------------------------------------------------------------
class CVerticalScrollList : public CPanel2D
{
	DECLARE_PANEL2D( CVerticalScrollList, CPanel2D );

public:
	CVerticalScrollList( CPanel2D *parent, const char * pchPanelID );
	virtual ~CVerticalScrollList();

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

private:
};


} // namespace panorama

#endif // VERTICALSCROLLLIST_H
