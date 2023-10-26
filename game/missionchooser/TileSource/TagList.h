#ifndef _INCLUDED_TAGLIST_H
#define _INCLUDED_TAGLIST_H

#include "utlvector.h"

// stores a list of tags (strings) that can be applied to room templates
// This is mainly used to provide a nice list of tagnames in the room template edit dialogue
class CTagList
{
public:
	CTagList();
	~CTagList();

	int GetNumTags() { return m_tags.Count(); }
	const char* GetTag( int i ) { return m_tags[i]; }
	const char* GetTagDescription( int i ) { return m_tagDescriptions[i]; }

	const char* FindTag( const char *TagName );
	const char* FindTagDescription( const char *TagName );

private:
	void LoadTags();

	CUtlVector<const char*> m_tags;
	CUtlVector<const char*> m_tagDescriptions;
};

// singleton accessor
CTagList* TagList();

#endif // _INCLUDED_TAGLIST_H