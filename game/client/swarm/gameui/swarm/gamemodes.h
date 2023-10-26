#ifndef __GAMEMODES_H__
#define __GAMEMODES_H__

#include "basemodui.h"
#include "vhybridbutton.h"

struct GameModeInfo_t
{
	CUtlString						m_NameId;
	CUtlString						m_NameText;
	CUtlString						m_CommandText;
	CUtlString						m_TitleText;
	CUtlString						m_HintText;
	CUtlString						m_HintTextDisabled;
	int								m_nImageId;
	bool							m_bEnabled;
	BaseModUI::BaseModHybridButton	*m_pHybridButton;
};

class GameModes : public vgui::Button
{
public:
	DECLARE_CLASS_SIMPLE( GameModes, vgui::Button );

	GameModes( vgui::Panel *pParent, const char *pName );
	virtual ~GameModes();

	virtual void PaintBackground( void );
	virtual void ApplySettings( KeyValues *pInResourceData );
	virtual void NavigateTo();
	virtual void NavigateFrom();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnCommand( const char *command );
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnKeyCodePressed( vgui::KeyCode code );

	bool	ScrollLeft();
	bool	ScrollRight( int nCount = 0 );
	void	SetEnabled( const char *pNameId, bool bEnabled );
	bool	SetActive( const char *pNameId, bool bForce );	
	bool	GetLastActiveNameId( char *pOutBuffer, int nOutBufferSize );
	bool	IsScrollBusy();

	int		GetNumGameInfos();
	BaseModUI::BaseModHybridButton *GetHybridButton( int nIndex );

private:
	void	SetActiveGameMode( int nActive, bool bKeepFocus );
	int		NameIdToModeInfo( const char *pNameId );
	int		DrawColoredText( vgui::HFont hFont, int x, int y, Color color, const char *pAnsiText, float alphaScale = 1.0f );
	int		DrawSmearBackgroundFade( int x, int y, int wide, int tall );

	// the active pic
	int			m_nPicOffsetX;
	int			m_nPicWidth;
	int			m_nPicHeight;

	// the button and hint underneath the active pic
	int			m_nMenuTitleX;
	int			m_nMenuTitleY;
	int			m_nMenuTitleWide;
	int			m_nMenuTitleTall;
	int			m_nMenuTitleActualTall;

	// the sub pics to the right of the active pic
	int			m_nSubPics;
	int			m_nSubPicGap;
	int			m_nSubPicOffsetX;
	int			m_nSubPicOffsetY;
	int			m_nSubPicWidth;
	int			m_nSubPicHeight;
	int			m_nNameFontHeight;
	vgui::HFont	m_hNameFont;
	int			m_nSubPicX;
	int			m_nSubPicY;

	// arrows
	int			m_nArrowWidth;
	int			m_nArrowHeight;
	int			m_nArrowOffsetY;
	int			m_nLeftArrowX;
	int			m_nLeftArrowY;
	int			m_nRightArrowX;
	int			m_nRightArrowY;
	int			m_nRightArrowOffsetX;

	int			m_nActive;

	float		m_startScrollTime;
	bool		m_bLeftScroll;
	bool		m_bHideLabels;
	int			m_nScrollMultipleCount;

	int			m_nBorderImageId;
	int			m_nTopBorderImageId;
	int			m_nBottomBorderImageId;
	int			m_nLeftArrowId;
	int			m_nRightArrowId;
	Color		m_smearColor;

	CUtlVector< GameModeInfo_t >	m_GameModeInfos;
};

#endif