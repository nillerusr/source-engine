//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UISCHEDULEDDEL_H
#define UISCHEDULEDDEL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utldelegate.h" 

namespace panorama
{


// Panorama wrapper around CUtlSymbol which always creates symbol in panorama.dll module
class CUIScheduledDel
{
public:
	// constructor, destructor
	CUIScheduledDel( CUtlDelegate< void() > del );
	~CUIScheduledDel();

	void Schedule( double flSecondsToRunIn ); 

	void Cancel();

	bool BScheduled();

	double GetSecondsUntilScheduled() { return MAX( m_flNextScheduled - UIEngine()->GetCurrentFrameTime(), 0.0 ); }

private:

	double m_flNextScheduled;
	int m_iScheduledIndex;
	CUtlDelegate< void() > m_del;
};

} // namespace panorama

#endif // UISCHEDULEDDEL_H