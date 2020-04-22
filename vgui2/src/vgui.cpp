//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core implementation of vgui
//
// $NoKeywords: $
//===========================================================================//


#if defined( WIN32 ) && !defined( _X360 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "VGuiMatSurface/IMatSystemSurface.h"
#include <vgui/VGUI.h>
#include <vgui/Dar.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/IClientPanel.h>
#include <vgui/IScheme.h>
#include <KeyValues.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <tier0/dbg.h>
#include <tier1/utlhandletable.h>
#include "vgui_internal.h"
#include "VPanel.h"
#include "IMessageListener.h"
#include "tier3/tier3.h"
#include "utllinkedlist.h"
#include "utlpriorityqueue.h"
#include "utlvector.h"
#include "tier0/vprof.h"
#include "tier0/icommandline.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#undef GetCursorPos // protected_things.h defines this, and it makes it so we can't access g_pInput->GetCursorPos.

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


using namespace vgui;
static const int WARN_PANEL_NUMBER = 32768; // in DEBUG if more panels than this are created then throw an Assert, helps catch panel leaks


//-----------------------------------------------------------------------------
// Purpose: Single item in the message queue
//-----------------------------------------------------------------------------
struct MessageItem_t
{
	KeyValues *_params; // message data
						// _params->GetName() is the message name

	HPanel _messageTo;	// the panel this message is to be sent to
	HPanel _from;		// the panel this message is from (if any)
	float _arrivalTime;	// time at which the message should be passed on to the recipient

	int _messageID;		// incrementing message index
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool PriorityQueueComp(const MessageItem_t& x, const MessageItem_t& y) 
{
	if (x._arrivalTime > y._arrivalTime)
	{
		return true;
	}
	else if (x._arrivalTime < y._arrivalTime)
	{
		return false;
	}

	// compare messageID's to ensure we have the messages in the correct order
	return (x._messageID > y._messageID);
}



//-----------------------------------------------------------------------------
// Purpose: Implementation of core vgui functionality
//-----------------------------------------------------------------------------
class CVGui : public CTier3AppSystem< IVGui >
{
	typedef CTier3AppSystem< IVGui > BaseClass;

public:
	CVGui();
	~CVGui();

//-----------------------------------------------------------------------------
	// SRC specific stuff
	// Here's where the app systems get to learn about each other 
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();

	// Here's where systems can access other interfaces implemented by this object
	// Returns NULL if it doesn't implement the requested interface
	virtual void *QueryInterface( const char *pInterfaceName );

	// Init, shutdown
	virtual InitReturnVal_t Init();
	virtual void Shutdown();
	// End of specific interface
//-----------------------------------------------------------------------------


	virtual void RunFrame();

	virtual void Start()
	{
		m_bRunning = true;
	}

	// signals vgui to Stop running
	virtual void Stop()
	{
		m_bRunning = false;
	}

	// returns true if vgui is current active
	virtual bool IsRunning()
	{
		return m_bRunning;
	}

	virtual void ShutdownMessage(unsigned int shutdownID);

	// safe-pointer handle methods
	virtual VPANEL AllocPanel();
	virtual void FreePanel(VPANEL ipanel);
	virtual HPanel PanelToHandle(VPANEL panel);
	virtual VPANEL HandleToPanel(HPanel index);
	virtual void MarkPanelForDeletion(VPANEL panel);

	virtual void AddTickSignal(VPANEL panel, int intervalMilliseconds = 0);
	virtual void AddTickSignalToHead( VPANEL panel, int intervalMilliseconds = 0 ) OVERRIDE;
	virtual void RemoveTickSignal(VPANEL panel );


	// message pump method
	virtual void PostMessage(VPANEL target, KeyValues *params, VPANEL from, float delaySeconds = 0.0f);

	virtual void SetSleep( bool state ) { m_bDoSleep = state; };
	virtual bool GetShouldVGuiControlSleep() { return m_bDoSleep; }

	virtual void DPrintf(const char *format, ...);
	virtual void DPrintf2(const char *format, ...);
	virtual void SpewAllActivePanelNames();

	// Creates/ destroys vgui contexts, which contains information
	// about which controls have mouse + key focus, for example.
	virtual HContext CreateContext();
	virtual void DestroyContext( HContext context ); 

