//========= Copyright Valve Corporation, All rights reserved. ============//
#include "cbase.h"
#include "hud_baseachievement_tracker.h"
#include "c_cs_player.h"
#include "iachievementmgr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// The number of counter-strike HUD achievements to display
const int cMaxCSHUDAchievments = 4;


using namespace vgui;

class CHudAchievementTracker : public CHudBaseAchievementTracker
{
    DECLARE_CLASS_SIMPLE( CHudAchievementTracker, CHudBaseAchievementTracker );

public:
    CHudAchievementTracker( const char *pElementName );
    virtual void OnThink();
    virtual void PerformLayout();
    virtual int  GetMaxAchievementsShown();
    virtual bool ShouldShowAchievement( IAchievement *pAchievement );

private:
    CPanelAnimationVarAliasType( int, m_iNormalY, "NormalY", "5", "proportional_int" );
};

DECLARE_HUDELEMENT( CHudAchievementTracker );


CHudAchievementTracker::CHudAchievementTracker( const char *pElementName ) : BaseClass( pElementName )
{
	RegisterForRenderGroup( "hide_for_scoreboard" );
}

void CHudAchievementTracker::OnThink()
{
    BaseClass::OnThink();
}

int CHudAchievementTracker::GetMaxAchievementsShown()
{
    return MIN( BaseClass::GetMaxAchievementsShown(), cMaxCSHUDAchievments );
}

void CHudAchievementTracker::PerformLayout()
{
    BaseClass::PerformLayout();

    int x, y;
    GetPos( x, y );
    SetPos( x, m_iNormalY );
}

bool CHudAchievementTracker::ShouldShowAchievement( IAchievement *pAchievement )
{
    if ( !BaseClass::ShouldShowAchievement(pAchievement) )
        return false;

    C_CSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
    if ( !pPlayer )
        return false;

    return true;
}