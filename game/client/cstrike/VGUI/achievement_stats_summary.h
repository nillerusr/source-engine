//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ACHIEVEMENTANDSTATSSUMMARY_H
#define ACHIEVEMENTANDSTATSSUMMARY_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyDialog.h>

class CAchievementsPage;
class CLifetimeStatsPage;
class CMatchStatsPage;
class StatCard;
class CStatsSummary;

const int cAchievementsDialogMinWidth = 1024; // don't show this screen for lower resolutions

//-----------------------------------------------------------------------------
// Purpose: dialog for displaying the achievements/stats summary
//-----------------------------------------------------------------------------
class CAchievementAndStatsSummary : public vgui::PropertyDialog
{
    DECLARE_CLASS_SIMPLE( CAchievementAndStatsSummary, vgui::PropertyDialog );

public:
    CAchievementAndStatsSummary(vgui::Panel *parent);
    ~CAchievementAndStatsSummary();

    virtual void Activate();

	void OnKeyCodePressed( vgui::KeyCode code )
	{
		if ( code == KEY_XBUTTON_B )
		{
			Close();
		}
		else
		{
			BaseClass::OnKeyCodePressed(code);
		}
	}

protected:
    virtual bool OnOK(bool applyOnly);
    virtual void OnSizeChanged( int newWide, int newTall );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

private:
    CAchievementsPage*  m_pAchievementsPage;
    CLifetimeStatsPage* m_pLifetimeStatsPage;
	CMatchStatsPage*	m_pMatchStatsPage;
	CStatsSummary*		m_pStatsSummary;
};


#endif // ACHIEVEMENTANDSTATSSUMMARY_H
