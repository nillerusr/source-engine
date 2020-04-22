//========= Copyright Valve Corporation, All rights reserved. ============//
//                       TOGL CODE LICENSE
//
//  Copyright 2011-2014 Valve Corporation
//  All Rights Reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//
// cglmquery.cpp
//
//===============================================================================

#include "togl/rendermechanism.h"

#ifndef _WIN32
	#include <unistd.h>
#endif

// memdbgon -must- be the last include file in a .cpp file.
#include "tier0/memdbgon.h"

//===============================================================================

// http://www.opengl.org/registry/specs/ARB/occlusion_query.txt
// Workaround for "Calling either GenQueriesARB or DeleteQueriesARB while any query of any target is active causes an INVALID_OPERATION error to be generated."
uint CGLMQuery::s_nTotalOcclusionQueryCreatesOrDeletes;

extern ConVar gl_errorcheckall;
extern ConVar gl_errorcheckqueries;
extern ConVar gl_errorchecknone;

// how many microseconds to wait after a failed query-available test
// presently on MTGL this doesn't happen, but it could change, keep this handy
ConVar  gl_nullqueries( "gl_nullqueries", "0" );


//===============================================================================

CGLMQuery::CGLMQuery( GLMContext *ctx, GLMQueryParams *params )
{
	// get the type of query requested
	// generate name(s) needed
	// set initial state appropriately
	
	m_ctx = ctx;
	m_params = *params;

	m_name				=	0;
	m_syncobj			=	0;

	m_started = m_stopped = m_done = false;
	
	m_nullQuery = false;
		// assume value of convar at start time
		// does not change during individual query lifetime
		// started null = stays null
		// started live = stays live
	
	switch(m_params.m_type)
	{
		case EOcclusion:
		{
			//make an occlusion query (and a fence to go with it)
			gGL->glGenQueriesARB( 1, &m_name );
			s_nTotalOcclusionQueryCreatesOrDeletes++;
			GLMPRINTF(("-A-      CGLMQuery(OQ) created name %d", m_name));
		}
		break;

		case EFence:
			//make a fence - no aux fence needed

			m_syncobj = 0;

			if (gGL->m_bHave_GL_ARB_sync)
				{ /* GL_ARB_sync doesn't separate gen and set, so we do glFenceSync() later. */ }
			else if (gGL->m_bHave_GL_NV_fence)
				gGL->glGenFencesNV(1, &m_name );
			else if (gGL->m_bHave_GL_APPLE_fence)
				gGL->glGenFencesAPPLE(1, &m_name );

			GLMPRINTF(("-A-      CGLMQuery(fence) created name %d", m_name));
		break;
	}
	
}

CGLMQuery::~CGLMQuery()
{
	GLMPRINTF(("-A-> ~CGLMQuery"));
	
	// make sure query has completed (might not be necessary)
	// delete the name(s)

	switch(m_params.m_type)
	{
		case EOcclusion:
		{
			// do a finish occlusion query ?
			GLMPRINTF(("-A-      ~CGLMQuery(OQ) deleting name %d", m_name));
			gGL->glDeleteQueriesARB(1, &m_name );
			s_nTotalOcclusionQueryCreatesOrDeletes++;
		}
		break;

		case EFence:
		{
			// do a finish fence ?
			GLMPRINTF(("-A-      ~CGLMQuery(fence) deleting name %llu", gGL->m_bHave_GL_ARB_sync ? (unsigned long long) m_syncobj : (unsigned long long) m_name));
#ifdef HAVE_GL_ARB_SYNC
			if (gGL->m_bHave_GL_ARB_sync)
				gGL->glDeleteSync( m_syncobj );
			else 
#endif
			if (gGL->m_bHave_GL_NV_fence)
				gGL->glDeleteFencesNV(1, &m_name );
			else if (gGL->m_bHave_GL_APPLE_fence)
				gGL->glDeleteFencesAPPLE(1, &m_name );
		}
		break;
	}
	
	m_name = 0;
	m_syncobj = 0;

	GLMPRINTF(("-A-< ~CGLMQuery"));
}




void	CGLMQuery::Start( void )		// "start counting"
{
	m_nullQuery = (gl_nullqueries.GetInt() != 0);	// latch value for remainder of query life

	m_started = true;
	m_stopped = false;
	m_done = false;

	switch(m_params.m_type)
	{
		case EOcclusion:
		{
			if (m_nullQuery)
			{
				// do nothing..
			}
			else
			{
				gGL->glBeginQueryARB( GL_SAMPLES_PASSED_ARB, m_name );
			}
		}
		break;

		case EFence:
#ifdef HAVE_GL_ARB_SYNC		
			if (gGL->m_bHave_GL_ARB_sync)
			{
				if (m_syncobj != 0) gGL->glDeleteSync(m_syncobj);
				m_syncobj = gGL->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
			}
			else
#endif 
			if (gGL->m_bHave_GL_NV_fence)
				gGL->glSetFenceNV( m_name, GL_ALL_COMPLETED_NV );
			else if (gGL->m_bHave_GL_APPLE_fence)
				gGL->glSetFenceAPPLE( m_name );
			
			m_stopped = true;	// caller should not call Stop on a fence, it self-stops
		break;
	}
}

