#ifndef _INCLUDED_SWARMOPEDIA_TOPICS_H
#define _INCLUDED_SWARMOPEDIA_TOPICS_H
#ifdef _WIN32
#pragma once
#endif

// reads in the list of topics for the Swarmopedia
class SwarmopediaPanel;

class SwarmopediaTopics : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( SwarmopediaTopics, vgui::Panel );
public:
	SwarmopediaTopics();
	virtual ~SwarmopediaTopics();

	void LoadTopics();

	// fills in the target list panel with buttons for each of the entries in the desired list
	bool SetupList(SwarmopediaPanel* pPanel, const char *szDesiredList);

protected:
	KeyValues* GetSubkeyForList(const char *szListName);

private:
	KeyValues* m_pKeys;
};

SwarmopediaTopics* GetSwarmopediaTopics();

#endif _INCLUDED_SWARMOPEDIA_TOPICS_H