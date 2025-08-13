#ifndef _ANIM_SCENE_ENTITIES_H_
#define _ANIM_SCENE_ENTITIES_H_

// framework includes
#include "fwanimation/directorcomponentsyncedscene.h"

// game includes
#include "animation/AnimScene/AnimSceneEntity.h"
#include "animation/AnimScene/AnimSceneEntityTypes.h"
#include "animation/AnimScene/AnimSceneHelpers.h"
#include "animation/debug/AnimDebug.h"
#include "scene/RegdRefTypes.h"

class CAnimSceneEntityInitialiser;

// TODO - move this to its own file ? 
class CAnimSceneLeadInData
{
public:

	virtual ~CAnimSceneLeadInData() { }

	CAnimSceneClipSet m_clipSet;
	atHashString m_playbackList;

	PAR_PARSABLE;
};

class CAnimScenePed : public CAnimSceneEntity
{
public:
	CAnimScenePed()
		: CAnimSceneEntity()
		, m_pPed(NULL)
		, m_optional(false)
	{

	}

	CAnimScenePed(const CAnimScenePed& other)
		: CAnimSceneEntity(other)
		, m_debugPedName(other.m_debugPedName)
		, m_optional(other.m_optional)
	{
		SetPed(other.m_pPed);
#if __BANK
		// take ownership of the debug model
		if (other.m_widgets->m_usingDebugModel)
		{	
			m_widgets->m_usingDebugModel = true;
			other.m_widgets->m_usingDebugModel=false;
		}
#endif //__BANK
	}

	virtual CAnimScenePed* Copy()
	{
		return rage_new CAnimScenePed(*this);
	}

	virtual eAnimSceneEntityType GetType() const { return ANIM_SCENE_ENTITY_PED; }

	CPed* GetPed(BANK_ONLY(bool bCreateDebugPed = true)) { BANK_ONLY(if (bCreateDebugPed) CreateDebugPedIfRequired()); return m_pPed; }
	const CPed* GetPed() const { return m_pPed; }
	void SetPed (CPed* pPed);

	virtual bool SupportsLocationData() { return true; }
	virtual bool IsDisabled() const { return m_optional && m_pPed==NULL; }
	virtual bool IsOptional() const { return m_optional; }

	CAnimSceneLeadInData* FindLeadIn(u32 playbackListHash = 0, bool fallbackOnDefault = true) 
	{
		s32 defaultListIndex = -1;
		for (s32 i=0; i<m_leadIns.GetCount(); i++)
		{
			if (m_leadIns[i].m_playbackList.GetHash()==playbackListHash)
			{
				return &m_leadIns[i];
			}
			else if (m_leadIns[i].m_playbackList.GetHash()==0)
			{
				// remember the default list. We'll use this if the exact list isn't found
				defaultListIndex = i;
			}
		}

		// return the default list
		if (fallbackOnDefault && defaultListIndex>=0)
		{
			return &m_leadIns[defaultListIndex];
		}

		return NULL; 
	}
	
#if __BANK

	void RemoveLeadIn(u32 playbackListHash = 0) 
	{
		for (s32 i=0; i<m_leadIns.GetCount(); i++)
		{
			if (m_leadIns[i].m_playbackList.GetHash()==playbackListHash)
			{
				m_leadIns.Delete(i);
				return;
			}
		}
	}

	virtual void Shutdown(CAnimScene* pScene);
	void SetDebugPed (CPed* pPed);
	void SetLeadInClipSet(atHashString playbackList, fwMvClipSetId clipSet);
	virtual void InitForEditor(CAnimSceneEntityInitialiser*);
#endif //__BANK

	PAR_PARSABLE;
private:

	atHashString m_debugPedName;
	atArray<CAnimSceneLeadInData> m_leadIns;
	RegdPed m_pPed;

#if __BANK
	void OnPostAddWidgets(bkBank& pBank);
	void RegisterSelectedPed();
	void UnregisterPed();
	void OnModelSelected();
	void CreateDebugPedIfRequired();
#if __DEV
	virtual void DebugRender(CAnimScene* pScene, debugPlayback::OnScreenOutput& mainOutput);
#endif // __DEV
	struct Widgets{
		Widgets()
			: m_usingDebugModel(false)
		{

		}
		CDebugPedModelSelector m_modelSelector;
		mutable bool m_usingDebugModel;
	};
#endif // __BANK

