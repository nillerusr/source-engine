#include "cbase.h"
#include "CampaignMapSearchLights.h"
#include "vgui/isurface.h"
#include "asw_gamerules.h"
#include "asw_campaign_info.h"
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

int CampaignMapSearchLights::s_nSearchLightTexture = -1;

CampaignMapSearchLights::CampaignMapSearchLights(vgui::Panel *parent, const char *panelName) :
	vgui::Panel(parent, panelName)
{

}

void CampaignMapSearchLights::Paint()
{
	// make sure we're the same size as our parent
	if (!GetParent())
		return;

	int w, t;
	GetParent()->GetSize(w, t);
	SetSize(w, t);
	PaintSearchLights();
}

void CampaignMapSearchLights::PaintSearchLights()
{
	if (s_nSearchLightTexture == -1)
	{
		s_nSearchLightTexture = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( s_nSearchLightTexture, "vgui/swarm/Campaign/CampaignSearchlight" , true, false);
		if (s_nSearchLightTexture == -1)
			return;
	}

	if (!ASWGameRules() || !ASWGameRules()->GetCampaignInfo())
		return;

	CASW_Campaign_Info *pInfo = ASWGameRules()->GetCampaignInfo();
	vgui::surface()->DrawSetTexture(s_nSearchLightTexture);
	vgui::surface()->DrawSetColor(Color(255,255,255,128));

	for (int i=0;i<ASW_NUM_SEARCH_LIGHTS;i++)
	{
		if (pInfo->m_iSearchLightX[i] != 0)
		{
			Vector2D blip_centre((pInfo->m_iSearchLightX[i] / 1024.0f) * GetWide(),
								 (pInfo->m_iSearchLightY[i] / 1024.0f) * GetTall());

			int xoffset = 0;	// rotates around centre so can have no offset
			int yoffset = 0;
			int iLightSize = GetWide() * 0.11f;
			float fOscSize = 40.0f;
			float fOscScale = 0.5f + 0.05f * i;
			float fOscOffset = 1.9f * i;
			float fFacingYaw = pInfo->m_iSearchLightAngle[i] + (sin((gpGlobals->curtime*fOscScale)+fOscOffset) * fOscSize);
			Vector vecCornerTL(xoffset, iLightSize * -0.5f + yoffset, 0);
			Vector vecCornerTR(iLightSize + xoffset, iLightSize * -0.5f+ yoffset, 0);
			Vector vecCornerBR(iLightSize + xoffset, iLightSize * 0.5f+ yoffset, 0);
			Vector vecCornerBL(xoffset, iLightSize * 0.5f+ yoffset, 0);
			Vector vecCornerTL_rotated, vecCornerTR_rotated, vecCornerBL_rotated, vecCornerBR_rotated;

			// rotate it by our facing yaw
			QAngle angFacing(0, -fFacingYaw, 0);
			VectorRotate(vecCornerTL, angFacing, vecCornerTL_rotated);
			VectorRotate(vecCornerTR, angFacing, vecCornerTR_rotated);
			VectorRotate(vecCornerBR, angFacing, vecCornerBR_rotated);
			VectorRotate(vecCornerBL, angFacing, vecCornerBL_rotated);
			
			vgui::Vertex_t points[4] = 
			{ 
			vgui::Vertex_t( Vector2D(blip_centre.x + vecCornerTL_rotated.x, blip_centre.y + vecCornerTL_rotated.y), Vector2D(0,0) ), 
			vgui::Vertex_t( Vector2D(blip_centre.x + vecCornerTR_rotated.x, blip_centre.y + vecCornerTR_rotated.y), Vector2D(1,0) ), 
			vgui::Vertex_t( Vector2D(blip_centre.x + vecCornerBR_rotated.x, blip_centre.y + vecCornerBR_rotated.y), Vector2D(1,1) ), 
			vgui::Vertex_t( Vector2D(blip_centre.x + vecCornerBL_rotated.x, blip_centre.y + vecCornerBL_rotated.y), Vector2D(0,1) ) 
			}; 
			vgui::surface()->DrawTexturedPolygon( 4, points );
		}
	}
}