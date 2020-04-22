//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef _TF_GC_SHARED_H
#define _TF_GC_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/msgprotobuf.h"

using namespace GCSDK;

#define MMLog(...) do { Log( __VA_ARGS__ ); } while(false)

//-----------------------------------------------------------------------------
// ReliableMessage - A message/job class that retry until confirmed, and be sent
//                   In order with other such messages.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Check for pending messages
//-----------------------------------------------------------------------------
static bool BPendingReliableMessages();

//-----------------------------------------------------------------------------
GCSDK::CGCClientJob *s_pCurrentConfirmJob = NULL;
CUtlQueue< GCSDK::CGCClientJob * > s_queuePendingConfirmJobs;

template < typename RELIABLE_MSG_CLASS, typename MSG_TYPE, ETFGCMsg E_MSG_TYPE, typename REPLY_TYPE, ETFGCMsg E_REPLY_TYPE>
class CJobReliableMessageBase : public GCSDK::CGCClientJob
{
public:
	typedef CProtoBufMsg< MSG_TYPE >   Msg_t;
	typedef CProtoBufMsg< REPLY_TYPE > Reply_t;

	CJobReliableMessageBase()
		: GCSDK::CGCClientJob( GCClientSystem()->GetGCClient() )
		, m_msg( E_MSG_TYPE )
		, m_msgReply()
	{}

	Msg_t &Msg() { return m_msg; }
	void Enqueue()
	{
		static_cast<RELIABLE_MSG_CLASS *>(this)->InitDebugString( m_strDebug );
		MMLog( "[SendMsgUntilConfirmed] %s queued for %s\n", GetMsgName(), DebugString() );

		if ( !s_pCurrentConfirmJob )
		{
			s_pCurrentConfirmJob = this;
			this->StartJobDelayed( NULL );
		}
		else
		{
			// Queue, confirm jobs will kick next in queue as necessary
			s_queuePendingConfirmJobs.Insert( this );
		}
	}

	virtual bool BYieldingRunJob( void *pvStartParam )
	{
		Assert( s_pCurrentConfirmJob == this );
		bool bRet = BYieldingRunJobInternal();

		if ( s_queuePendingConfirmJobs.Count() )
		{
			// Kick off next job
			s_pCurrentConfirmJob = s_queuePendingConfirmJobs.RemoveAtHead();
			s_pCurrentConfirmJob->StartJob( NULL );
		}
		else
		{
			s_pCurrentConfirmJob = NULL;
		}

		return bRet;
	}

	bool BYieldingRunJobInternal()
	{
		MMLog( "[SendMsgUntilConfirmed] %s started for %s\n", GetMsgName(), DebugString() );

		// Trigger OnPrepare
		static_cast<RELIABLE_MSG_CLASS *>(this)->OnPrepare();

		for ( ;; )
		{
			BYieldingWaitOneFrame();

			// Create and load the message
			// continuously attempt to send the message to the GC
			BYldSendMessageAndGetReply_t result = BYldSendMessageAndGetReplyEx( m_msg, 30, &m_msgReply, E_REPLY_TYPE );

			switch ( result )
			{
			case BYLDREPLY_SUCCESS:
				MMLog( "[SendMsgUntilConfirmed] %s successfully sent for %s\n",
				       GetMsgName(), DebugString() );
				// Trigger OnReply
				static_cast<RELIABLE_MSG_CLASS *>(this)->OnReply( m_msgReply );
				return true;
			case BYLDREPLY_SEND_FAILED:
				MMLog( "[SendMsgUntilConfirmed] %s send FAILED for %s -- retrying\n",
				       GetMsgName(), DebugString() );
				break;
			case BYLDREPLY_TIMEOUT:
				MMLog( "[SendMsgUntilConfirmed] %s send TIMEOUT for %s -- retrying\n",
				       GetMsgName(), DebugString() );
				break;
			case BYLDREPLY_MSG_TYPE_MISMATCH:
				MMLog( "[SendMsgUntilConfirmed] %s send TYPE MISMATCH for %s\n",
				       GetMsgName(), DebugString() );
				Assert( !"Mismatched response type in reliable message" );
				return true;
			}
		}
	}

protected:
	// Overrides

	// Must be overridden by reliable message implementers. Debug string is e.g. "Match 12345, Lobby 4"
	void InitDebugString( CUtlString &debugStr ) {}
	const char *MsgName() { return "<unknown>"; }

	// Optionally overridden
	void OnReply( Reply_t &msgReply ) {}
	// Called before sending, after previous messages in queue have flushed
	void OnPrepare() {}

private:
	const char *DebugString() { return m_strDebug.Get(); }

	// Forward to override
	const char *GetMsgName() { return static_cast<RELIABLE_MSG_CLASS *>(this)->MsgName(); }

	Msg_t   m_msg;
	Reply_t m_msgReply;
	CUtlString m_strDebug;

	void _static_asserts() {
		// Ensure we passed an override and provided provided these
#if __cplusplus >= 201103L && !defined ( OSX ) // (Don't have time to figure out what criteria the OS X toolchain has to not blow this)
		static_assert( std::is_base_of< decltype( *this ), RELIABLE_MSG_CLASS >::value,
		               "RELIABLE_MSG_CLASS Must be an override of this base" );
		static_assert( !std::is_same< decltype( &(decltype( *this )::InitDebugString) ),
		               decltype( &RELIABLE_MSG_CLASS::InitDebugString ) >::value && \
		               !std::is_same< decltype( &(decltype( *this )::MsgName) ),
		               decltype( &RELIABLE_MSG_CLASS::MsgName ) >::value,
		               "RELIABLE_MSG_CLASS class must override DebugString and MsgName" );
#endif // __cplusplus >= 201103L && !defined ( OSX )
	}
};

static bool BPendingReliableMessages()
{
	Assert( !s_queuePendingConfirmJobs.Count() || s_pCurrentConfirmJob );
	return !!s_pCurrentConfirmJob || s_queuePendingConfirmJobs.Count();
}


#endif // _TF_GC_SHARED_H