	// Associates a particular panel with a vgui context
	// Associating NULL is valid; it disconnects the panel from the context
	virtual void AssociatePanelWithContext( HContext context, VPANEL pRoot );

	// Activates a particular input context, use DEFAULT_VGUI_CONTEXT
	// to get the one normally used by VGUI
	virtual void ActivateContext( HContext context );

	// enables VR mode
	virtual void SetVRMode( bool bVRMode ) OVERRIDE
	{
		m_bVRMode = bVRMode;
	}
	virtual bool GetVRMode() OVERRIDE
	{
		return m_bVRMode;
	}

	bool IsDispatchingMessages( void )
	{
		return m_InDispatcher;
	}

private:
	// VGUI contexts
	struct Context_t
	{
		HInputContext m_hInputContext;
	};

	struct Tick_t
	{
		VPanel	*panel;
		int		interval;
		int		nexttick;
		bool	bMarkDeleted;
		// Debugging
		char	panelname[ 64 ];
	};

	Tick_t* CreateNewTick( VPANEL panel, int intervalMilliseconds );

	// Returns the current context
	Context_t *GetContext( HContext context );

	void PanelCreated(VPanel *panel);
	void PanelDeleted(VPanel *panel);
	bool DispatchMessages();
	void DestroyAllContexts( );
	void ClearMessageQueues();
	inline bool IsReentrant() const 
	{ 
		return m_nReentrancyCount > 0; 
	}

	// safe panel handle stuff
	CUtlHandleTable< VPanel, 20 > m_HandleTable;
	int m_iCurrentMessageID;

	bool m_bRunning : 1;
	bool m_bDoSleep : 1;
	bool m_InDispatcher : 1;
	bool m_bDebugMessages : 1;
	bool m_bVRMode : 1;
	bool m_bCanRemoveTickSignal : 1;
	int m_nReentrancyCount;

	CUtlVector< Tick_t * > m_TickSignalVec;
	CUtlLinkedList< Context_t >	m_Contexts;

	HContext m_hContext;
	Context_t m_DefaultContext;

#ifdef DEBUG
	int m_iDeleteCount, m_iDeletePanelCount;
#endif

	// message queue. holds all vgui messages generated by windows events
	CUtlLinkedList<MessageItem_t, ushort> m_MessageQueue;

	// secondary message queue, holds all vgui messages generated by vgui
	CUtlLinkedList<MessageItem_t, ushort> m_SecondaryQueue;

	// timing queue, holds all the messages that have to arrive at a specified time
	CUtlPriorityQueue<MessageItem_t> m_DelayedMessageQueue;
};

CVGui g_VGui;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CVGui, IVGui, VGUI_IVGUI_INTERFACE_VERSION, g_VGui);

bool IsDispatchingMessageQueue( void )
{
	return g_VGui.IsDispatchingMessages();
}