void	CGLMQuery::Stop( void )			// "stop counting"
{
	Assert(m_started);
	
	if ( m_stopped )
		return;

	switch(m_params.m_type)
	{
		case EOcclusion:
		{
			if (m_nullQuery)
			{
				// do nothing..
			}
			else
			{
				gGL->glEndQueryARB( GL_SAMPLES_PASSED_ARB );	// we are only putting the request-to-stop-counting into the cmd stream.
			}
		}
		break;

		case EFence:
			// nop - you don't "end" a fence, you just test it and/or finish it out in Complete
		break;
	}
	
	m_stopped = true;
}

bool	CGLMQuery::IsDone( void )
{
	Assert(m_started);
	Assert(m_stopped);

	if(!m_done)		// you can ask more than once, but we only check until it comes back as done.
	{
		// on occlusion: glGetQueryObjectivARB - large cost on pre SLGU, cheap after
		// on fence: glTestFence* on the fence
		switch(m_params.m_type)
		{
			case EOcclusion:	// just test the fence that was set after the query begin
			{
				if (m_nullQuery)
				{
					// do almost nothing.. but claim work is complete
					m_done = true;
				}
				else
				{
					// prepare to pay a big price on drivers prior to 10.6.4+SLGU
					
					GLint available = 0;
					gGL->glGetQueryObjectivARB(m_name, GL_QUERY_RESULT_AVAILABLE_ARB, &available );
					
					m_done = (available != 0);					
				}
			}
			break;

			case EFence:
			{
#ifdef HAVE_GL_ARB_SYNC
				if (gGL->m_bHave_GL_ARB_sync)
					m_done = (gGL->glClientWaitSync( m_syncobj, 0, 0 ) == GL_ALREADY_SIGNALED);
				else 
#endif
				if ( m_name == 0 )
					m_done = true;
				else if (gGL->m_bHave_GL_NV_fence)
					m_done = gGL->glTestFenceNV( m_name ) != 0;
				else if (gGL->m_bHave_GL_APPLE_fence)
					m_done = gGL->glTestFenceAPPLE( m_name ) != 0;

				if (m_done)
				{
					if (gGL->m_bHave_GL_ARB_sync)
						{ /* no-op; we already know it's set to GL_ALREADY_SIGNALED. */ }
					else
					{
						if (gGL->m_bHave_GL_NV_fence)
							gGL->glFinishFenceNV( m_name );	// no set fence goes un-finished
						else if (gGL->m_bHave_GL_APPLE_fence)
							gGL->glFinishFenceAPPLE( m_name );	// no set fence goes un-finished
					}
				}
			}
			break;
		}
	}
	
	return m_done;
}

void	CGLMQuery::Complete( uint *result )
{
	uint resultval = 0;
	//bool bogus_available = false;
	
	// blocking call if not done
	Assert(m_started);
	Assert(m_stopped);

	switch(m_params.m_type)
	{
		case EOcclusion:
		{
			if (m_nullQuery)
			{
				m_done = true;
				resultval = 0;		// we did say "null queries..."
			}
			else
			{
				gGL->glGetQueryObjectuivARB( m_name, GL_QUERY_RESULT_ARB, &resultval);
				m_done = true;
			}
		}
		break;

		case EFence:
		{
			if(!m_done)
			{
#ifdef HAVE_GL_ARB_SYNC				
				if (gGL->m_bHave_GL_ARB_sync)
				{
					if (gGL->glClientWaitSync( m_syncobj, 0, 0 ) != GL_ALREADY_SIGNALED)
					{
						GLenum syncstate;
						do {
							const GLuint64 timeout = 10 * ((GLuint64)1000 * 1000 * 1000);  // 10 seconds in nanoseconds.
							(void)timeout;
							syncstate = gGL->glClientWaitSync( m_syncobj, GL_SYNC_FLUSH_COMMANDS_BIT, 0 );
						} while (syncstate == GL_TIMEOUT_EXPIRED);  // any errors or success break out of this loop.
					}
				}
				else
#endif
				if (gGL->m_bHave_GL_NV_fence)
					gGL->glFinishFenceNV( m_name );
				else if (gGL->m_bHave_GL_APPLE_fence)
					gGL->glFinishFenceAPPLE( m_name );
				
				m_done = true;					// for clarity or if they try to Complete twice
			}
		}
		break;
	}

	Assert( m_done );
	
	// reset state for re-use - i.e. you have to call Complete if you want to re-use the object
	m_started = m_stopped = m_done = false;
	
	if (result)	// caller may pass NULL if not interested in result, for example to clear a fence
	{
		*result = resultval;
	}
}



	// accessors for the started/stopped state
bool	CGLMQuery::IsStarted	( void )
{
	return m_started;
}

bool	CGLMQuery::IsStopped	( void )
{
	return m_stopped;
}

