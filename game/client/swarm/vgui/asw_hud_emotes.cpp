
#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"
#include "iclientmode.h"
#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>

#include <vgui/ILocalize.h>

#include <filesystem.h>
#include <keyvalues.h>

#include "asw_hudelement.h"
#include "c_asw_marine.h"
#include "asw_marine_profile.h"
#include "c_asw_marine_resource.h"
#include "c_asw_sentry_base.h"
#include "c_asw_game_resource.h"
#include "c_asw_door.h"
#include "c_asw_use_area.h"

#include "ConVar.h"
#include "tier0/vprof.h"
#include "idebugoverlaypanel.h"
#include "engine/IVDebugOverlay.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

extern ConVar asw_draw_hud;

//-----------------------------------------------------------------------------
// Purpose: Shows the marines emote graphics
//-----------------------------------------------------------------------------
class CASWHudEmotes : public CASW_HudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASWHudEmotes, vgui::Panel );

public:
	CASWHudEmotes( const char *pElementName );
	~CASWHudEmotes();

	virtual void Paint();	
	virtual void ApplySchemeSettings(IScheme *pScheme);
	virtual void PaintEmotes();
	virtual void PaintEmotesFor(C_ASW_Marine* pMarine);
	virtual void PaintEmote(C_BaseEntity* pEnt, float fTime, int iTexture, float fScale=1.0f);
	virtual bool ShouldDraw( void ) { return asw_draw_hud.GetBool() && CASW_HudElement::ShouldDraw(); }

	CPanelAnimationVarAliasType( int, m_nMedicTexture,		"MedicEmoteTexture",	"vgui/swarm/Emotes/EmoteMedic", "textureid" );
	CPanelAnimationVarAliasType( int, m_nAmmoTexture,		"AmmoEmoteTexture",		"vgui/swarm/Emotes/EmoteAmmo", "textureid" );
	CPanelAnimationVarAliasType( int, m_nSmileTexture,		"SmileEmoteTexture",	"vgui/swarm/Emotes/EmoteSmile", "textureid" );
	CPanelAnimationVarAliasType( int, m_nStopTexture,		"StopEmoteTexture",		"vgui/swarm/Emotes/EmoteStop", "textureid" );
	CPanelAnimationVarAliasType( int, m_nGoTexture,			"GoEmoteTexture",		"vgui/swarm/Emotes/EmoteGo", "textureid" );
	CPanelAnimationVarAliasType( int, m_nExclaimTexture,	"ExclaimEmoteTexture",	"vgui/swarm/Emotes/EmoteExclaim", "textureid" );
	CPanelAnimationVarAliasType( int, m_nAnimeTexture,		"AnimeEmoteTexture",	"vgui/swarm/Emotes/EmoteAnime", "textureid" );
	CPanelAnimationVarAliasType( int, m_nQuestionTexture,	"QuestionTexture",	"vgui/swarm/Emotes/EmoteQuestion", "textureid" );
	CPanelAnimationVarAliasType( int, m_nWrenchTexture,		"WrenchTexture",	"vgui/swarm/ClassIcons/EngineerIcon", "textureid" );
	CPanelAnimationVarAliasType( int, m_nSentryUpTexture,	"SentryUpTexture",	"vgui/swarm/ClassIcons/SentryBuild", "textureid" );
	CPanelAnimationVarAliasType( int, m_nSentryDnTexture,	"SentryDnTexture",	"vgui/swarm/ClassIcons/SentryDismantle", "textureid" );
	CPanelAnimationVarAliasType( int, m_nHackTexture,		"HackTexture",	"vgui/swarm/ClassIcons/HackIcon", "textureid" );
	CPanelAnimationVarAliasType( int, m_nWeldTexture,		"WeldTexture",	"vgui/swarm/ClassIcons/WeldIcon", "textureid" );
};	

DECLARE_HUDELEMENT( CASWHudEmotes );

CASWHudEmotes::CASWHudEmotes( const char *pElementName ) : CASW_HudElement( pElementName ), BaseClass( NULL, "ASWHudEmotes" )
{
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent( pParent );
	SetHiddenBits( HIDEHUD_REMOTE_TURRET);
	
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);	
}

CASWHudEmotes::~CASWHudEmotes()
{

}

void CASWHudEmotes::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(0,0,0,0));
	SetPaintBackgroundEnabled( false );
}

void CASWHudEmotes::Paint()
{
	VPROF_BUDGET( "CASWHudEmotes::Paint", VPROF_BUDGETGROUP_ASW_CLIENT );
	BaseClass::Paint();
	PaintEmotes();
}


void CASWHudEmotes::PaintEmotes()
{
	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		C_ASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if (!pMR)
			continue;

		C_ASW_Marine *marine = pMR->GetMarineEntity();
		if ( !marine )
			continue;

		PaintEmotesFor(marine);
	}
}

