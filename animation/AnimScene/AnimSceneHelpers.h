#ifndef _ANIM_SCENE_HELPERS_H_
#define _ANIM_SCENE_HELPERS_H_

// rage includes
#include "atl/functor.h"
#include "atl/hashstring.h"
#include "data/base.h"

#include "animation/anim_channel.h" 
#include "animation/AnimDefines.h"


// game includes
#include "animation/AnimScene/AnimSceneEntity.h"
#include "animation/debug/AnimDebug.h"
#include "scene/RegdRefTypes.h"
#include "debug/DebugScene.h"
//
// animation/AnimScene/AnimSceneHelpers.h
//

class CAnimScene;
#if __BANK
class CAuthoringHelper; 
#endif

//////////////////////////////////////////////////////////////////////////
// CAnimSceneRuntimeData - Helper for storing runtime only data in anim scenes
// Since anim scenes are loaded as PSO structures, any runtime only data 
// (i.e. not required in the data file) needs to be stored in a seperate structure
// and rage_new'd at runtime. this reduces the data cost of the structure
// to a single pointer. This helper class handles construction and 
// destruction of the structure, and overloads the pointer operator
// for easy access. By default the structure is lazily constructed
// on first use (although it can be externally Set e.g. in cases where
// a non-default constructor is required).

template <class StructureType>
class CAnimSceneRuntimeData
{
public:
	CAnimSceneRuntimeData()
		: m_pData(NULL)
	{

	}

	~CAnimSceneRuntimeData()
	{
		Shutdown();
	}

	// Default initialisation
	// create a debug structure to use
	void Init()
	{
		if (!m_pData)
		{
			m_pData = rage_new StructureType();
			animAssert(m_pData);
		}
	}

	// init with a premade structure
	void Init(StructureType* pData)
	{
		Set(pData);
	}

	void Shutdown()
	{
		if (m_pData)
		{
			delete m_pData;
			m_pData = NULL;
		}
	}

	// PURPOSE: Provide the structure data to use.
	void Set(StructureType* pData)
	{
		if (m_pData && m_pData != pData)
		{
			delete m_pData;
		}
		m_pData = pData;
	}

	// PURPOSE: Get the structure.
	StructureType* Get()
	{ 
		if (!m_pData)
		{
			m_pData = rage_new StructureType();
			animAssert(m_pData);
		}
		animAssertf(m_pData, "Anim scene structure data failed to initialise!");
		return m_pData; 
	}

	const StructureType* Get() const
	{ 
		if (!m_pData)
		{
			m_pData = rage_new StructureType();
			animAssert(m_pData);
		}
		animAssertf(m_pData, "Anim scene structure data failed to initialise!");
		return m_pData; 
	}

	// overridden pointer deref operator for easy access
	StructureType *operator->() 
	{ 
		return Get();
	}

	const StructureType *operator->() const
	{ 
		return Get();
	}

private:
	mutable StructureType *m_pData;
};

#define DECLARE_ANIM_SCENE_RUNTIME_DATA(structure, var) CAnimSceneRuntimeData<structure> var

#if __BANK
#define DECLARE_ANIM_SCENE_BANK_DATA(structure, var) CAnimSceneRuntimeData<structure> var
#else
#define DECLARE_ANIM_SCENE_BANK_DATA(structure, var) void *var
#endif //__BANK

// CAnimSceneHelperBase
// Base class for all helpers.  Allows us to register helpers etc.
//
class CAnimSceneHelperBase : public datBase
{
public:

	CAnimSceneHelperBase()
		: m_pOwningScene(NULL)
	{

	}

	~CAnimSceneHelperBase() {}

	virtual void OnInit(CAnimScene* pScene)
	{
		m_pOwningScene = pScene;
	}

	PAR_PARSABLE;

protected:

	CAnimScene*	m_pOwningScene;
};

// CAnimSceneMatrix
// A useful helper to represent a matrix in an anim scene object (presents a nicer pargen interface than the standard matrix)
//
class CAnimSceneMatrix : public CAnimSceneHelperBase
{
public:

	CAnimSceneMatrix()
		: m_matrix(V_IDENTITY)
	{

	}

	virtual ~CAnimSceneMatrix()
	{
#if __BANK
		for(int i= m_AnimSceneMatrix.GetCount() -1; i >=0; i--)
		{
			if(m_AnimSceneMatrix[i] == this)
			{
				m_bankData->m_parentSceneMatrix = NULL; 
				m_AnimSceneMatrix[i] = NULL; 
				m_AnimSceneMatrix.Delete(i); 
			}
		}
#endif
	}

#if __BANK
	void OnPreAddWidgets(bkBank& pBank);
	void OnPostAddWidgets(bkBank& pBank);
	void OnWidgetChanged();
	CAuthoringHelper& GetAuthoringHelper() { return m_bankData->m_AuthoringHelper; }
	void Update(); 
	void SetParentMatrix(CAnimSceneMatrix* parentMat) { m_bankData->m_parentSceneMatrix = parentMat; }
#endif //__BANK

