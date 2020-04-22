//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>

#include "GraphPanel.h"

#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <KeyValues.h>

#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ToggleButton.h>
#include <vgui_controls/RadioButton.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/CheckButton.h>

#define max(a,b)            (((a) > (b)) ? (a) : (b))

#define STATS_UPDATE_RATE	5.0f


// colors for the various graph lines+controls
Color CGraphPanel::CGraphsImage::CPUColor= Color(0,255,0,255); // green
Color CGraphPanel::CGraphsImage::FPSColor= Color(255,0,0,255); // red
Color CGraphPanel::CGraphsImage::NetInColor = Color(255,255,0,255); // yellow
Color CGraphPanel::CGraphsImage::NetOutColor = Color(0,255,255,255); // light blue
Color CGraphPanel::CGraphsImage::PlayersColor = Color(255,0,255,255); // purple
Color CGraphPanel::CGraphsImage::PingColor = Color(0,0,0,255); // black
//Color CGraphPanel::CGraphsImage::lineColor = Color(76,88,68,255);

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGraphPanel::CGraphPanel(vgui::Panel *parent, const char *name) : PropertyPage(parent, name)
{
	SetMinimumSize(300,200);

	m_pGraphsPanel = new ImagePanel(this,"Graphs");
	m_pGraphs = new CGraphsImage();
	m_pGraphsPanel->SetImage(m_pGraphs);

	m_pInButton = new CheckButton(this,"InCheck","#Graph_In");
	m_pOutButton = new CheckButton(this,"OutCheck","#Graph_Out");
	m_pFPSButton = new CheckButton(this,"FPSCheck","#Graph_FPS");
	m_pCPUButton = new CheckButton(this,"CPUCheck","#Graph_CPU");
	m_pPINGButton = new CheckButton(this,"PingCheck","#Graph_Ping");
	m_pPlayerButton = new CheckButton(this,"PlayersCheck","#Graph_Players");

	m_pTimeCombo = new ComboBox(this, "TimeCombo",3,false);
	m_pTimeCombo->AddItem("#Graph_Minutes", NULL);
	int defaultItem = m_pTimeCombo->AddItem("#Graph_Hours", NULL);
	m_pTimeCombo->AddItem("#Graph_Day", NULL);
	m_pTimeCombo->ActivateItem(defaultItem);

	m_pVertCombo = new ComboBox(this, "VertCombo",6,false);
	m_pVertCombo->AddItem("#Graph_In", NULL);
	m_pVertCombo->AddItem("#Graph_Out", NULL);
	m_pVertCombo->AddItem("#Graph_FPS", NULL);
	defaultItem = m_pVertCombo->AddItem("#Graph_CPU", NULL);
	m_pVertCombo->AddItem("#Graph_Ping", NULL);
	m_pVertCombo->AddItem("#Graph_Players", NULL);
	m_pVertCombo->ActivateItem(defaultItem);

	// now setup defaults
	m_pCPUButton->SetSelected(true);
	m_pInButton->SetSelected(false);
	m_pOutButton->SetSelected(false);
	m_pFPSButton->SetSelected(false);
	m_pPINGButton->SetSelected(false);

	LoadControlSettings("Admin/GraphPanel.res", "PLATFORM");

	int w,h;
	m_pGraphsPanel->GetSize(w,h);
	m_pGraphs->SaveSize(w,h);
	m_pGraphs->SetDraw(m_pCPUButton->IsSelected(),m_pFPSButton->IsSelected(),
	m_pInButton->IsSelected(),m_pOutButton->IsSelected(),m_pPINGButton->IsSelected(),m_pPlayerButton->IsSelected());

	m_pPINGButton->SetFgColor(m_pGraphs->GetPingColor());
	m_pCPUButton->SetFgColor(m_pGraphs->GetCPUColor());
	m_pFPSButton->SetFgColor(m_pGraphs->GetFPSColor());
	m_pInButton->SetFgColor(m_pGraphs->GetInColor());
	m_pOutButton->SetFgColor(m_pGraphs->GetOutColor());
	m_pPlayerButton->SetFgColor(m_pGraphs->GetPlayersColor());

	ivgui()->AddTickSignal(GetVPanel());
	m_flNextStatsUpdateTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGraphPanel::~CGraphPanel()
{

}

void CGraphPanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_pGraphsPanel->SetBorder(	pScheme->GetBorder("ButtonDepressedBorder"));
	m_pGraphs->SetBgColor(GetSchemeColor("WindowBG", pScheme));
	m_pGraphs->SetAxisColor(Color(76,88,68,255));

}

