//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_PROGRESSBAR_H
#define PANORAMA_PROGRESSBAR_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/panel2d.h"

namespace panorama
{

//////////////////////////////////////////////////////////////////////////
//
// progress bar, just sizes two panels to represent progress
//
class CProgressBar: public CPanel2D
{
	DECLARE_PANEL2D( CProgressBar, CPanel2D );
public:
	CProgressBar( CPanel2D *pParent, const char *pchID );
	virtual ~CProgressBar();

	void SetMin( float flMin ) { m_flMin = flMin; InvalidateSizeAndPosition(); }
	void SetMax( float flMax ) { m_flMax = flMax; InvalidateSizeAndPosition(); }
	void SetValue( float flValue ) { m_flCur = flValue; InvalidateSizeAndPosition(); }
	float GetValue() { return m_flCur; }

protected:
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );

private:
	float m_flMin;
	float m_flMax;
	float m_flCur;

	CPanel2D *m_ppanelLeft;
	CPanel2D *m_ppanelRight;

};

} // namespace panorama

#endif // PANORAMA_PROGRESSBAR_H


