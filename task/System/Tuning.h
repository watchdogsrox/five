//
// task/System/Tuning.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_SYSTEM_TUNING_H
#define TASK_SYSTEM_TUNING_H

#include "atl/array.h"
#include "data/base.h"
#include "parser/manager.h"

#if __BANK
#include "bank/bank.h"
#endif

class CTuning;
class CTunablesGroup;

//-----------------------------------------------------------------------------

/*.
PURPOSE
	Manager for tuning data, which keeps track of all objects derived from
	CTuning, organized in groups.
*/
class CTuningManager
{
	friend class CTuning;
	friend class CTunablesGroup;

public:
	CTuningManager();
	~CTuningManager();

	static void Init(unsigned int initMode);
	static void Shutdown(unsigned int shutdownMode);

	static void GetTuningIndexesFromHash(u32 uGroupHash, u32 uTuningHash, int &outGroupIdx, int &outItemIdx);
	static const CTuning* GetTuningFromIndexes(int iGroupIdx, int iItemIdx);
	static bool AppendPhysicsTasks(const char *fname);
	static bool RevertPhysicsTasks(const char *fname);
	BANK_ONLY(static void DebugOutputTuningList();)

#if __BANK
	static void CreateWidgets();
	void CreateWidgetsInternal();
#endif

protected:
	void RegisterTuning(CTuning& tunables, const char* pGroupName, const char* pBankName, bool bNonFinal, bool bUsePso);
	void UnregisterTuning(CTuning& tunables);
	void FinishInitialization();

	// PURPOSE:	Array of owned groups of CTuning objects.
	atArray<CTunablesGroup*>	m_TunableGroups;

	// PURPOSE:	Boolean used to temporarily disable the registration of tuning
	//			objects, used internally when loading.
	bool						m_RegistrationActive;

	static CTuningManager*		sm_Instance;

#if __BANK
	static bkButton*	ms_pCreateButton;
#endif
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	Base class for a tuning data object. When instantiated, it registers with the
	CTuningManager instance.
*/
class CTuning : public datBase
{
public:
	CTuning(const char* BANK_ONLY(pName), const u32 hash, const char* pGroupName, const char* pBankName, const bool nonFinalOnly = false, bool usePso = false)
		: m_Name(BANK_ONLY(pName,) hash)
	{
		RegisterTuning(pGroupName, pBankName, nonFinalOnly, usePso);
	}
	virtual ~CTuning();

	// PURPOSE:	Callback function that will be called after loading, either from PSO or from XML.
	virtual void Finalize() {}

	u32 GetHash() const			{	return m_Name.GetHash();	}

#if !__NO_OUTPUT
	const char* GetName() const	{	return m_Name.TryGetCStr();	}
#endif

	atHashString GetNameHashString() const { return m_Name; }
	void SetNameHashString(atHashString name) { m_Name = name ; }

protected:

	void RegisterTuning(const char* pGroupName, const char* BANK_ONLY(pBankName), const bool nonFinalOnly = false, bool usePso = false);

	// PURPOSE:	The name of the object. Generally this would be the name of the
	//			class the tuning data is to be used for.
	// NOTES:	The main reason for the name to exist is so that the same tuning
	//			data class can be used with multiple objects, to match up with
	//			the data found in the XML file.
	atHashString		m_Name;

	PAR_PARSABLE;
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	Container class used for loading sets of CTuning objects.
*/
class CTuningFile
{
public:
	CTuningFile() {}
	~CTuningFile();

	// PURPOSE:	Pointers to CTuning objects owned by this container.
	atArray<CTuning*> m_Tunables;

protected:
	// PURPOSE:	Intentionally unimplemented operators to prevent accidental copying.
	CTuningFile(const CTuningFile&);
	const CTuningFile& operator=(const CTuningFile&);

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	A group of objects containing tuning data, generally meant to be stored
	together in one file.
*/
class CTunablesGroup
{
public:
	explicit CTunablesGroup(const char* groupName);
	~CTunablesGroup();

	void Load(bool loadFromAssetsDir = false);
#if !__FINAL
	void Save();
	void SetNonFinal(const bool nonFinal)	{	m_NonFinal = nonFinal; }
#endif

	void SetUsePso(bool usePso)				{	m_UsePso = usePso;	}
	bool GetUsePso() const					{	return m_UsePso;	}

	void FinishInitialization();
	const char* GetName() const							{	return m_Name.c_str();	}

	void Add(CTuning& tunables)				{	m_Tunables.PushAndGrow(&tunables);	}
	void DeleteIfPresent(CTuning& tunables)	{	m_Tunables.DeleteMatches(&tunables);	}

#if __BANK
	void SetBankName(const char* name)			{	m_BankName = name;	}
	const char* GetBankName() const				{	return m_BankName.c_str();	}
	void AddWidgets();
	void RefreshBank();
#endif	// __BANK