//-----------------------------------------------------------------------------
// Purpose: Activates the page
//-----------------------------------------------------------------------------
void CGraphPanel::OnPageShow()
{
	BaseClass::OnPageShow();
}

//-----------------------------------------------------------------------------
// Purpose: Hides the page
//-----------------------------------------------------------------------------
void CGraphPanel::OnPageHide()
{
	BaseClass::OnPageHide();
}

//-----------------------------------------------------------------------------
// Purpose: called every frame to update stats page
//-----------------------------------------------------------------------------
void CGraphPanel::OnTick()
{
	if (m_flNextStatsUpdateTime > system()->GetFrameTime())
		return;

	m_flNextStatsUpdateTime = (float)system()->GetFrameTime() + STATS_UPDATE_RATE;
	RemoteServer().RequestValue(this, "stats");	
}

//-----------------------------------------------------------------------------
// Purpose: tells the image about the new size
//-----------------------------------------------------------------------------
void CGraphPanel::PerformLayout()
{
	BaseClass::PerformLayout();
	int w,h,x,y;
	m_pGraphsPanel->GetBounds(x,y,w,h);

	m_pGraphs->SaveSize(w,h); // tell the image about the resize

	// push the mid axis label to the middle of the image
	Label *entry = dynamic_cast<Label *>(FindChildByName("AxisMid"));
	if (entry)
	{
		int entry_x,entry_y;
		entry->GetPos(entry_x,entry_y);
		entry->SetPos(entry_x,y+(h/2)-8);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles stats command returns
//-----------------------------------------------------------------------------
void CGraphPanel::OnServerDataResponse(const char *value, const char *response)
{
	if (!stricmp(value, "stats"))
	{
		// parse the stats out of the response
		Points_t p;
		float uptime, users;
		sscanf(response, "%f %f %f %f %f %f %f", &p.cpu, &p.in, &p.out, &uptime, &users, &p.fps, &p.players);
		p.cpu = p.cpu / 100; // its given as a value between 0<x<100, we want 0<x<1
		p.ping = 0;
		p.time = (float)system()->GetCurrentTime();
		m_pGraphs->AddPoint(p);

		// days:hours:minutes:seconds
		char timeText[64];
		_snprintf(timeText, sizeof(timeText), "%i", (int)p.players);
		SetControlString("TotalUsersLabel", timeText);

		// mark the vert combo has changed to force it to update graph ranges
		m_pVertCombo->GetText(timeText, 64);
		OnTextChanged(m_pVertCombo, timeText);
	}
}

//-----------------------------------------------------------------------------
// Purpose: the paint routine for the graph image. Handles the layout and drawing of the graph image
//-----------------------------------------------------------------------------
void CGraphPanel::CGraphsImage::Paint()
{
	int x,y;
	float distPoints; // needs to be a float, rounding errors cause problems with lots of points
	int bottom=5; // be 5 pixels above the bottom
	int left=2;

	int *pCpuX=NULL, *pCpuY=NULL;
	int *pInX=NULL, *pInY=NULL;
	int *pOutX=NULL, *pOutY=NULL;
	int *pFPSX=NULL, *pFPSY=NULL;
	int *pPingX=NULL, *pPingY=NULL;
	int *pPlayersX=NULL, *pPlayersY=NULL;

	GetSize(x,y);

	SetColor(bgColor);
	SetBkColor(bgColor);
	DrawFilledRect(0,0,x,y);

	y-=4; // borders
	x-=4;


	if(!cpu && !fps && !net_i && !net_o && !ping && !players) // no graphs selected
		return; 

	if(points.Count()<2)
		return; // not enough points yet...

	if(x<=200 || y<=100) 
		return; // to small


	distPoints= static_cast<float>(x)/static_cast<float>(points.Count()-1);
	if(distPoints<=0)
	{
		distPoints=1;
	}

	SetColor(lineColor);
	SetBkColor(lineColor);
	//DrawLine(4,5,x,5);
	DrawLine(4,y/2,x,y/2);
	//DrawLine(4,y,x,y);

	float RangePing=maxPing;
	float RangeFPS=maxFPS;
	float Range=0;
	float RangePlayers=maxPlayers;


	if(ping)
	{
		RangePing+=static_cast<float>(maxPing*0.1); // don't let the top of the range touch the top of the panel

		if(RangePing<=1) 
		{ // don't let the zero be at the top of the screen
			RangePing=1.0;
		}
		pPingX = new int[points.Count()];
		pPingY = new int[points.Count()];
	}

	if(cpu)
	{
		pCpuX = new int[points.Count()];
		pCpuY = new int[points.Count()];
	}
	
	if(fps)
	{			
		RangeFPS+=static_cast<float>(maxFPS*0.1); // don't let the top of the range touch the top of the panel

		if(RangeFPS<=1) 
		{ // don't let the zero be at the top of the screen
			RangeFPS=1.0;
		}
		pFPSX = new int[points.Count()];
		pFPSY = new int[points.Count()];
	}

	if(net_i)
	{
	
		// put them on a common scale, base it at zero 
		Range = max(maxIn,maxOut);
			
		Range+=static_cast<float>(Range*0.1); // don't let the top of the range touch the top of the panel

		if(Range<=1) 
		{ // don't let the zero be at the top of the screen
			Range=1.0;
		}

		pInX = new int[points.Count()];
		pInY = new int[points.Count()];
	}

	if(net_o)
	{
		// put them on a common scale, base it at zero 
		Range = max(maxIn,maxOut);
			
		Range+=static_cast<float>(Range*0.1); // don't let the top of the range touch the top of the panel

		if(Range<=1) 
		{ // don't let the zero be at the top of the screen
			Range=1.0;
		}

		pOutX = new int[points.Count()];
		pOutY = new int[points.Count()];
	}

	if(players)
	{
		RangePlayers+=static_cast<float>(maxPlayers*0.1); // don't let the top of the range touch the top of the panel

		pPlayersX = new int[points.Count()];
		pPlayersY = new int[points.Count()];
	}

	for(int i=0;i<points.Count();i++)
	// draw the graphs, left to right
	{
	
		if(cpu) 
		{
			pCpuX[i] = left+static_cast<int>(i*distPoints);
			pCpuY[i] = static_cast<int>((1-points[i].cpu)*y);
		}
	
		if(net_i)
		{
			pInX[i] = left+static_cast<int>(i*distPoints);
			pInY[i] = static_cast<int>(( (Range-points[i].in)/Range)*y-bottom);
		}
		
		if(net_o)
		{
			pOutX[i] = left+static_cast<int>(i*distPoints);
			pOutY[i] = static_cast<int>(((Range-points[i].out)/Range)*y-bottom);
		}

		if(fps)
		{
			pFPSX[i] = left+static_cast<int>(i*distPoints);
			pFPSY[i] = static_cast<int>(( (RangeFPS-points[i].fps)/RangeFPS)*y-bottom);
		}

		if(ping)
		{
			pPingX[i] = left+static_cast<int>(i*distPoints);
			pPingY[i] = static_cast<int>(( (RangePing-points[i].ping)/RangePing)*y-bottom);
		}

		if(players)
		{
			pPlayersX[i] = left+static_cast<int>(i*distPoints);
			pPlayersY[i] = static_cast<int>(( (RangePlayers-points[i].players)/RangePlayers)*y-bottom);
		}

	
	}
	// we use DrawPolyLine, its much, much, much more efficient than calling lots of DrawLine()'s

	if(cpu)
	{
		SetColor(CPUColor); // green
		DrawPolyLine(pCpuX, pCpuY, points.Count());	
		delete [] pCpuX;
		delete [] pCpuY;
	} 

	if(net_i) 
	{
		SetColor(NetInColor); // red
		DrawPolyLine(pInX, pInY, points.Count());	
		delete [] pInX;
		delete [] pInY;
	}
	if(net_o)
	{
		SetColor(NetOutColor); //yellow
		DrawPolyLine(pOutX, pOutY, points.Count());	
		delete [] pOutX;
		delete [] pOutY;
	}
	if(fps)
	{
		SetColor(FPSColor);
		DrawPolyLine(pFPSX, pFPSY, points.Count());	
		delete [] pFPSX;
		delete [] pFPSY;
	}

	if(ping)
	{
		SetColor(PingColor);
		DrawPolyLine(pPingX, pPingY, points.Count());	
		delete [] pPingX;
		delete [] pPingY;
	}

	if(players)
	{
		SetColor(PlayersColor);
		DrawPolyLine(pPlayersX, pPlayersY, points.Count());	
		delete [] pPlayersX;
		delete [] pPlayersY;
	}
}  


//-----------------------------------------------------------------------------
// Purpose: constructor for the graphs image
//-----------------------------------------------------------------------------
CGraphPanel::CGraphsImage::CGraphsImage(): vgui::Image(), points()
{
	maxIn=maxOut=minIn=minOut=minFPS=maxFPS=minPing=maxPing=0;
	net_i=net_o=fps=cpu=ping=players=false;
	numAvgs=0;
	memset(&avgPoint,0x0,sizeof(Points_t));
}

//-----------------------------------------------------------------------------
// Purpose: sets which graph to draw, true means draw it
//-----------------------------------------------------------------------------
void CGraphPanel::CGraphsImage::SetDraw(bool cpu_in,bool fps_in,bool net_in,bool net_out,bool ping_in,bool players_in)
{
	cpu=cpu_in;
	fps=fps_in;
	net_i=net_in;
	net_o=net_out;
	ping=ping_in;
	players=players_in;
}

//-----------------------------------------------------------------------------
// Purpose: used to average points over a period of time
//-----------------------------------------------------------------------------
void CGraphPanel::CGraphsImage::AvgPoint(Points_t p)
{
	avgPoint.cpu += p.cpu;
	avgPoint.fps += p.fps;
	avgPoint.in += p.in;
	avgPoint.out += p.out;
	avgPoint.ping += p.ping;
	avgPoint.players += p.players;
	numAvgs++;
}


//-----------------------------------------------------------------------------
// Purpose: updates the current bounds of the points based on this new point
//-----------------------------------------------------------------------------
void CGraphPanel::CGraphsImage::CheckBounds(Points_t p)
{
	if(p.in>maxIn)
	{
		maxIn=avgPoint.in;
	}
	if(p.out>maxOut)
	{
		maxOut=avgPoint.out;
	}

	if(p.in<minIn)
	{
		minIn=avgPoint.in;
	}
	if(p.out<minOut)
	{
		minOut=avgPoint.out;
	}

	if(p.fps>maxFPS)
	{
		maxFPS=avgPoint.fps;
	}
	if(p.fps<minFPS)
	{
		minFPS=avgPoint.fps;
	}

	if(p.ping>maxPing)
	{
		maxPing=avgPoint.ping;
	}
	if(p.ping<minPing)
	{
		minPing=avgPoint.ping;
	}

	if(p.players>maxPlayers)
	{
		maxPlayers=avgPoint.players;
	}
	if(p.players<minPlayers)
	{
		minPlayers=avgPoint.players;
	}
	
}

//-----------------------------------------------------------------------------
// Purpose: adds a point to the graph image. 
//-----------------------------------------------------------------------------
bool CGraphPanel::CGraphsImage::AddPoint(Points_t p)
{
	int x,y;
	bool recalcBounds=false;

	GetSize(x,y);
	
	if(avgPoint.cpu>1)  // cpu is a percent !
	{
		return false;
	}

	if(timeBetween==SECONDS) // most recent minute
	{
		while(points.Count() && (p.time-points[0].time)>60)
		{
			points.Remove(0);
		}
	}
	else if(timeBetween==HOURS) // most recent hour
	{
		while(points.Count() && (p.time-points[0].time)>60*60)
		{
			points.Remove(0);
		}
	} 
	else if ( timeBetween==MINUTES) // most recent day
	{
			while(points.Count() && (p.time-points[0].time)>60*60*24)
		{
			points.Remove(0);
		}
	}

	AvgPoint(p);
	// now work out the average of all the values
	avgPoint.cpu /= numAvgs;
	avgPoint.fps /= numAvgs;
	avgPoint.in  /= numAvgs;
	avgPoint.out /= numAvgs;
	avgPoint.ping /= numAvgs;
	avgPoint.players /= numAvgs;
	avgPoint.time = p.time;
	numAvgs=0;

	int k=0;

	if(x!=0 && points.Count()> x/2) 
	// there are more points than pixels so thin them out
	{
		while(points.Count()> x/2)
		{
			// check that the bounds don't move
			if(points[0].in==maxIn ||
				points[0].out==maxOut ||
				points[0].fps==maxFPS ||
				points[0].ping==maxPing ||
				points[0].players==maxPlayers)
			{
				recalcBounds=true;
			}
			points.Remove(k); // remove the head node
			k+=2;
			if(k>points.Count()) 
			{
				k=0;
			}
		}
	}

	if(recalcBounds) 
	{
		for(int i=0;i<points.Count();i++)
		{
			CheckBounds(points[i]);
		}
	}


	CheckBounds(avgPoint);

	points.AddToTail(avgPoint);
	
	memset(&avgPoint,0x0,sizeof(Points_t));

	return true;
}

void CGraphPanel::CGraphsImage::SetScale(intervals time)
{
	timeBetween=time;

	// scale is reset so remove all the points
	points.RemoveAll();

	// and reset the maxes
	maxIn=maxOut=minIn=minOut=minFPS=maxFPS=minPing=maxPing=maxPlayers=minPlayers=0;

}


//-----------------------------------------------------------------------------
// Purpose: clear button handler, clears the current points
//-----------------------------------------------------------------------------
void CGraphPanel::OnClearButton()
{
	m_pGraphs->RemovePoints();
}

//-----------------------------------------------------------------------------
// Purpose: passes the state of the check buttons (for graph line display) through to the graph image
//-----------------------------------------------------------------------------
void CGraphPanel::OnCheckButton()
{
	m_pGraphs->SetDraw(m_pCPUButton->IsSelected(), m_pFPSButton->IsSelected(), m_pInButton->IsSelected(), m_pOutButton->IsSelected(), m_pPINGButton->IsSelected(), m_pPlayerButton->IsSelected());
}

//-----------------------------------------------------------------------------
// Purpose:Handles the scale radio buttons, passes the scale to use through to the graph image
//-----------------------------------------------------------------------------
void CGraphPanel::OnTextChanged(Panel *panel, const char *text)
{
	if (panel == m_pTimeCombo)
	{
		if (strstr(text, "Hour"))
		{
			m_pGraphs->SetScale(MINUTES);
		}
		else if (strstr(text, "Day"))
		{
			m_pGraphs->SetScale(HOURS);
		}
		else
		{
			m_pGraphs->SetScale(SECONDS);
		}
	}
	else if (panel == m_pVertCombo)
	{
		float maxVal, minVal;
		char minText[20], midText[20], maxText[20];

		if (strstr(text, "CPU"))
		{
			SetAxisLabels(m_pGraphs->GetCPUColor(), "100%", "50%", "0%");
		}
		else if (strstr(text, "FPS"))
		{
			m_pGraphs->GetFPSLimits(maxVal, minVal);
			sprintf(maxText,"%0.2f", maxVal);
			sprintf(midText,"%0.2f", (maxVal - minVal) / 2);
			sprintf(minText,"%0.2f", minVal);
			SetAxisLabels(m_pGraphs->GetFPSColor(), maxText, midText, minText);
		}
		else if (strstr(text, "In"))
		{
			m_pGraphs->GetInLimits(maxVal, minVal);
			sprintf(maxText,"%0.2f", maxVal);
			sprintf(midText,"%0.2f", (maxVal - minVal) / 2);
			sprintf(minText,"%0.2f", minVal);

			SetAxisLabels(m_pGraphs->GetInColor(), maxText, midText, minText);
		}
		else if (strstr(text, "Out"))
		{
			m_pGraphs->GetOutLimits(maxVal, minVal);
			sprintf(maxText,"%0.2f", maxVal);
			sprintf(midText,"%0.2f", (maxVal - minVal) / 2);
			sprintf(minText,"%0.2f", minVal);
			SetAxisLabels(m_pGraphs->GetOutColor(), maxText, midText, minText);
		}
		else if (strstr(text, "Ping"))
		{
			m_pGraphs->GetPingLimits(maxVal, minVal);
			sprintf(maxText,"%0.2f", maxVal);
			sprintf(midText,"%0.2f", (maxVal - minVal) / 2);
			sprintf(minText,"%0.2f", minVal);
			SetAxisLabels(m_pGraphs->GetPingColor(), maxText, midText, minText);
		}
		else if (strstr(text, "Players"))
		{
			m_pGraphs->GetPlayerLimits(maxVal, minVal);
			sprintf(maxText,"%0.2f", maxVal);
			sprintf(midText,"%0.2f", (maxVal - minVal) / 2);
			sprintf(minText,"%0.2f", minVal);

			SetAxisLabels(m_pGraphs->GetPlayersColor(), maxText, midText, minText);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGraphPanel::SetAxisLabels(Color c, char *max, char *mid, char *min)
{
	Label *lab;
	lab= GetLabel("AxisMax");
	if(lab)
	{
		lab->SetFgColor(c);
		lab->SetText(max);
	}
	lab = GetLabel("AxisMid");
	if(lab)
	{
		lab->SetFgColor(c);
		lab->SetText(mid);
	}
	lab = GetLabel("AxisMin");
	if(lab)
	{
		lab->SetFgColor(c);
		lab->SetText(min);
	}
}

Label *CGraphPanel::GetLabel(const char *name)
{
	Label *lab = dynamic_cast<Label *>(FindChildByName(name));
	return lab;
}
