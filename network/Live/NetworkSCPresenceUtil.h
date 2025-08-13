//===========================================================================
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.             
//===========================================================================

#ifndef NETWORKSCPRESENCEUTIL_H
#define NETWORKSCPRESENCEUTIL_H

#include "atl/array.h"
#include "atl/string.h"
#include "atl/singleton.h"
#include "vectormath/vec3v.h"
#include "rline/scpresence/rlscpresence.h"

namespace rage 
{
	class snSession;
}

using namespace rage;

class NetworkSCPresenceUtil
{
public:
	NetworkSCPresenceUtil();

	static void UpdateMPSCPresenceInfo(const snSession& inSession);
	static void UpdateCurrentMission(const char* missionName);
	static void Update();

	static bool SetPresenceAttributeInt(const char* name, int value, bool bDefferCommit);
	static bool SetPresenceAttributeFloat(const char* name, float value, bool bDefferCommit);
	static bool SetPresenceAttributeString(const char* name, const char* value, bool bDefferCommit);

	static bool ClearAttribute(const char* name);
	static bool HasAttribute(const char* name);
	static bool IsAttributeSet(const char* name);

	//API for use from script to control what presence attributes they update
	bool Script_SetPresenceAttributeInt(int attributeID, int value );
	bool Script_SetPresenceAttributeFloat(int attributeID, float value );
	bool Script_SetPresenceAttributeString(int attributeID, const char* value );

	bool Script_SetActivityAttribute(int nActivityID, float rating);
	bool IsActivityAttributeSet(int nActivityID);

protected:

	void RegisterScriptAttributeIds();

	class PresenceAttrId
	{
	public:
		PresenceAttrId() : m_hashValue(0), m_defferedSecs(0) {	m_attrName[0] = '\0';	}
		PresenceAttrId(const char* name) 
			: m_hashValue(0) 
			, m_defferedSecs(0)
		{	
			m_attrName[0] = '\0';
			Set(name);
		}
		
		void Set(const char* name, int defferedDelaySecs = 0);
		bool IsDeffered() const { return m_defferedSecs > 0; }
		
		int m_hashValue;
		char m_attrName[RLSC_PRESENCE_ATTR_NAME_MAX_SIZE];
		u32 m_defferedSecs;
	};
	const PresenceAttrId* FindInfoForAttrId(int attributeId) const;

	static const int MAX_NUM_SCRIPT_PRESENCE_ATTRS = 16;
	atFixedArray<PresenceAttrId, MAX_NUM_SCRIPT_PRESENCE_ATTRS> m_AttrIdMap;

	struct ActivityPresenceAttr
	{
		ActivityPresenceAttr() : m_bIsSet(false), m_fActivityRating(0.0f) { }
		bool m_bIsSet;
		float m_fActivityRating;
	};

	static const int MAX_NUM_SCRIPT_ACTIVITY_ATTRS = 16;
	ActivityPresenceAttr m_aActivityAttributes[MAX_NUM_SCRIPT_ACTIVITY_ATTRS];

	u32 m_uLastUpdateTimeMs;
	Vec3V m_vLastUpdatePos;

	void UpdateCurrentPlayerLocation(bool bForce = false);
	void UpdateGameVersionPresence();

	//@NOTE we had to turn these WAY down for ship, but they are tunable via the tunables
	static const u32	sm_DEFAULT_PlayerLocUpdateIntervalSec = 0; //How often to run the checks (not necessarily do the update).  Saves us doing all the calculations.						 
	static const u32	sm_DEFAULT_PlayerLocUpdateTimeoutMs = 6000 * 60 * 1000;  //How long to wait to update the player loc, regardless.  Also scale the distance check with respect to speed.
	static const float	sm_DEFAULT_PlayerLocUpdateDist2;  //Min distance threshold for check. Initialized in the cpp

	u32	m_PlayerLocUpdateIntervalSec;
	u32	m_PlayerLocUpdateTimeoutMs;
	float m_PlayerLocUpdateDist2;
	bool m_bTunablesRecvd;				//Record that we've checked the tunables

	void UpdatePlayerLocationParamsFromTunables();

	void UpdateDefferedAttributes(bool bForce = false);

	static const int DEFAULT_DEFFERED_DELAY_SEC = 60;
	void AddDefferedAttribute(const char* name, int value, u32 secDelay = DEFAULT_DEFFERED_DELAY_SEC);
	void AddDefferedAttribute(const char* name, float value, u32 secDelay = DEFAULT_DEFFERED_DELAY_SEC);
	void AddDefferedAttribute(const char* name, const char* value, u32 secDelay = DEFAULT_DEFFERED_DELAY_SEC);

	class DefferedAttributeUpdate
	{
	public:
		DefferedAttributeUpdate() { Reset(); }
		~DefferedAttributeUpdate(){ Reset(); }
		DefferedAttributeUpdate& operator=(const DefferedAttributeUpdate& that);
		
		void Set(const char* name, u32 secDelay, int value);
		void Set(const char* name, u32 secDelay, float value);
		void Set(const char* name, u32 secDelay, const char* value);

		void UpdateValue(int value);
		void UpdateValue(float value);
		void UpdateValue(const char* value);
		
		enum eAttributeType {NONE, INT, FLOAT, STRING};

		void Reset()
		{
			m_type = NONE;
			m_asInt = 0;
			m_name[0] = '\0';
			m_updateSysTimeMs = 0;
			m_asString.Clear();
		}

		const char* GetName() const { return m_name; }
		eAttributeType GetType() const {return m_type;}

		int GetInt() const { return m_asInt; }
		float GetFloat() const { return m_asFloat; }
		const char* GetString() const { return m_asString.c_str(); }

		bool IsExpired(u32 currentTimeMs) const;
	protected:
		void SetName(const char*);
		void SetDelay(u32 secDelay);

		eAttributeType m_type;
		char m_name[32];
		u32 m_updateSysTimeMs;

		union
		{
			int m_asInt;
			float m_asFloat;
		};
		atString m_asString;		
	};

	class DefferedAttributeUpdateList
	{
	public:
		int GetCount() const { return m_defferedAttributes.GetCount(); }
		const DefferedAttributeUpdate& GetIndex(int index) { return m_defferedAttributes[index]; }
		DefferedAttributeUpdate& GetIndexNonConst(int index) { return m_defferedAttributes[index]; }
		int GetIndexByName(const char* name);

		void ClearAll();
		void ClearExpired(u32 currentSysTimeMs);
		DefferedAttributeUpdate& Add();
		
	protected:
		atArray<DefferedAttributeUpdate> m_defferedAttributes;
	};

	DefferedAttributeUpdateList m_defferedAttributeList;
};

typedef atSingleton<NetworkSCPresenceUtil> SCPresenceSingleton;
#define SCPRESENCEUTIL SCPresenceSingleton::GetInstance()

#endif // NETWORKSCPRESENCEUTIL_H
