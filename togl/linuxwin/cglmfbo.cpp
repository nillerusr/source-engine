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
// cglmfbo.cpp
//
//===============================================================================

#include "togl/rendermechanism.h"

// memdbgon -must- be the last include file in a .cpp file.
#include "tier0/memdbgon.h"

CGLMFBO::CGLMFBO( GLMContext *ctx )
{
	m_ctx = ctx;
	m_ctx->CheckCurrent();
	
	gGL->glGenFramebuffersEXT( 1, &m_name );
	
	memset( m_attach, 0, sizeof( m_attach ) );
}


CGLMFBO::~CGLMFBO( )
{
	m_ctx->CheckCurrent();

	// detach all known attached textures first... necessary ?
	for( int index = 0; index < kAttCount; index++)
	{
		if (m_attach[ index ].m_tex)
		{
			TexDetach( (EGLMFBOAttachment)index );
		}
	}
	
	gGL->glDeleteFramebuffersEXT( 1, &m_name );
	
	m_name = 0;
	m_ctx = NULL;
}

// the tex attach path should also select a specific slice of the texture...
// and we need a way to make renderbuffers..

static GLenum EncodeAttachmentFBO( EGLMFBOAttachment index )
{
	if (index < kAttDepth)
	{
		return GL_COLOR_ATTACHMENT0_EXT + (int) index;
	}
	else
	{
		switch( index )
		{
			case kAttDepth:
				return	GL_DEPTH_ATTACHMENT_EXT;
			break;

			case kAttStencil:
				return	GL_STENCIL_ATTACHMENT_EXT;
			break;
			
			case kAttDepthStencil:
				return	GL_DEPTH_STENCIL_ATTACHMENT_EXT;
			break;
			
			default:
				GLMStop(); // bad news
			break;
		}
	}
	
	GLMStop(); // bad news
	// shouldn't get here
	return GL_COLOR_ATTACHMENT0_EXT;
}

void	CGLMFBO::TexAttach( GLMFBOTexAttachParams *params, EGLMFBOAttachment attachIndex, GLenum fboBindPoint )
{
	// force our parent context to be current
	m_ctx->MakeCurrent();

	// bind to context (will cause FBO object creation on first use)
	m_ctx->BindFBOToCtx( this, fboBindPoint );
	
	// it's either a plain 2D, a 2D face of a cube map, or a slice of a 3D.
	CGLMTex			*tex = params->m_tex;
	
	// always detach what is currently there, if anything
	this->TexDetach( attachIndex, fboBindPoint );

	if (!tex)
	{
		// andif they pass NULL to us, then we are done.
		return;
	}
	
	GLMTexLayout	*layout = tex->m_layout;

	GLenum			target = tex->m_layout->m_key.m_texGLTarget;
	
	GLenum			attachIndexGL = EncodeAttachmentFBO( attachIndex );
	
	switch( target )
	{
		case GL_TEXTURE_2D:
		{
			// we will attach the underlying RBO on a multisampled tex, iff the tex has one, **and** we're not being asked to attach it to the read buffer.
			// if we get a req to attach an MSAA tex to the read buffer, chances are it's BlitTex calling, andit has already resolved the tex, so in those
			// cases you really do want to attach the texture and not the RBO to the FBO in question.
			
			bool useRBO = false;	// initial state
			
			if (layout->m_key.m_texFlags & kGLMTexMultisampled)
			{
				// it is an MSAA tex
				if (fboBindPoint == GL_READ_FRAMEBUFFER_EXT)
				{
					// I think you just want to read a resolved tex.
					// But I will check that it is resolved first..
					Assert( tex->IsRBODirty() == false );
				}
				else
				{
					// you want to draw into it.  You get the RBO bound instead of the tex.
					useRBO = true;
				}
			}
				
			if (useRBO)
			{
				// MSAA path - attach the RBO, not the texture, and mark the RBO dirty
				if (attachIndexGL==GL_DEPTH_STENCIL_ATTACHMENT_EXT)
				{
					// you have to attach it both places...
					// http://www.opengl.org/wiki/GL_EXT_framebuffer_object

					// bind the RBO to the GL_RENDERBUFFER_EXT target
					gGL->glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, tex->m_rboName );
										
					// attach the GL_RENDERBUFFER_EXT target to the depth and stencil attach points
					gGL->glFramebufferRenderbufferEXT( fboBindPoint, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, tex->m_rboName);						
											
					gGL->glFramebufferRenderbufferEXT( fboBindPoint, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, tex->m_rboName);
					
					// no need to leave the RBO hanging on
					gGL->glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, 0 );
				}
				else
				{
					// color attachment (likely 0)
					gGL->glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, tex->m_rboName );
										
					gGL->glFramebufferRenderbufferEXT( fboBindPoint, attachIndexGL, GL_RENDERBUFFER_EXT, tex->m_rboName);
					
					gGL->glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, 0 );
				}
				tex->ForceRBODirty();
			}
			else
			{
				// regular path - attaching a texture2d
				
				if (attachIndexGL==GL_DEPTH_STENCIL_ATTACHMENT_EXT)
				{
					// you have to attach it both places...
					// http://www.opengl.org/wiki/GL_EXT_framebuffer_object
					
					gGL->glFramebufferTexture2DEXT( fboBindPoint, GL_DEPTH_ATTACHMENT_EXT, target, tex->m_texName, params->m_mip );
					
					gGL->glFramebufferTexture2DEXT( fboBindPoint, GL_STENCIL_ATTACHMENT_EXT, target, tex->m_texName, params->m_mip );
				}
				else
				{
					gGL->glFramebufferTexture2DEXT( fboBindPoint, attachIndexGL, target, tex->m_texName, params->m_mip );
				}
			}
		}
		break;

		case GL_TEXTURE_3D:
		{			
			gGL->glFramebufferTexture3DEXT( fboBindPoint, attachIndexGL, target, tex->m_texName, params->m_mip, params->m_zslice );
		}
		break;

		case GL_TEXTURE_CUBE_MAP:
		{
			// adjust target to steer to the proper face of the cube map
			target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + params->m_face;
			
			gGL->glFramebufferTexture2DEXT( fboBindPoint, attachIndexGL, target, tex->m_texName, params->m_mip );
		}
		break;
	}
	
	// log the attached tex
	m_attach[ attachIndex ] = *params;
	
	// indicate that the tex has been bound to an RT
	tex->m_rtAttachCount++;
}

