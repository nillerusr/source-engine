#ifndef _INCLUDED_TILE_CHECK_H
#define _INCLUDED_TILE_CHECK_H

#include "utlvector.h"
#include "KeyValues.h"
#include "vgui_controls/Frame.h"

class CRoomTemplate;

class TileCheck
{
public:
	TileCheck();
	virtual ~TileCheck();

	void StartCheckingAllRoomTemplates();
	bool ContinueCheckingAllRoomTemplates();
	void CheckTemplate( CRoomTemplate *pTemplate );

	void TileCheckError( const char *pMsg, ... );
	void ClearTileCheckErrors();
	bool ShowTileCheckErrors();
	CUtlVector<const char*> m_TileCheckErrors;

	int CountAINodes( KeyValues *pRoomKeys );
	int CountNonFuncDetailBrushes( KeyValues *pRoomKeys );
	int CountDirectors( KeyValues *pRoomKeys );

	int m_iCurrentTheme;
	int m_iCurrentRoomTemplate;
	
	float GetProgress() { return (float) m_iCheckedTemplates / (float) m_iTotalTemplates; }
	int m_iTotalTemplates;
	int m_iCheckedTemplates;
};

namespace vgui
{
	class Label;
	class ProgressBar;
	class RichText;
}

//-----------------------------------------------------------------------------
// Purpose: Main dialog for visualizing and editing location grid keyvalues
//-----------------------------------------------------------------------------
class CTile_Check_Frame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CTile_Check_Frame, vgui::Frame );

public:
	CTile_Check_Frame( Panel *parent, const char *name );
	virtual ~CTile_Check_Frame();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnTick();

	vgui::Label *m_pCheckingLabel;
	vgui::ProgressBar *m_pProgressBar;
	vgui::RichText *m_pErrorTextBox;

protected:
	virtual void OnClose();
	virtual void OnCommand( const char *command );

	TileCheck *m_pTileCheck;
	bool m_bFirstPerformLayout;
	bool m_bCheckingTiles;
};

#endif // _INCLUDED_TILE_CHECK_H