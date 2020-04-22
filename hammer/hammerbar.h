//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines a special dockable dialog bar that activates itself when
//			the mouse cursor moves over it. This enables stacking of the
//			bars with only a small portion of each visible.
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef HAMMERBAR_H
#define HAMMERBAR_H
#pragma once

#include "utlvector.h"

#define RIGHT_JUSTIFY	0x01
#define BOTTOM_JUSTIFY	0x02
#define GROUP_BOX		0x04

struct ControlInfo_t
{
	int		m_nIDDialogItem;
	DWORD	m_dwPlacementFlag;
	int		m_nWidthBuffer;
	int		m_nHeightBuffer;
	int		m_nPosX;
	int		m_nPosY;
};

class CHammerBar : public CDialogBar
{
	public:

		CHammerBar(void)
		{
		}

		~CHammerBar(void);

		BOOL Create( CWnd* pParentWnd, UINT nIDTemplate, UINT nStyle, UINT nID );
		BOOL Create( CWnd* pParentWnd, UINT nIDTemplate, UINT nStyle, UINT nID, char *pszName );

		CSize m_sizeDocked;
        CSize m_sizeFloating;

		virtual CSize CalcDynamicLayout(int nLength, DWORD dwMode);
		virtual void OnSize( UINT nType, int cx, int cy );
		
		void AddControl( int nIDTemplate, DWORD dwPlacementFlag );
		void AdjustControls( void );

		CUtlVector< ControlInfo_t > m_ControlList;

	protected:

		afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);		

		DECLARE_MESSAGE_MAP() 
};


#endif // HAMMERBAR_H