	inline int GetNumTunables() const { return m_Tunables.GetCount(); }
	inline const CTuning* GetTuning(int idx) const { return ( (u32)idx < (u32)m_Tunables.GetCount() ) ? m_Tunables[idx] : NULL; }

protected:
	enum FileType
	{
		kAssetXml,
		kCommonCrcXml,
		kCommonXml,
		kPlatformCrcPso
	};
	void GenerateFilename(char* buffOut, int buffSz, FileType fileType) const;

#if __BANK
	void LoadAndUpdateWidgets(bool loadFromAssetsDir);
	static void LoadCB(CTunablesGroup* obj)				{	obj->LoadAndUpdateWidgets(false);	}
	static void LoadFromAssetsCB(CTunablesGroup* obj)	{	obj->LoadAndUpdateWidgets(true);	}
	static void SaveCB(CTunablesGroup* obj)	{	obj->Save();	}

	// PURPOSE:	If m_BankGroup is set, this is the bank the group was added to,
	//			which is needed in order to delete it when data is reloaded.
	bkBank*				m_Bank;

	// PURPOSE:	Pointer to widget group we've added, if any.
	bkGroup*			m_BankGroup;

	ConstString			m_BankDisplayFilename;
	ConstString			m_BankName;
#endif

#if !__FINAL
	bool				m_NonFinal;
#endif // !__FINAL

	// PURPOSE:	True if this group of tuning objects is meant to be loaded from PSO files.
	bool				m_UsePso;

	// PURPOSE:	The name of the group.
	ConstString			m_Name;

