#include "shaderlib/cshader.h"
class depthwrite_ps20_Static_Index
{
public:
	depthwrite_ps20_Static_Index( )
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
#endif // _DEBUG
		return 0;
	}
};
#define shaderStaticTest_depthwrite_ps20 0
class depthwrite_ps20_Dynamic_Index
{
private:
	int m_nALPHACLIP;
#ifdef _DEBUG
	bool m_bALPHACLIP;
#endif
public:
	void SetALPHACLIP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nALPHACLIP = i;
#ifdef _DEBUG
		m_bALPHACLIP = true;
#endif
	}
	void SetALPHACLIP( bool i )
	{
		m_nALPHACLIP = i ? 1 : 0;
#ifdef _DEBUG
		m_bALPHACLIP = true;
#endif
	}
public:
	depthwrite_ps20_Dynamic_Index()
	{
#ifdef _DEBUG
		m_bALPHACLIP = false;
#endif // _DEBUG
		m_nALPHACLIP = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bALPHACLIP;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nALPHACLIP ) + 0;
	}
};
#define shaderDynamicTest_depthwrite_ps20 psh_forgot_to_set_dynamic_ALPHACLIP + 0