	DECLARE_ANIM_SCENE_BANK_DATA(Widgets, m_widgets);

	bool m_optional;
};

class CAnimSceneVehicle : public CAnimSceneEntity
{
public:
	CAnimSceneVehicle()
		: CAnimSceneEntity()
		, m_pVeh(NULL)
		, m_optional(false)
	{

	}

	CAnimSceneVehicle(const CAnimSceneVehicle& other)
		: CAnimSceneEntity(other)
		, m_debugModelName(other.m_debugModelName)
		, m_optional(other.m_optional)
	{
		SetVehicle(other.m_pVeh);
#if __BANK
		// take ownership of the debug model
		if (other.m_widgets->m_usingDebugModel)
		{	
			m_widgets->m_usingDebugModel = true;
			other.m_widgets->m_usingDebugModel=false;
		}
#endif //__BANK
	}

	virtual CAnimSceneVehicle* Copy()
	{
		return rage_new CAnimSceneVehicle(*this);
	}

	eAnimSceneEntityType GetType() const { return ANIM_SCENE_ENTITY_VEHICLE; }

	CVehicle* GetVehicle(BANK_ONLY(bool bCreateDebugModel = true)) { BANK_ONLY(if (bCreateDebugModel) CreateDebugVehIfRequired()); return m_pVeh; }
	void SetVehicle( CVehicle* pVeh);
	const CVehicle* GetVehicle() const { return m_pVeh; }

	virtual bool SupportsLocationData() { return true; }
	virtual bool IsDisabled() const { return m_optional && m_pVeh==NULL; }
	virtual bool IsOptional() const { return m_optional; }

#if __BANK
	virtual void Shutdown(CAnimScene* pScene);
	void SetDebugVehicle(CVehicle* pVeh);
	virtual void InitForEditor(CAnimSceneEntityInitialiser*);
#endif //__BANK

	PAR_PARSABLE;
private:
	atHashString m_debugModelName;
	RegdVeh m_pVeh;
#if __BANK
	void OnPostAddWidgets(bkBank& pBank);
	void RegisterSelectedVehicle();
	void UnregisterVehicle();
	void OnModelSelected();
#if __DEV
	virtual void DebugRender(CAnimScene* pScene, debugPlayback::OnScreenOutput& mainOutput);
#endif //__DEV
	void CreateDebugVehIfRequired();
	struct Widgets{
		Widgets()
			: m_usingDebugModel(false)
		{

		}
		CDebugVehicleSelector m_modelSelector;
		mutable bool m_usingDebugModel;
	};
#endif // __BANK
	DECLARE_ANIM_SCENE_BANK_DATA(Widgets, m_widgets);

	bool m_optional;
};

class CAnimSceneObject : public CAnimSceneEntity
{
public:
	CAnimSceneObject()
		: CAnimSceneEntity()
		, m_optional(false)
	{

	}

	CAnimSceneObject(const CAnimSceneObject& other)
		: CAnimSceneEntity(other)
		, m_debugModelName(other.m_debugModelName)
		, m_optional(other.m_optional)
	{
		SetObject(other.m_data->m_pObj);
#if __BANK
		// take ownership of the debug model
		if (other.m_widgets->m_usingDebugModel)
		{	
			m_widgets->m_usingDebugModel = true;
			other.m_widgets->m_usingDebugModel=false;
		}
#endif //__BANK
	}

	virtual CAnimSceneObject* Copy()
	{
		return rage_new CAnimSceneObject(*this);
	}

	eAnimSceneEntityType GetType() const  { return ANIM_SCENE_ENTITY_OBJECT; }

	CObject* GetObject(BANK_ONLY(bool bCreateDebugModel = true)) { BANK_ONLY(if (bCreateDebugModel) CreateDebugObjIfRequired()); return m_data->m_pObj; }
	void SetObject( CObject* pObj, bool bSelect = true);
	const CObject* GetObject() const { return m_data->m_pObj; }
	virtual bool SupportsLocationData() { return true; }
	virtual bool IsDisabled() const { return m_optional && m_data->m_pObj==NULL; }
	virtual bool IsOptional() const { return m_optional; }
	virtual void OnStartOfScene();
	virtual void OnOtherEntityRemovedFromSceneEarly(CAnimSceneEntity::Id entityId);

