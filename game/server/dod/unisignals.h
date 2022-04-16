//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//


#ifndef __UNISIGNALS_H__
#define __UNISIGNALS_H__



// Unified signals allow state updating to be performed at one point in the
// calling code.  The sequence of events is:
//
//   1. Signal( whatever ), Signal( whatever ), etc.
//   2. ( GetState() & Update() ) to get only the changes since the last update
//   3. Goto 1
//
// Or, alternately:
//
//   1. Signal( whatever ), Signal( whatever ), etc.
//   2. Update()
//   3. GetState()
//   4. Goto 1


class CUnifiedSignals
{
public:
	inline CUnifiedSignals();
	inline int Update();				// Returns a mask for the changed state bits; signals are cleared after this update is performed
	inline void Signal( int flSignal );	// ORed combo of BPSIG_xxx bits
	inline int GetState();				// Returns the current state (ORed combo of BPSIG_xxx flags)

private:
	int m_flSignal;						// States to trigger
	int m_flState;						// States after the previous update
};



inline CUnifiedSignals::CUnifiedSignals()
{
	m_flSignal		= 0;
	m_flState		= 0;
}



inline int CUnifiedSignals::Update()
{
	int old		= m_flState;
	m_flState	= m_flSignal;
	m_flSignal	= 0;
	
	return m_flState ^ old;
}



inline void CUnifiedSignals::Signal( int flSignal )
{
	m_flSignal |= flSignal; 
}



inline int CUnifiedSignals::GetState()
{
	return m_flState; 
}



#endif // __UNISIGNALS_H__