	// PURPOSE:	Array of pointers to the tunable objects in the group. These
	//			objects are not owned.
	atArray<CTuning*>	m_Tunables;
};

//-----------------------------------------------------------------------------
/// Various macros to simplify use

#define IMPLEMENT_TUNABLES(classname, name, hash, groupname, bankname) \
	classname::Tunables::Tunables() : CTuning(name, hash, groupname, bankname, false, false) {}

#define IMPLEMENT_TUNABLES_PSO(classname, name, hash, groupname, bankname) \
	classname::Tunables::Tunables() : CTuning(name, hash, groupname, bankname, false, true) {}

#if !__FINAL
#define IMPLEMENT_NON_FINAL_TUNABLES(classname, name, hash, groupname, bankname) \
	classname::Tunables::Tunables() : CTuning(name, hash, groupname, bankname, true, false) {}
#endif // !__FINAL

#define IMPLEMENT_EVENT_TUNABLES(classname, hash, groupname)			IMPLEMENT_TUNABLES(classname, #classname, hash, groupname, "Event Tuning")
#define IMPLEMENT_EVENT_TUNABLES_PSO(classname, hash, groupname)		IMPLEMENT_TUNABLES_PSO(classname, #classname, hash, groupname, "Event Tuning")
#define IMPLEMENT_DAMAGE_EVENT_TUNABLES(classname, hash)				IMPLEMENT_EVENT_TUNABLES(classname, hash, "Damage Events")
#define IMPLEMENT_GENERAL_EVENT_TUNABLES(classname, hash)				IMPLEMENT_EVENT_TUNABLES_PSO(classname, hash, "General Events")
#define IMPLEMENT_LEADER_EVENT_TUNABLES(classname, hash)				IMPLEMENT_EVENT_TUNABLES(classname, hash, "Leader Events")
#define IMPLEMENT_MOVEMENT_EVENT_TUNABLES(classname, hash)			IMPLEMENT_EVENT_TUNABLES_PSO(classname, hash, "Movement Events")
#define IMPLEMENT_RANDOM_EVENT_TUNABLES(classname, hash)				IMPLEMENT_EVENT_TUNABLES_PSO(classname, hash, "Random Events")
#define IMPLEMENT_REACTION_EVENT_TUNABLES(classname, hash)			IMPLEMENT_EVENT_TUNABLES_PSO(classname, hash, "Reaction Events")
#define IMPLEMENT_SOUND_EVENT_TUNABLES(classname, hash)				IMPLEMENT_EVENT_TUNABLES_PSO(classname, hash, "Sound Events")
#define IMPLEMENT_WEAPON_EVENT_TUNABLES(classname, hash)				IMPLEMENT_EVENT_TUNABLES_PSO(classname, hash, "Weapon Events")

#define IMPLEMENT_TASK_TUNABLES(classname, hash, groupname)			IMPLEMENT_TUNABLES(classname, #classname, hash, groupname, "Task Tuning")
#define IMPLEMENT_TASK_TUNABLES_PSO(classname, hash, groupname)		IMPLEMENT_TUNABLES_PSO(classname, #classname, hash, groupname, "Task Tuning")
#define IMPLEMENT_ANIMATION_TASK_TUNABLES(classname, hash)			IMPLEMENT_TASK_TUNABLES(classname, hash, "Animation Tasks")
#define IMPLEMENT_COMBAT_TASK_TUNABLES(classname, hash)				IMPLEMENT_TASK_TUNABLES_PSO(classname, hash, "Combat Tasks")
#define IMPLEMENT_CRIME_TASK_TUNABLES(classname, hash)				IMPLEMENT_TASK_TUNABLES_PSO(classname, hash, "Crime Tasks")
#define IMPLEMENT_DEFAULT_TASK_TUNABLES(classname, hash)				IMPLEMENT_TASK_TUNABLES_PSO(classname, hash, "Default Tasks")
#define IMPLEMENT_GENERAL_TASK_TUNABLES(classname, hash)				IMPLEMENT_TASK_TUNABLES_PSO(classname, hash, "General Tasks")
#define IMPLEMENT_MOTION_TASK_TUNABLES(classname, hash)				IMPLEMENT_TASK_TUNABLES_PSO(classname, hash, "Motion Tasks")
#define IMPLEMENT_MOVEMENT_TASK_TUNABLES(classname, hash)				IMPLEMENT_TASK_TUNABLES_PSO(classname, hash, "Movement Tasks")
#define IMPLEMENT_PHYSICS_TASK_TUNABLES(classname, hash)				IMPLEMENT_TASK_TUNABLES_PSO(classname, hash, "Physics Tasks")
#define IMPLEMENT_RESPONSE_TASK_TUNABLES(classname, hash)				IMPLEMENT_TASK_TUNABLES_PSO(classname, hash, "Response Tasks")
#define IMPLEMENT_SCENARIO_TASK_TUNABLES(classname, hash)				IMPLEMENT_TASK_TUNABLES_PSO(classname, hash, "Scenario Tasks")
#define IMPLEMENT_SERVICE_TASK_TUNABLES(classname, hash)				IMPLEMENT_TASK_TUNABLES_PSO(classname, hash, "Service Tasks")
#define IMPLEMENT_VEHICLE_TASK_TUNABLES(classname, hash)				IMPLEMENT_TASK_TUNABLES_PSO(classname, hash, "Vehicle Tasks")
#define IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(classname, hash)			IMPLEMENT_TASK_TUNABLES_PSO(classname, hash, "Vehicle AI Tasks")
#define IMPLEMENT_WEAPON_TASK_TUNABLES(classname, hash)				IMPLEMENT_TASK_TUNABLES_PSO(classname, hash, "Weapon Tasks")

#if !__FINAL
#define IMPLEMENT_NON_FINAL_TASK_TUNABLES(classname, hash, groupname)	IMPLEMENT_NON_FINAL_TUNABLES(classname, #classname, hash, groupname, "Task Tuning Non Final")
#define IMPLEMENT_NON_FINAL_COMBAT_TASK_TUNABLES(classname, hash)		IMPLEMENT_NON_FINAL_TASK_TUNABLES(classname, hash, "Combat Tasks Non Final")
#endif // !__FINAL

#define IMPLEMENT_PLAYER_TUNABLES_PSO(classname, hash, groupname)		IMPLEMENT_TUNABLES_PSO(classname, #classname, hash, groupname, "Player Tunables")
#define IMPLEMENT_PLAYER_TARGETTING_TUNABLES(classname, hash)			IMPLEMENT_PLAYER_TUNABLES_PSO(classname, hash, "Player Targetting")
#define IMPLEMENT_PED_TARGET_EVALUATOR_TUNABLES(classname, hash)		IMPLEMENT_PLAYER_TUNABLES_PSO(classname, hash, "Ped Target Evaluator")
#define IMPLEMENT_PLAYER_INFO_TUNABLES(classname, hash)				IMPLEMENT_PLAYER_TUNABLES_PSO(classname, hash, "Player Info")

#define IMPLEMENT_IK_TUNABLES_PSO(classname, hash, groupname)			IMPLEMENT_TUNABLES_PSO(classname, #classname, hash, groupname, "Ik Tuning")
#define IMPLEMENT_IK_SOLVER_TUNABLES(classname, hash)					IMPLEMENT_IK_TUNABLES_PSO(classname, hash ,"Ik Solvers")

#define IMPLEMENT_MINIMAP_TUNABLES(classname, hash)						IMPLEMENT_TUNABLES_PSO(classname, #classname, hash, "Minimap", "Minimap Tuning")

#define IMPLEMENT_VEHICLE_DAMAGE_TUNABLES(classname, hash)				IMPLEMENT_TUNABLES_PSO(classname, #classname, hash, "Vehicle Damage", "Vehicle Tuning")
//-----------------------------------------------------------------------------

#endif	// TASK_SYSTEM_TUNING_H

// End of file task/System/Tuning.h