void CASWHudEmotes::PaintEmotesFor(C_ASW_Marine* pMarine)
{
	if (pMarine->bClientEmoteMedic)
		PaintEmote(pMarine, pMarine->fEmoteMedicTime, m_nMedicTexture);
	if (pMarine->bClientEmoteAmmo)
		PaintEmote(pMarine, pMarine->fEmoteAmmoTime, m_nAmmoTexture);
	if (pMarine->bClientEmoteSmile)
		PaintEmote(pMarine, pMarine->fEmoteSmileTime, m_nSmileTexture);
	if (pMarine->bClientEmoteStop)
		PaintEmote(pMarine, pMarine->fEmoteStopTime, m_nStopTexture);
	if (pMarine->bClientEmoteGo)
		PaintEmote(pMarine, pMarine->fEmoteGoTime, m_nGoTexture);
	if (pMarine->bClientEmoteExclaim)
		PaintEmote(pMarine, pMarine->fEmoteExclaimTime, m_nExclaimTexture);
	if (pMarine->bClientEmoteAnimeSmile)
		PaintEmote(pMarine, pMarine->fEmoteAnimeSmileTime, m_nAnimeTexture);
	if (pMarine->bClientEmoteQuestion)
		PaintEmote(pMarine, pMarine->fEmoteQuestionTime, m_nQuestionTexture);

	bool bBuildingSentry = false;
	bool bWelding = false;

	// paint wrench over a sentry gun if a marine is setting one up within an engineer's aura
	C_BaseEntity *pUsing = pMarine->m_hUsingEntity.Get();
	if (pUsing)
	{
		C_ASW_Sentry_Base* pSentry = dynamic_cast<C_ASW_Sentry_Base*>(pUsing);
		if ( pSentry )
		{
			if ( pSentry->IsAssembled() )
				PaintEmote(pMarine, 1.5f, m_nSentryDnTexture, 0.9f);
			else
				PaintEmote(pMarine, 1.5f, m_nSentryUpTexture, 0.9f);

			if ( pSentry->m_bSkillMarineHelping )
			{
				PaintEmote(pSentry, 1.0f, m_nWrenchTexture, 0.4f);
			}

			bBuildingSentry = true;
		}
	}

	// and a welding door
	if (pMarine->GetMarineResource())
	{
		C_ASW_Door *pDoor = pMarine->GetMarineResource()->m_hWeldingDoor.Get();
		if ( pDoor )
		{
			bWelding = true;
			if ( pDoor->m_bSkillMarineHelping )
				PaintEmote(pDoor, 1.0f, m_nWrenchTexture, 0.4f);	

			PaintEmote(pMarine, 1.5f, m_nWeldTexture, 0.75f);
		}
	}
	
	// paint wrench over an engineer if he's boosting a sentry/weld
	if (pMarine->GetMarineResource())
	{
		if ( pMarine->GetMarineResource()->m_bUsingEngineeringAura && !bBuildingSentry && !bWelding )
		{
			PaintEmote(pMarine, 0.9f, m_nWrenchTexture, 0.4f);
		}
	}

	bool bDrawHackIcon = false;

	if ( pMarine->IsInhabited() )
	{
		if ( pMarine->IsHacking() )
			bDrawHackIcon = true;
	}
	else
	{
		if ( pUsing )
		{
			C_ASW_Use_Area *pArea = static_cast< C_ASW_Use_Area* >( pUsing );
			if ( pArea->Classify() == CLASS_ASW_BUTTON_PANEL )
				bDrawHackIcon = true;
		}
	}

	if ( bDrawHackIcon )
		PaintEmote(pMarine, 1.0f, m_nHackTexture, 0.75f);
}

void CASWHudEmotes::PaintEmote(C_BaseEntity* pEnt, float fTime, int iTexture, float fEmoteScale)
{
	//Msg("PaintEmote scale = %f\n", fEmoteScale);
	if (fEmoteScale < 0)
		fEmoteScale = 0;
	Vector screenPos;
	Vector vecFacing;
	AngleVectors(pEnt->GetRenderAngles(), &vecFacing);
	vecFacing *= 5;
	if (!debugoverlay->ScreenPosition( pEnt->GetRenderOrigin() + Vector(0,40,70) + vecFacing, screenPos )) 
	{
		//Msg("Emote marinepos: %s\n", VecToString(pEnt->GetRenderOrigin()));
		//Msg("Emote marinefacing: %s\n", VecToString(vecFacing));
		
		float xPos		= screenPos[0];
		float yPos		= screenPos[1];

		if (iTexture != -1)
		{
			float fAlpha = 0;
			float fSize = 0.5 + 0.5 * ((2.0 - fTime) / 2.0f);
			if (fTime < 1.0f)		// fade in
				fAlpha = fTime;
			else						// fade out
				fAlpha = 2.0 - fTime;
			float f = clamp(1.0f - fAlpha, 0.0f, 1.0f);
			f = f * f * f;
			fAlpha = 1.0f - f;

			fSize = (0.5f + 0.5f * fAlpha) * 0.5f;
			float fScale = (ScreenHeight() / 768.0f) * fEmoteScale;
			float HalfW = 128.0f * fScale * fSize * 0.5f;
			float HalfH = 128.0f * fScale * fSize * 0.5f;

			surface()->DrawSetColor(Color(255,255,255,fAlpha*255.0f));
			surface()->DrawSetTexture(iTexture);
			//int x = xPos - HalfW;
			//int y = yPos - HalfH;
			
			Vertex_t points[4] = 
			{ 
			Vertex_t( Vector2D(xPos - HalfW, yPos - HalfH), Vector2D(0,0) ), 
			Vertex_t( Vector2D(xPos + HalfW, yPos - HalfH), Vector2D(1,0) ), 
			Vertex_t( Vector2D(xPos + HalfW, yPos + HalfH), Vector2D(1,1) ), 
			Vertex_t( Vector2D(xPos - HalfW, yPos + HalfH), Vector2D(0,1) ) 
			}; 
			surface()->DrawTexturedPolygon( 4, points );

			//surface()->DrawTexturedRect(xPos - HalfW, yPos - HalfH,
										//xPos + HalfW, yPos + HalfH);
		}
	}
}