namespace vgui
{
IVGui *g_pIVgui = &g_VGui;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVGui::CVGui() : m_DelayedMessageQueue(0, 4, PriorityQueueComp)
{
	m_bRunning = false;
	m_InDispatcher = false;
	m_bDebugMessages = false;
	m_bDoSleep = true;
	m_bVRMode = false;
	m_bCanRemoveTickSignal = true;
	m_nReentrancyCount = 0;
	m_hContext = DEFAULT_VGUI_CONTEXT;
	m_DefaultContext.m_hInputContext = DEFAULT_INPUT_CONTEXT;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CVGui::~CVGui()
{
#ifdef _DEBUG
	int nCount = m_HandleTable.GetHandleCount();
	int nActualCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		UtlHandle_t h = m_HandleTable.GetHandleFromIndex( i );
		if ( m_HandleTable.IsHandleValid( h ) )
		{
			++nActualCount;
		}
	}

	if ( nActualCount > 0 )
	{
		Msg("Memory leak: panels left in CVGui::m_PanelList: %d\n", nActualCount );
	}
#endif // _DEBUG
}

//-----------------------------------------------------------------------------
// Purpose: Dumps out list of all active panels
//-----------------------------------------------------------------------------
void CVGui::SpewAllActivePanelNames()
{
	int nCount = m_HandleTable.GetHandleCount();
	for ( int i = 0; i < nCount; ++i )
	{
		UtlHandle_t h = m_HandleTable.GetHandleFromIndex( i );
		if ( m_HandleTable.IsHandleValid( h ) )
		{
			VPanel *pPanel = m_HandleTable.GetHandle( h );
			Msg("\tpanel '%s' of type '%s' leaked\n", g_pIPanel->GetName( (VPANEL)pPanel ), ((VPanel *)pPanel)->GetClassName());
		}
	}
}


//-----------------------------------------------------------------------------
// Creates/ destroys "input" contexts, which contains information
// about which controls have mouse + key focus, for example.
//-----------------------------------------------------------------------------
HContext CVGui::CreateContext()
{
	HContext i = m_Contexts.AddToTail();
	m_Contexts[i].m_hInputContext = g_pInput->CreateInputContext();
	return i;
}

void CVGui::DestroyContext( HContext context )
{
	Assert( context != DEFAULT_VGUI_CONTEXT );

	if ( m_hContext == context )
	{
		ActivateContext( DEFAULT_VGUI_CONTEXT );
	}

	g_pInput->DestroyInputContext( GetContext(context)->m_hInputContext );
	m_Contexts.Remove(context);
}

void CVGui::DestroyAllContexts( )
{
	HContext next;
	HContext i = m_Contexts.Head();
	while (i != m_Contexts.InvalidIndex())
	{
		next = m_Contexts.Next(i);
		DestroyContext( i );
		i = next;
	}
}


//-----------------------------------------------------------------------------
// Returns the current context
//-----------------------------------------------------------------------------
CVGui::Context_t *CVGui::GetContext( HContext context )
{
	if (context == DEFAULT_VGUI_CONTEXT)
		return &m_DefaultContext;
	return &m_Contexts[context];
}


//-----------------------------------------------------------------------------
// Associates a particular panel with a context
// Associating NULL is valid; it disconnects the panel from the context
//-----------------------------------------------------------------------------
void CVGui::AssociatePanelWithContext( HContext context, VPANEL pRoot )
{
	Assert( context != DEFAULT_VGUI_CONTEXT );
	g_pInput->AssociatePanelWithInputContext( GetContext(context)->m_hInputContext, pRoot );
}


//-----------------------------------------------------------------------------
// Activates a particular context, use DEFAULT_VGUI_CONTEXT
// to get the one normally used by VGUI
//-----------------------------------------------------------------------------
void CVGui::ActivateContext( HContext context )
{
	Assert( (context == DEFAULT_VGUI_CONTEXT) || m_Contexts.IsValidIndex(context) );

	if ( m_hContext != context )
	{
		// Clear out any messages queues that may be full...
		if ( !IsReentrant() )
		{
			DispatchMessages();
		}

		m_hContext = context;
		g_pInput->ActivateInputContext( GetContext(m_hContext)->m_hInputContext ); 

		if ( context != DEFAULT_VGUI_CONTEXT && !IsReentrant() )
		{
			g_pInput->RunFrame( );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Runs a single vgui frame, pumping all message to panels
//-----------------------------------------------------------------------------
void CVGui::RunFrame() 
{
	// NOTE: This can happen when running in Maya waiting for modal dialogs
	bool bIsReentrant = m_InDispatcher;
	if ( bIsReentrant )
	{
		++m_nReentrancyCount;
	}

#ifdef DEBUG
//  memory allocation debug helper
//	DPrintf( "Delete Count:%i,%i\n", m_iDeleteCount, m_iDeletePanelCount );
//	m_iDeleteCount = 	m_iDeletePanelCount = 0;
#endif

	// this will generate all key and mouse events as well as make a real repaint
	{
		VPROF( "surface()->RunFrame()" );
		g_pSurface->RunFrame();
	}

	// give the system a chance to process
	{
		VPROF( "system()->RunFrame()" );
		g_pSystem->RunFrame();
	}

	// update cursor positions
	if ( IsPC() && !IsReentrant() )
	{
		VPROF( "update cursor positions" );
		int cursorX, cursorY;
		g_pInput->GetCursorPosition(cursorX, cursorY);

		// this does the actual work given a x,y and a surface
		g_pInput->UpdateMouseFocus(cursorX, cursorY);

	}

	if ( !bIsReentrant )
	{
		VPROF( "input()->RunFrame()" );
		g_pInput->RunFrame();
	}

	// messenging
	if ( !bIsReentrant )
	{
		VPROF( "messaging" );

		// send all the messages waiting in the queue
		DispatchMessages();

		// Do the OnTicks before purging messages, since in previous code they were posted after dispatch and wouldn't hit
		//  until next frame
		int time = g_pSystem->GetTimeMillis();

		m_bCanRemoveTickSignal = false;

		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Ticks", __FUNCTION__ );
		// directly invoke tick all who asked to be ticked
		int count = m_TickSignalVec.Count();
		for (int i = count - 1; i >= 0; i-- )
		{
			Tick_t *t = m_TickSignalVec[i];
			if ( t->bMarkDeleted )
				continue;

			if ( t->interval != 0 )
			{
				if ( time < t->nexttick )
					continue;

				t->nexttick = time + t->interval;
			}
			
			t->panel->Client()->OnTick();
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Ticks: %s", __FUNCTION__, t->panel->Client()->GetName() );
		}

		m_bCanRemoveTickSignal = true;

		// get count again. panels could be added to tick vector in OnTick
		count = m_TickSignalVec.Count();

		// Remove all panels that tried to remove tick in OnTick
		for (int i = count - 1; i >= 0; i-- )
		{
			Tick_t *t = m_TickSignalVec[i];
			if ( t->bMarkDeleted )
			{
				m_TickSignalVec.Remove( i );
				delete t;
			}
		}
	}

	{
		VPROF( "SolveTraverse" );
		// make sure the hierarchy is up to date
		g_pSurface->SolveTraverse(g_pSurface->GetEmbeddedPanel());
		g_pSurface->ApplyChanges();
#ifdef WIN32
		Assert( IsX360() || ( IsPC() && _heapchk() == _HEAPOK ) );
#endif
	}

	if ( bIsReentrant )
	{
		--m_nReentrancyCount;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPANEL CVGui::AllocPanel()
{
#ifdef DEBUG
	m_iDeleteCount++;
#endif

	VPanel *panel = new VPanel;
	PanelCreated(panel);
	return (VPANEL)panel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVGui::FreePanel(VPANEL ipanel)
{
	PanelDeleted((VPanel *)ipanel);
	delete (VPanel *)ipanel;
#ifdef DEBUG
	m_iDeleteCount--;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Returns the safe index of the panel
//-----------------------------------------------------------------------------
HPanel CVGui::PanelToHandle(VPANEL panel)
{
	if (panel)
		return ((VPanel*)panel)->GetHPanel();
	return INVALID_PANEL;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the panel at the specified index
//-----------------------------------------------------------------------------
VPANEL CVGui::HandleToPanel(HPanel index)
{
	if ( !m_HandleTable.IsHandleValid( index ) )
	{
		return NULL;
	}
	return (VPANEL)m_HandleTable.GetHandle( (UtlHandle_t)index );
}


//-----------------------------------------------------------------------------
// Purpose: Called whenever a panel is constructed
//-----------------------------------------------------------------------------
void CVGui::PanelCreated(VPanel *panel)
{
	UtlHandle_t h = m_HandleTable.AddHandle();
	m_HandleTable.SetHandle( h, panel );

#if DUMP_PANEL_LIST
	int nCount = m_HandleTable.GetHandleCount();
	int nActualCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		UtlHandle_t h = m_HandleTable.GetHandleFromIndex( i );
		if ( m_HandleTable.IsHandleValid( h ) )
		{
			++nActualCount;
		}
	}

	if ( nActualCount >= WARN_PANEL_NUMBER )
	{
		FILE *file1 = fopen("panellist.txt", "w");
		if (file1 != NULL)
		{
			fprintf(file1, "Too many panels...listing them all.\n");
			int panelIndex;
			for (panelIndex = 0; panelIndex < nCount; ++panelIndex)
			{
				UtlHandle_t h = m_HandleTable.GetHandleFromIndex( i );
				VPanel *pPanel = m_HandleTable.GetHandle( h );
				IClientPanel *ipanel = ( pPanel ) ? pPanel->Client() : NULL;
				if ( ipanel )
					fprintf(file1, "panel %d: name: %s   classname: %s\n", panelIndex, ipanel->GetName(), ipanel->GetClassName());
				else
					fprintf(file1, "panel %d: can't get ipanel\n", panelIndex);
			}

			fclose(file1);
		}
	}

	Assert( nActualCount < WARN_PANEL_NUMBER );
#endif // DUMP_PANEL_LIST

	((VPanel *)panel)->SetHPanel( h );

	g_pSurface->AddPanel((VPANEL)panel);
}

//-----------------------------------------------------------------------------
// Purpose: instantly stops the app from pointing to the focus'd object
//			used when an object is being deleted
//-----------------------------------------------------------------------------
void CVGui::PanelDeleted(VPanel *focus)
{
	Assert( focus );
	g_pSurface->ReleasePanel((VPANEL)focus);
	g_pInput->PanelDeleted((VPANEL)focus);

	// remove from safe handle list
	UtlHandle_t h = ((VPanel *)focus)->GetHPanel();

	Assert( m_HandleTable.IsHandleValid(h) );
	if ( m_HandleTable.IsHandleValid(h) )
	{
		m_HandleTable.RemoveHandle( h );
	}

	((VPanel *)focus)->SetHPanel( INVALID_PANEL );

	// remove from tick signal dar
	RemoveTickSignal( (VPANEL)focus );
}

//-----------------------------------------------------------------------------
// Purpose: Creates or updates a tick signal for a panel.  Returns NULL if already ticking.
//-----------------------------------------------------------------------------
CVGui::Tick_t* CVGui::CreateNewTick( VPANEL panel, int intervalMilliseconds )
{
	Tick_t *t;
	// See if it's already in list
	int count = m_TickSignalVec.Count();
	for (int i = 0; i < count; i++ )
	{
		Tick_t *t = m_TickSignalVec[i];
		if ( t->panel == (VPanel *)panel )
		{
			// Go ahead and update intervals
			t->interval = intervalMilliseconds;
			t->nexttick = g_pSystem->GetTimeMillis() + t->interval;

			// Somebody added this panel back to the tick list, don't delete it
			t->bMarkDeleted = false;
			return NULL;
		}
	}

	// Add to list
	t = new Tick_t;

	t->panel = (VPanel *)panel;
	t->interval = intervalMilliseconds;
	t->nexttick = g_pSystem->GetTimeMillis() + t->interval;
	t->bMarkDeleted = false;

	if ( strlen( ((VPanel *)panel)->Client()->GetName() ) > 0 )
	{
		strncpy( t->panelname, ((VPanel *)panel)->Client()->GetName(), sizeof( t->panelname ) );
	}
	else
	{
		strncpy( t->panelname, ((VPanel *)panel)->Client()->GetClassName(), sizeof( t->panelname ) );
	}

	return t;
}

//-----------------------------------------------------------------------------
// Purpose: Adds the panel to the tail of a tick signal list, so the panel receives a message every frame
//-----------------------------------------------------------------------------
void CVGui::AddTickSignal(VPANEL panel, int intervalMilliseconds /*=0*/ )
{
	Tick_t* t = CreateNewTick( panel, intervalMilliseconds );

	if ( t )
	{
		// add the element to the end list 
		m_TickSignalVec.AddToTail( t );
		// panel is removed from list when deleted
	}
}

//-----------------------------------------------------------------------------
// Purpose: Adds the panel to the head of a tick signal list, so the panel receives a message every frame
//-----------------------------------------------------------------------------
void CVGui::AddTickSignalToHead(VPANEL panel, int intervalMilliseconds /*=0*/ )
{
	Tick_t* t = CreateNewTick( panel, intervalMilliseconds );

	if ( t )
	{
		// simply add the element to the head list 
		m_TickSignalVec.AddToHead( t );
		// panel is removed from list when deleted
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVGui::RemoveTickSignal( VPANEL panel )
{
	VPanel *search = (VPanel *)panel;

	// remove from tick signal dar
	int count = m_TickSignalVec.Count();

	for (int i = 0; i < count; i++ )
	{
		Tick_t *tick = m_TickSignalVec[i];
		if ( tick->panel == search )
		{
			if ( m_bCanRemoveTickSignal )
			{
				m_TickSignalVec.Remove( i );
				delete tick;
			}
			else
			{
				tick->bMarkDeleted = true;
			}
			
			return;
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: message pump
//			loops through and sends all active messages
//			note that more messages may be posted during the process
//-----------------------------------------------------------------------------
bool CVGui::DispatchMessages()
{
	int time = g_pSystem->GetTimeMillis();

	m_InDispatcher = true;
	bool doneWork = (m_MessageQueue.Count() > 12);

	bool bUsingDelayedQueue = (m_DelayedMessageQueue.Count() > 0);

	// Need two passes because we send the mouse move message after all
	// other messages are done, but the mouse move message may itself generate
	// some more messages
	int nPassCount = 0;
	while ( nPassCount < 2 )
	{
		while (m_MessageQueue.Count() > 0 || (m_SecondaryQueue.Count() > 0) || bUsingDelayedQueue)
		{
			// get the first message
			MessageItem_t *messageItem = NULL;
			int messageIndex = 0;

			// use the secondary queue until it empties. empty it after each message in the
			// primary queue. this makes primary messages completely resolve 
			bool bUsingSecondaryQueue = (m_SecondaryQueue.Count() > 0);
			if (bUsingSecondaryQueue)
			{
				doneWork = true;
				messageIndex = m_SecondaryQueue.Head();
				messageItem = &m_SecondaryQueue[messageIndex];
			}
			else if (bUsingDelayedQueue)
			{
				if (m_DelayedMessageQueue.Count() >0)
				{
					messageItem = (MessageItem_t*)&m_DelayedMessageQueue.ElementAtHead();
				}
				if (!messageItem || messageItem->_arrivalTime > time)
				{
					// no more items in the delayed message queue, move to the system queue
					bUsingDelayedQueue = false;
					continue;
				}
			}
			else
			{
				messageIndex = m_MessageQueue.Head();
				messageItem = &m_MessageQueue[messageIndex];
			}

			// message debug code 

			if ( m_bDebugMessages )
			{
				const char *qname = bUsingSecondaryQueue ? "Secondary" : "Primary";

				if (strcmp(messageItem->_params->GetName(), "Tick")
					&& strcmp(messageItem->_params->GetName(), "MouseFocusTicked") 
					&& strcmp(messageItem->_params->GetName(), "KeyFocusTicked")
					&& strcmp(messageItem->_params->GetName(), "CursorMoved"))
				{
					if (!stricmp(messageItem->_params->GetName(), "command"))
					{
						g_pIVgui->DPrintf2( "%s Queue dispatching command( %s, %s -- %i )\n", qname, messageItem->_params->GetName(), messageItem->_params->GetString("command"), messageItem->_messageID );
					}
					else
					{
						g_pIVgui->DPrintf2( "%s Queue dispatching( %s -- %i )\n", qname ,messageItem->_params->GetName(), messageItem->_messageID );
					}
				}
			}

			// send it
			KeyValues *params = messageItem->_params;

			// Deal with special internal cursor movement messages
			if ( messageItem->_messageTo == 0xFFFFFFFF )
			{
				if ( !Q_stricmp( params->GetName(), "SetCursorPosInternal" ) )
				{
					int nXPos = params->GetInt( "xpos", 0 );
					int nYPos = params->GetInt( "ypos", 0 );
					g_pInput->UpdateCursorPosInternal( nXPos, nYPos );
				}
			}
#ifdef _X360
			else if ( messageItem->_messageTo == 0xFFFFFFFE ) // special tag to always give message to the active key focus
			{
				VPanel *vto = (VPanel *) g_pInput->GetCalculatedFocus();
				if (vto)
				{
					vto->SendMessage(params, g_pIVgui->HandleToPanel(messageItem->_from));
				}
			}
#endif
			else
			{
				VPanel *vto = (VPanel *)g_pIVgui->HandleToPanel(messageItem->_messageTo);
				if (vto)
				{
					//			Msg("Sending message: %s to %s\n", params ? params->GetName() : "\"\"", vto->GetName() ? vto->GetName() : "\"\"");
					vto->SendMessage(params, g_pIVgui->HandleToPanel(messageItem->_from));
				}
			}

			// free the keyvalues memory
			// we can't reference the messageItem pointer anymore since the queue might have moved in memory
			if (params)
			{
				params->deleteThis();
			}

			// remove it from the queue
			if (bUsingSecondaryQueue)
			{
				m_SecondaryQueue.Remove(messageIndex);
			}
			else if (bUsingDelayedQueue)
			{
				m_DelayedMessageQueue.RemoveAtHead();
			}
			else
			{
				m_MessageQueue.Remove(messageIndex);
			}
		}

		++nPassCount;
		if ( nPassCount == 1 )
		{
			// Specifically post the current mouse position as a message
			g_pInput->PostCursorMessage();
		}
	}

	// Make sure the windows cursor is in the right place after processing input 
	// Needs to be done here because a message provoked by the cursor moved
	// message may move the cursor also
	g_pInput->HandleExplicitSetCursor( );

	m_InDispatcher = false;
	return doneWork;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVGui::MarkPanelForDeletion(VPANEL panel)
{
	PostMessage(panel, new KeyValues("Delete"), NULL);
}

//-----------------------------------------------------------------------------
// Purpose: Adds a message to the queue to be sent to a user
//-----------------------------------------------------------------------------
void CVGui::PostMessage(VPANEL target, KeyValues *params, VPANEL from, float delay)
{
	// Ignore all messages in re-entrant mode
	if ( IsReentrant() )
	{
		Assert( 0 );
		if (params)
		{
			params->deleteThis();
		}
		return;
	}

	if (!target)
	{
		if (params)
		{
			params->deleteThis();
		}
		return;
	}

	MessageItem_t messageItem;
	 
#ifdef _X360
	// Special coded target that will always send the message to the key focus
	// this is needed since we might send two messages on a tice, and the first
	// could change the focus.
	if( target == (VPANEL) MESSAGE_CURRENT_KEYFOCUS )
	{
		messageItem._messageTo = 0xFFFFFFFE;
	}
	else
#endif	
	{
		messageItem._messageTo = (target != (VPANEL) MESSAGE_CURSOR_POS ) ? g_pIVgui->PanelToHandle(target) : 0xFFFFFFFF;
	}
	messageItem._params = params;
	Assert(params->GetName());
	messageItem._from = g_pIVgui->PanelToHandle(from);
	messageItem._arrivalTime = 0;
	messageItem._messageID = m_iCurrentMessageID++;

	/* message debug code
	//if ( stricmp(messageItem._params->GetName(),"CursorMoved") && stricmp(messageItem._params->GetName(),"KeyFocusTicked"))
	{
		g_pIVgui->DPrintf2( "posting( %s -- %i )\n", messageItem._params->GetName(), messageItem._messageID );
	}
	*/
				
	// add the message to the correct message queue
	if (delay > 0.0f)
	{
		messageItem._arrivalTime = g_pSystem->GetTimeMillis() + (delay * 1000);
		m_DelayedMessageQueue.Insert(messageItem);
	}
	else if (m_InDispatcher)
	{
		m_SecondaryQueue.AddToTail(messageItem);
	}
	else
	{
		m_MessageQueue.AddToTail(messageItem);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVGui::ShutdownMessage(unsigned int shutdownID)
{
	// broadcast Shutdown to all the top level windows, and see if any take notice
	VPANEL panel = g_pSurface->GetEmbeddedPanel();
	for (int i = 0; i < ((VPanel *)panel)->GetChildCount(); i++)
	{
		g_pIVgui->PostMessage((VPANEL)((VPanel *)panel)->GetChild(i), new KeyValues("ShutdownRequest", "id", shutdownID), NULL);
	}

	// post to the top level window as well
	g_pIVgui->PostMessage(panel, new KeyValues("ShutdownRequest", "id", shutdownID), NULL);
}

//-----------------------------------------------------------------------------
// Purpose: Clears all the memory queues and free's their memory
//-----------------------------------------------------------------------------
void CVGui::ClearMessageQueues()
{
	Assert(!m_InDispatcher);

	{FOR_EACH_LL( m_MessageQueue, i )
	{
		if (m_MessageQueue[i]._params)
		{
			m_MessageQueue[i]._params->deleteThis();
		}
	}}
	m_MessageQueue.RemoveAll();

	// secondary message queue, holds all vgui messages generated by vgui
	{FOR_EACH_LL( m_SecondaryQueue, i )
	{
		if (m_SecondaryQueue[i]._params)
		{
			m_SecondaryQueue[i]._params->deleteThis();
		}
	}}
	m_SecondaryQueue.RemoveAll();

	// timing queue, holds all the messages that have to arrive at a specified time
	while (m_DelayedMessageQueue.Count() > 0)
	{
		if (m_DelayedMessageQueue.ElementAtHead()._params)
		{
			m_DelayedMessageQueue.ElementAtHead()._params->deleteThis();
		}
		m_DelayedMessageQueue.RemoveAtHead();
	}
}

/*
static void*(*staticMalloc)(size_t size)=malloc;
static void(*staticFree)(void* memblock)=free;

static int g_iMemoryBlocksAllocated = 0;

void *operator new(size_t size)
{
	g_iMemoryBlocksAllocated += 1;
	return staticMalloc(size);
}

void operator delete(void* memblock)
{
	if (!memblock)
		return;

	g_iMemoryBlocksAllocated -= 1;

	if (g_iMemoryBlocksAllocated < 0)
	{
		int x = 3;
	}

	staticFree(memblock);
}

void *operator new [] (size_t size)
{
	return staticMalloc(size);
}

void operator delete [] (void *pMem)
{
	staticFree(pMem);
}
*/

void CVGui::DPrintf(const char* format,...)
{
	char    buf[2048];
	va_list argList;

	va_start(argList,format);
	Q_vsnprintf(buf,sizeof( buf ), format,argList);
	va_end(argList);

#ifdef WIN32
	::OutputDebugString(buf);
#else
	Msg( "%s", buf );
#endif
}

void CVGui::DPrintf2(const char* format,...)
{
	char    buf[2048];
	va_list argList;
	static int ctr=0;

	Q_snprintf(buf,sizeof( buf ), "%d:",ctr++ );

	va_start(argList,format);
	Q_vsnprintf(buf+strlen(buf),sizeof( buf )-strlen(buf),format,argList);
	va_end(argList);

#ifdef WIN32
	::OutputDebugString(buf);
#else
	Msg( "%s", buf );
#endif
}

void vgui::vgui_strcpy(char* dst,int dstLen,const char* src)
{
	Assert(dst!=null);
	Assert(dstLen>=0);
	Assert(src!=null);

	int srcLen=strlen(src)+1;
	if(srcLen>dstLen)
	{
		srcLen=dstLen;
	}

	memcpy(dst,src,srcLen-1);
	dst[srcLen-1]=0;
}

//-----------------------------------------------------------------------------
	// HL2/TFC specific stuff
//-----------------------------------------------------------------------------
// Here's where the app systems get to learn about each other 
//-----------------------------------------------------------------------------
bool CVGui::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	if ( !g_pFullFileSystem || !g_pVGuiLocalize )
	{
		Warning( "IVGui unable to connect to required interfaces!\n" );
		return false;
	}

	return VGui_InternalLoadInterfaces( &factory, 1 );
}

void CVGui::Disconnect()
{
	// FIXME: Blat out interface pointers
	BaseClass::Disconnect();
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CVGui::Init()
{
	m_hContext = DEFAULT_VGUI_CONTEXT;
	m_bDebugMessages = CommandLine()->FindParm( "-vguimessages" ) ? true : false;

	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	return INIT_OK;
}

void CVGui::Shutdown()
{
	g_pSystem->SaveUserConfigFile();

	DestroyAllContexts();
	ClearMessageQueues();

	g_pSystem->Shutdown();
	g_pScheme->Shutdown(true);

	if ( !g_pSurface->QueryInterface( MAT_SYSTEM_SURFACE_INTERFACE_VERSION ) )
	{
		g_pSurface->Shutdown();
	}

	BaseClass::Shutdown();
}

//-----------------------------------------------------------------------------
// Here's where systems can access other interfaces implemented by this object
// Returns NULL if it doesn't implement the requested interface
//-----------------------------------------------------------------------------
void *CVGui::QueryInterface( const char *pInterfaceName )
{
	// FIXME: Should this go here?
	// Access other global interfaces exposed by this system...
	CreateInterfaceFn vguiFactory = Sys_GetFactoryThis();
	return vguiFactory( pInterfaceName, NULL );
}
