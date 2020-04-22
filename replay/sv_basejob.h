//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SV_BASEJOB_H
#define SV_BASEJOB_H

//----------------------------------------------------------------------------------------

#undef Yield

#include "spew.h"
#include "vstdlib/jobthread.h"

//----------------------------------------------------------------------------------------

class CBaseJob : public CJob,
				 public CBaseSpewer
{
public:
	CBaseJob( JobPriority_t priority = JP_NORMAL, ISpewer *pSpewer = g_pDefaultSpewer );

	enum Error_t
	{
		ERROR_NONE = -1,
	};

	const char *GetErrorStr() const { return m_szError; }

protected:
	void	SetError( int nError, const char *pError = NULL );

private:
	int				m_nError;
	char			m_szError[256];
};

//----------------------------------------------------------------------------------------

#endif // SV_BASEJOB_H
