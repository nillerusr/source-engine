//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef GRAPHPANEL_H
#define GRAPHPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <KeyValues.h>

#include <vgui_controls/Frame.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Image.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui/IScheme.h>

#include "RemoteServer.h"

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CGraphPanel : public vgui::PropertyPage, public IServerDataResponse
{
	DECLARE_CLASS_SIMPLE( CGraphPanel, vgui::PropertyPage );
public:
	CGraphPanel(vgui::Panel *parent, const char *name);
	~CGraphPanel();

	// property page handlers
	virtual void OnPageShow();
	virtual void OnPageHide();

	void PerformLayout();

protected:
	// callback for receiving server data
	virtual void OnServerDataResponse(const char *value, const char *response);

	// called every frame to update stats page
	virtual void OnTick();

private:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	typedef enum { SECONDS, MINUTES, HOURS } intervals;
	struct Points_t
	{
		float cpu; // the percent CPU usage
		float in; // the ingoing bandwidth in kB/sec
		float out; // the outgoing bandwidth in kB/sec
		float time; // the time this was recorded
		float fps; // the FPS of the server
		float ping; // the ping of the server
		float players; // the number of players currently on the server
	};

	// internal class for holding the graph picture
	class CGraphsImage: public vgui::Image
	{
	public:
		CGraphsImage();
		using Image::DrawLine;
		using Image::SetPos;
		bool AddPoint(Points_t p);
		void RemovePoints() { points.RemoveAll(); }
		void SaveSize(int x1,int y1) { x=x1;y=y1; SetSize(x1,y1);}

		void SetDraw(bool cpu_in,bool fps_in,bool net_in,bool net_out,bool ping_in,bool players_in);
		void SetScale(intervals time);
		void SetBgColor(Color col) { bgColor=col; }
		void SetAxisColor(Color col) { lineColor=col; }

		// return the pre-defined colors of each graph type
		Color GetCPUColor() { return CPUColor; }
		Color GetFPSColor() { return FPSColor; }
		Color GetInColor() { return NetInColor; }
		Color GetOutColor() { return NetOutColor; }
		Color GetPlayersColor() { return PlayersColor; }
		Color GetPingColor() { return PingColor; }

		// return the limits of various graphs
		void GetFPSLimits(float &max, float &min) { min=minFPS; max=maxFPS; }
		void GetInLimits(float &max, float &min) { min=minIn; max=maxIn; }
		void GetOutLimits(float &max, float &min) { min=minOut; max=maxOut; }
		void GetPingLimits(float &max, float &min) { min=minPing; max=maxPing; }
		void GetPlayerLimits(float &max, float &min) { min=minPlayers; max=maxPlayers; }

		virtual void Paint();	

	private:

		void AvgPoint(Points_t p);
		void CheckBounds(Points_t p);

		CUtlVector<Points_t> points;
		
		int x,y; // the size
		float maxIn,minIn,maxOut,minOut; // max and min bandwidths
		float maxFPS,minFPS;
		float minPing,maxPing;
		float maxPlayers,minPlayers;

		bool cpu,fps,net_i,ping,net_o,players;
		intervals timeBetween;
		Points_t avgPoint;
		int numAvgs;
		Color bgColor,lineColor;

		// colors for the various graphs, set in graphpanel.cpp
		static Color CPUColor;
		static Color FPSColor;
		static Color NetInColor;
		static Color NetOutColor; 
		static Color PlayersColor;
		static Color PingColor;
	};
	friend CGraphsImage; // so it can use the intervals enum

	vgui::Label *GetLabel(const char *name);
	void SetAxisLabels(Color c,char *max,char *mid,char *min);

	// msg handlers 
	MESSAGE_FUNC( OnCheckButton, "CheckButtonChecked" );
	MESSAGE_FUNC_PTR_CHARPTR( OnTextChanged, "TextChanged", panel, text );	
	MESSAGE_FUNC( OnClearButton, "clear" );

	// GUI elements
	CGraphsImage *m_pGraphs;
	vgui::ImagePanel *m_pGraphsPanel;

	vgui::Button *m_pClearButton;

	vgui::CheckButton *m_pInButton;
	vgui::CheckButton *m_pOutButton;
	vgui::CheckButton *m_pFPSButton;
	vgui::CheckButton *m_pCPUButton;
	vgui::CheckButton *m_pPINGButton;
	vgui::CheckButton *m_pPlayerButton;
	
	vgui::ComboBox *m_pTimeCombo;
	vgui::ComboBox *m_pVertCombo;

	float m_flNextStatsUpdateTime;
};

#endif // GRAPHPANEL_H