void	CGLMFBO::TexDetach( EGLMFBOAttachment attachIndex, GLenum fboBindPoint )
{
	// force our parent context to be current
	m_ctx->MakeCurrent();

	// bind to context (will cause FBO object creation on first use)
	m_ctx->BindFBOToCtx( this, fboBindPoint );

	if (m_attach[ attachIndex ].m_tex)
	{
		CGLMTex			*tex = m_attach[ attachIndex ].m_tex;
		GLMTexLayout	*layout = tex->m_layout;
		GLenum			target = tex->m_layout->m_key.m_texGLTarget;
		
		GLenum			attachIndexGL = EncodeAttachmentFBO( attachIndex );

		switch( target )
		{
			case GL_TEXTURE_2D:
			{
				if (layout->m_key.m_texFlags & kGLMTexMultisampled)
				{
					// MSAA path - detach the RBO, not the texture
					// (is this the right time to resolve?  probably better to wait until someone tries to sample the texture)

					gGL->glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, 0 );
						
					if (attachIndexGL==GL_DEPTH_STENCIL_ATTACHMENT_EXT)
					{
						// detach the GL_RENDERBUFFER_EXT target at depth and stencil attach points
						gGL->glFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);						
							
						gGL->glFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);
					}
					else
					{
						// color attachment (likely 0)
						gGL->glFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT, attachIndexGL, GL_RENDERBUFFER_EXT, 0);
					}
				}
				else
				{
					// plain tex detach
					if (attachIndexGL==GL_DEPTH_STENCIL_ATTACHMENT_EXT)
					{
						// you have to detach it both places...
						// http://www.opengl.org/wiki/GL_EXT_framebuffer_object
						
						gGL->glFramebufferTexture2DEXT( fboBindPoint, GL_DEPTH_ATTACHMENT_EXT, target, 0, 0 );
						gGL->glFramebufferTexture2DEXT( fboBindPoint, GL_STENCIL_ATTACHMENT_EXT, target, 0, 0 );
					}
					else
					{
						gGL->glFramebufferTexture2DEXT( fboBindPoint, attachIndexGL, target, 0, 0 );
					}
				}
			}
			break;

			case GL_TEXTURE_3D:
			{
				gGL->glFramebufferTexture3DEXT( fboBindPoint, attachIndexGL, target, 0, 0, 0 );
			}
			break;

			case GL_TEXTURE_CUBE_MAP:
			{
				gGL->glFramebufferTexture2DEXT( fboBindPoint, attachIndexGL, target, 0, 0 );
			}
			break;
		}
		
		// un-log the attached tex
		memset( &m_attach[ attachIndex ], 0, sizeof( m_attach[0] ) );
		
		// drop the RT attach count
		tex->m_rtAttachCount--;
	}
	else
	{
		//Debugger(); // odd, but not harmful - typ comes from D3D code passing NULL into SetRenderTarget
	}
}

void CGLMFBO::TexScrub( CGLMTex *tex )
{
	// see if it's attached anywhere
	for( int attachIndex = 0; attachIndex < kAttCount; attachIndex++ )
	{
		if (m_attach[ attachIndex ].m_tex == tex)
		{
			// blammo
			TexDetach( (EGLMFBOAttachment)attachIndex, GL_DRAW_FRAMEBUFFER_EXT );
		}
	}
}


bool	CGLMFBO::IsReady( void )
{
	bool result = false;
	
	// ensure our parent context is current
	m_ctx->CheckCurrent();
	
	// bind to context (will cause FBO object creation on first use)
	m_ctx->BindFBOToCtx( this );

	GLenum status;
	status = gGL->glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	switch(status)
	{
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			result = true;
		break;

		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			result = false;
			DebuggerBreak();
			/* choose different formats */
		break;

		default:
			result = false;
			DebuggerBreak();
			/* programming error; will fail on all hardware */
		break;
	}
	return result;
}
