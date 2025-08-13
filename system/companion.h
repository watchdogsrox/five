//--------------------------------------------------------------------------------------
// companion.h
//--------------------------------------------------------------------------------------

#pragma once

//	Companion app only on Durango, Orbis and now PC
#if (RSG_DURANGO || RSG_ORBIS || RSG_PC) && !__FINAL
#define COMPANION_APP		1
#else
#define COMPANION_APP		0
#endif	//	(RSG_DURANGO || RSG_ORBIS || RSG_PC) && !__FINAL

#if COMPANION_APP

#include "system/xtl.h"
#include <stdio.h>
#include <algorithm>
#include <list>
#include <set>
#include <vector>

using namespace std;

#include "AutoLock.h"

#include "vector/vector3.h"
#include "atl/string.h"
#include "rline/rltitleid.h"

#include "companion_blips.h"
#include "companion_routes.h"

#if RSG_ORBIS
//	Extra types for Orbis
typedef unsigned long DWORD;
typedef unsigned short WCHAR;
typedef long HRESULT;
//	Success codes
#define S_OK                                   ((HRESULT)0L)
#define S_FALSE                                ((HRESULT)1L)
#endif	//	RSG_ORBIS

//	Different message types
enum
{
	MESSAGE_TYPE_BLIPS = 0, 
	MESSAGE_TYPE_ROUTE, 
};

typedef enum
{
	MAP_DISPLAY_NORMAL = 0, 
	MAP_DISPLAY_PROLOGUE, 
	MAP_DISPLAY_ISLAND, 

	MAP_DISPLAY_MAX
}eMapDisplay;

RAGE_DECLARE_CHANNEL(Companion)

#define CompanionAssert(cond)					RAGE_ASSERT(Companion,cond)
#define CompanionAssertf(cond,fmt,...)			RAGE_ASSERTF(Companion,cond,fmt,##__VA_ARGS__)
#define CompanionFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(Companion,cond,fmt,##__VA_ARGS__)
#define CompanionVerify(cond)					RAGE_VERIFY(Companion,cond)
#define CompanionVerifyf(cond,fmt,...)			RAGE_VERIFYF(Companion,cond,fmt,##__VA_ARGS__)
#define CompanionErrorf(fmt,...)					RAGE_ERRORF(Companion,fmt,##__VA_ARGS__)
#define CompanionWarningf(fmt,...)				RAGE_WARNINGF(Companion,fmt,##__VA_ARGS__)
#define CompanionDisplayf(fmt,...)				RAGE_DISPLAYF(Companion,fmt,##__VA_ARGS__)
#define CompanionDebugf1(fmt,...)				RAGE_DEBUGF1(Companion,fmt,##__VA_ARGS__)
#define CompanionDebugf2(fmt,...)				RAGE_DEBUGF2(Companion,fmt,##__VA_ARGS__)
#define CompanionDebugf3(fmt,...)				RAGE_DEBUGF3(Companion,fmt,##__VA_ARGS__)
#define CompanionLogf(severity,fmt,...)			RAGE_LOGF(Companion,severity,fmt,##__VA_ARGS__)

//--------------------------------------------------------------------------------------
// Companion Data class
//--------------------------------------------------------------------------------------
class CCompanionData
{
public:

	// Ctor/dtor
	CCompanionData();
	virtual ~CCompanionData();

	static CCompanionData* GetInstance();

	CCompanionBlipMessage* GetBlipMessage();
	CCompanionRouteMessage* GetRouteMessage();
	void RemoveOldBlipMessages(bool whenReady = true);
	void RemoveOldRouteMessages(bool whenReady = true);

	typedef inlist<CCompanionBlipNode, &CCompanionBlipNode::m_node> BlipLinkedList;
	typedef inlist<CCompanionBlipNode, &CCompanionBlipNode::m_node>::iterator BlipLinkedListIterator;
	BlipLinkedList m_blipLinkedList;

	typedef inlist<CCompanionRouteNode, &CCompanionRouteNode::m_node> RouteNodeLinkedList;
	typedef inlist<CCompanionRouteNode, &CCompanionRouteNode::m_node>::iterator RouteNodeLinkedListIterator;
	RouteNodeLinkedList m_routeNodeLinkedList;

	virtual void Initialise() = 0;
	virtual void Update();

	HRESULT AddBlip( DWORD nClientId, CBlipComplex* pBlip, Color32 colour );
	void RemoveBlip(CBlipComplex* pBlip);
	void UpdateGpsRoute(s16 routeId, s32 numNodes, Vector3* pNodes, Color32 colour);
	HRESULT AddGpsPoint( DWORD nClientId, s16 routeId, s16 id, double x, double y );
	bool IsFlagSet(const CMiniMapBlip *pBlip, u32 flag);

	bool IsConnected()
	{
		return m_isConnected;
	}

	void ClearRouteNodeCount(int routeId);
	void SetRouteNodeCount(int routeId, s16 nodeCount);
	s16 GetRouteNodeCount(int routeId);
	void CreateWaypoint(double x, double y);
	void RemoveWaypoint();
	void ResendAll();
	void TogglePause();

	void SetRouteColour(Color32 colour);
	Color32 GetRouteColour() { return m_routeColour; }
	void SetMapDisplay(eMapDisplay display);
	eMapDisplay GetMapDisplay() { return m_mapDisplay; }
	bool GetIsInPlay() { return m_isInPlay; }

	virtual bool SetTextModeForCurrentDevice(bool) { return false; }
	virtual void SetTextEntryForCurrentDevice(const atString&) {}

	bool m_createWaypoint;
	bool m_removeWaypoint;
	bool m_resendAll;
	bool m_togglePause;
	double m_newWaypointX;
	double m_newWaypointY;

	s16 m_routeNodeCount[2];
	Color32 m_routeColour;
	eMapDisplay m_mapDisplay;

private:

	bool m_isConnected;
	bool m_isInPlay;

	static CCompanionData* sm_instance;

	bool IsPlayerActive();

protected:

	// Helpers
	virtual void OnClientConnected();
	virtual void OnClientDisconnected();
};

#endif	//	COMPANION_APP