	void SetParentEntityId(CAnimSceneEntity::Id entityId) { m_data->m_parentEntityId = entityId; }

#if __BANK
	virtual void Shutdown(CAnimScene* pScene);
	void SetDebugObject(CObject* pObj);
	virtual void InitForEditor(CAnimSceneEntityInitialiser*);
#endif //__BANK

	PAR_PARSABLE;
private:
	atHashString m_debugModelName;

	struct RuntimeData
	{
		RegdObj m_pObj;
		CAnimSceneEntity::Id m_parentEntityId;
	};

	DECLARE_ANIM_SCENE_RUNTIME_DATA(RuntimeData, m_data);
#if __BANK
	void OnPostAddWidgets(bkBank& pBank);
	void RegisterSelectedObject();
	void UnregisterObject();
	void OnModelSelected();
	void CreateDebugObjIfRequired();
#if __DEV
	virtual void DebugRender(CAnimScene* pScene, debugPlayback::OnScreenOutput& mainOutput);
#endif //__DEV
	struct Widgets{
		Widgets()
			: m_usingDebugModel(false)
		{

		}
		CDebugObjectSelector m_modelSelector;
		mutable bool m_usingDebugModel;
	};
#endif // __BANK
	DECLARE_ANIM_SCENE_BANK_DATA(Widgets, m_widgets);

	bool m_optional;
};

class CAnimSceneBoolean : public CAnimSceneEntity
{
public:
	CAnimSceneBoolean()
		: CAnimSceneEntity()
		, m_value(false)
		, m_resetsAtSceneStart(true)
		, m_originalValue(false)
		, m_originalValueCached(false)
	{

	}

	CAnimSceneBoolean(const CAnimSceneBoolean& other)
		: CAnimSceneEntity(other)
		, m_value(other.m_value)
		, m_resetsAtSceneStart(other.m_resetsAtSceneStart)
		, m_originalValue(other.m_originalValue)
		, m_originalValueCached(other.m_originalValueCached)
	{

	}

	eAnimSceneEntityType GetType() const { return ANIM_SCENE_BOOLEAN; }

	void SetValue(bool value) { m_value = value; }
	bool GetValue() const { return m_value; }

	virtual CAnimSceneBoolean* Copy()
	{
		return rage_new CAnimSceneBoolean(*this);
	}

	virtual void Init(CAnimScene* pScene)
	{
		CAnimSceneEntity::Init(pScene);

		if (!m_originalValueCached)
		{
			m_originalValue = m_value;
			m_originalValueCached = true;
		}
		else
		{
			if (m_resetsAtSceneStart)
			{
				m_value = m_originalValue;
			}
		}		
	}

#if __BANK
	virtual atString GetDebugString();
	virtual void OnDebugTextSelected() { SetValue(!GetValue()); }; // toggle the value when the user clicks on it
	virtual void InitForEditor(CAnimSceneEntityInitialiser*);
#endif // __BANK

	PAR_PARSABLE;
private:
	bool m_value;
	bool m_resetsAtSceneStart;

	bool m_originalValue;
	bool m_originalValueCached;
};

class CAnimSceneCamera : public CAnimSceneEntity
{
public:
	CAnimSceneCamera()
		: CAnimSceneEntity()
	{

	}

	CAnimSceneCamera(const CAnimSceneCamera& other)
		: CAnimSceneEntity(other)
	{

	}

	virtual CAnimSceneCamera* Copy()
	{
		return rage_new CAnimSceneCamera(*this);
	}

	virtual eAnimSceneEntityType GetType() const { return ANIM_SCENE_ENTITY_CAMERA; }

#if __BANK
	virtual void InitForEditor(CAnimSceneEntityInitialiser*);
#endif //__BANK

	PAR_PARSABLE;
private:
};


#endif //_ANIM_SCENE_ENTITIES_H_
