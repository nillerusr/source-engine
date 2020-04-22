//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>

#include <vgui/ISystem.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/ProgressBar.h>

using namespace vgui;

static const int TIMEOUT = 5000; // 5 second timeout

//-----------------------------------------------------------------------------
// Progress bars are used to illustrate a waiting period (such as 
// connecting to a server.
// Here we create a progress bar that fills up every 5 seconds.
//-----------------------------------------------------------------------------
class ProgressBarDemo: public DemoPage
{
	public:
		ProgressBarDemo(Panel *parent, const char *name);
		~ProgressBarDemo();

		void OnTick();
		void SetVisible(bool status);
		
	private:
		ProgressBar *m_pProgressBar;
		int m_iTimeoutTime;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ProgressBarDemo::ProgressBarDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	m_pProgressBar = new ProgressBar(this, "AProgressBar");
	m_pProgressBar->SetPos(100, 100);
	m_pProgressBar->SetWide(300);

	// This makes panel receive a 'Tick' message every frame 
	// (~50ms, depending on sleep times/framerate)
	// Panel is automatically removed from tick signal list when it's deleted
	ivgui()->AddTickSignal(this->GetVPanel());

	m_iTimeoutTime = 0;	 
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ProgressBarDemo::~ProgressBarDemo()
{
}

//-----------------------------------------------------------------------------
// Purpose: When the page is shown, initialize the progress bar.
//-----------------------------------------------------------------------------
void ProgressBarDemo::SetVisible(bool status)
{
	if (status)
	{
	// Set the timeout time.
	m_iTimeoutTime = system()->GetTimeMillis() + TIMEOUT;

	// Set progress bar to be none of the way done
	m_pProgressBar->SetProgress(0);

	}
	DemoPage::SetVisible(status);
}

//-----------------------------------------------------------------------------
// Purpose: Advances the status bar to the end, then starts over 
//-----------------------------------------------------------------------------
void ProgressBarDemo::OnTick()
{
	if (m_iTimeoutTime)
	{
		int currentTime = system()->GetTimeMillis();

		// Check for timeout
		if (currentTime > m_iTimeoutTime)
		{
			// Timed out, make a new timeout time
			m_iTimeoutTime = system()->GetTimeMillis() + TIMEOUT;
		}
		else
		{
			// Advance the status bar, in the range 0% to 100%
			float timePassedPercentage = (float)(currentTime - m_iTimeoutTime + TIMEOUT) / (float)TIMEOUT;
			m_pProgressBar->SetProgress(timePassedPercentage);
		}
	}
}


Panel* ProgressBarDemo_Create(Panel *parent)
{
	return new ProgressBarDemo(parent, "ProgressBarDemo");
}