	void SetMatrix(Mat34V_ConstRef mat) 
	{ 
		m_matrix = mat; 
#if __BANK
		m_bankData->m_position = m_matrix.d();
		m_bankData->m_orientation = Mat34VToEulersXYZ(m_matrix);
		m_bankData->m_orientation = Scale(m_bankData->m_orientation, ScalarV(V_TO_DEGREES));
#endif // __BANK
	}
	Mat34V_ConstRef GetMatrix() const { return m_matrix; }
	PAR_PARSABLE;

public:
#if __BANK
	static atArray<CAnimSceneMatrix*> m_AnimSceneMatrix; 
#endif
private:
	Mat34V m_matrix;

#if __BANK
	struct BankData{

		BankData()
			: m_orientation(V_ZERO)
			, m_position(V_ZERO)
			, m_AuthoringHelper(NULL,false)
			, m_parentSceneMatrix(NULL)
			, m_bWidgetChanged(false)
		{

		}
		Vec3V m_orientation; // can't be a Vec3V because the angle widget doesn't support it
		Vec3V m_position;
		CAuthoringHelper m_AuthoringHelper; 
		CAnimSceneMatrix* m_parentSceneMatrix; 
		bool m_bWidgetChanged;
	};
#endif // __BANK

	DECLARE_ANIM_SCENE_BANK_DATA(BankData, m_bankData);
};

class CAnimSceneClip : public CAnimSceneHelperBase
{
public:

	CAnimSceneClip()
	{

	}

	virtual ~CAnimSceneClip()
	{

	}

#if __BANK
	typedef Functor2<atHashString, atHashString> ClipChangedCB; // dictionary, clip

	void OnPreAddWidgets(bkBank& pBank);
	void AddClipWidgets();
	void OnShutdownWidgets();
	void OnDictionaryChanged();
	void OnClipChanged();
	void Edit();
	void SaveChanges();
	void SetClipSelectedCallback(ClipChangedCB cb) { m_widgets->m_clipSelectedCallback = cb; }
#endif //__BANK

	atHashString GetClipDictionaryName() const { return m_clipDictionaryName; }
	void SetClipDictionaryName(atHashString name) { m_clipDictionaryName=name; }
	atHashString GetClipName() const { return m_clipName; }
	void SetClipName(atHashString name) { m_clipName=name; }

	PAR_PARSABLE;

private:
	atHashString m_clipDictionaryName;
	atHashString m_clipName;
#if __BANK
	struct Widgets{
		Widgets()
			: m_pClipSelector(NULL)
			, m_pGroup(NULL)
		{

		}
		bkGroup* m_pGroup;
		CDebugClipSelector* m_pClipSelector;
		ClipChangedCB m_clipSelectedCallback;
	};
#endif //__BANK

	DECLARE_ANIM_SCENE_BANK_DATA(Widgets, m_widgets);
};

class CAnimSceneClipSet : public CAnimSceneHelperBase
{
public:

	CAnimSceneClipSet()
	{

	}

	virtual ~CAnimSceneClipSet()
	{

	}

	fwMvClipSetId GetId() const { return m_clipSetName; }
	atHashString GetName() const { return m_clipSetName; }

	void Set(fwMvClipSetId set) { m_clipSetName = set; }

	PAR_PARSABLE;

private:
	fwMvClipSetId m_clipSetName;
};

class CAnimSceneEntityHandle : public CAnimSceneHelperBase
{
public:

	CAnimSceneEntityHandle()
		: m_pEntity(NULL)
	{

	}

	virtual ~CAnimSceneEntityHandle()
	{

	}

	virtual void OnInit(CAnimScene* pScene);
	void OnPreSave();
	
	CAnimSceneEntity::Id GetId() const;

	// Accessors
	CAnimSceneEntity* GetEntity();
	const CAnimSceneEntity* GetEntity() const;
	CPhysical* GetPhysicalEntity();

	template <class Return_T>
	Return_T* GetEntity(eAnimSceneEntityType type)
	{
#if __BANK
		UpdateEntityId();
		Assertf(m_pOwningScene != NULL, "m_pOwningScene should not be null, was OnInit() called on this handle?");
#endif // !__BANK

		if (m_pEntity)
		{
			if (m_pEntity->GetType() == type)
			{
				animAssertf(static_cast<Return_T*>(m_pEntity)->GetType()==type, "Casting anim scene entity %s to the wrong type! Check your GetEntity<> call...", m_entityId.GetCStr());
				return static_cast<Return_T*>(m_pEntity);
			}
		}

		return NULL;
	}

#if __BANK
	void SetEntity(CAnimSceneEntity* pEnt);
	void UpdateEntityId() const;

	void OnPreAddWidgets(bkBank& pBank);
	void OnEntityChanged();
#endif //__BANK

	PAR_PARSABLE;

private:
	mutable CAnimSceneEntity::Id m_entityId;

	// This pointer is set during OnInit using the m_entityId that is serialised.
	// If the entity is renamed, then we do a lazy update when necessary to make sure the m_entityId remains valid - particularly on saving out.
	CAnimSceneEntity* m_pEntity;
	
#if __BANK
	struct Widgets{
		mutable CDebugAnimSceneEntitySelector m_selector;
	};
#endif //__BANK
	DECLARE_ANIM_SCENE_BANK_DATA(Widgets, m_widgets);
};

#endif //_ANIM_SCENE_HELPERS_H_
