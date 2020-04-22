//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "basetypes.h"
#include "changeframelist.h"
#include "dt.h"
#include "utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


class CChangeFrameList : public IChangeFrameList
{
public:

	void	Init( int nProperties, int iCurTick )
	{
		m_ChangeTicks.SetSize( nProperties );
		for ( int i=0; i < nProperties; i++ )
			m_ChangeTicks[i] = iCurTick;
	}


// IChangeFrameList implementation.
public:

	virtual void	Release()
	{
		delete this;
	}

	virtual IChangeFrameList* Copy()
	{
		CChangeFrameList *pRet = new CChangeFrameList;

		int numProps = m_ChangeTicks.Count();

		pRet->m_ChangeTicks.SetSize( numProps );

		for ( int i=0; i < numProps; i++ )
		{
			pRet->m_ChangeTicks[ i ] = m_ChangeTicks[ i ];
		}

		return pRet;

	}

	virtual int		GetNumProps()
	{
		return m_ChangeTicks.Count();
	}

	virtual void	SetChangeTick( const int *pPropIndices, int nPropIndices, const int iTick )
	{
		for ( int i=0; i < nPropIndices; i++ )
		{
			m_ChangeTicks[ pPropIndices[i] ] = iTick;
		}
	}

	virtual int		GetPropsChangedAfterTick( int iTick, int *iOutProps, int nMaxOutProps )
	{
		int nOutProps = 0;

		int c = m_ChangeTicks.Count();
		
		Assert( c <= nMaxOutProps );

		for ( int i=0; i < c; i++ )
		{
			if ( m_ChangeTicks[i] > iTick )
			{
				iOutProps[nOutProps] = i;
				++nOutProps;
			}
		}

		return nOutProps;
	}

// IChangeFrameList implementation.
protected:

	virtual			~CChangeFrameList()
	{
	}

private:
	// Change frames for each property.
	CUtlVector<int>		m_ChangeTicks;
};


IChangeFrameList* AllocChangeFrameList( int nProperties, int iCurTick )
{
	CChangeFrameList *pRet = new CChangeFrameList;
	pRet->Init( nProperties, iCurTick);
	return pRet;
}



