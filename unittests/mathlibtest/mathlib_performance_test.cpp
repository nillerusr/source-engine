//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program for CommandBuffer
//
// $NoKeywords: $
//=============================================================================//

#include "unitlib/unitlib.h"
#include "tier1/CommandBuffer.h"
#include "tier1/strtools.h"
#include "tier0/platform.h"
#include "tier0/fasttimer.h"


DEFINE_TESTSUITE( MathlibTestSuite )

DEFINE_TESTCASE( MathlibTestSSE, MathlibTestSuite )
{
	CFastTimer timer;
	timer.Start();

	float sum = 0.f;

	for( float a = 0.f; a <= M_PI; a += 0.000001f )
		sum += sinf(a);

	timer.End();

	Msg("cos Cycles: %llu\n", timer.GetDuration().GetLongCycles());
	Msg("cos sum - %f\n", sum);

	timer.Start();

	for( float a = 0.f; a <= M_PI; a += 0.000001f )
		sum += FastCos(a);

	timer.End();

	Msg("ssecos Cycles: %llu\n", timer.GetDuration().GetLongCycles());
	Msg("ssecos sum - %f\n", sum);
}
