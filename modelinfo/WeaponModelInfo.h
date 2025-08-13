//
// name:		WeaponModelInfo.cpp
// description:	File containing class describing weapon model info objects
// Written by:	Adam Fowler
#ifndef INC_WEAPONMODELINFO_H_
#define INC_WEAPONMODELINFO_H_

// game headers
#include "modelinfo/BaseModelInfo.h"
#include "renderer/HierarchyIds.h"

#define PROJECTILE_GRAVITY_FACTOR (1.0f)
const float PROJECTILE_DRAG_FACTOR = 0.00f;

namespace rage
{
class rmcDrawable;
}
class CCustomShaderEffectWeaponType;
class CWeaponModelInfo;
class CWeaponComponent;

struct WeaponHDStreamReq
{
	strRequest			m_Request;
	CWeaponModelInfo*	m_pTargetWeaponMI;
};

class CWeaponModelInfo : public CBaseModelInfo
{
public:
	// Intermediate struct used when constructing this class from xml (ie peds.xml)
	struct InitData {
		InitData	();
		void		Init();
		atString m_modelName;
		atString m_txdName;
		ConstString m_ptfxAssetName;
		float		m_lodDist;
		float       m_HDDistance;

		atHashString m_ExpressionSetName;
		atHashString m_ExpressionDictionaryName;
		atHashString m_ExpressionName;

		PAR_SIMPLE_PARSABLE;
	};
	struct InitDataList {
		atArray<InitData> m_InitDatas;
		PAR_SIMPLE_PARSABLE;
	};

	CWeaponModelInfo() {}

	virtual void Init();
	virtual void Shutdown();
	virtual void ShutdownExtra() { }

	virtual void InitMasterDrawableData(u32 modelIdx);
	virtual void DeleteMasterDrawableData();

	virtual void SetPhysics(phArchetypeDamp* pPhysics);

	virtual void DestroyCustomShaderEffects();

	virtual void InitWaterSamples();

	void Init(const InitData & rInitData);

	void SetBoneIndicies();
	s32 GetBoneIndex(eHierarchyId component) const {Assertf(component >= 0 && component < MAX_WEAPON_NODES, "Index [%d] out of range", component); return m_nBoneIndices[component];}
	eHierarchyId GetBoneHierarchyId(u32 boneHash) const;

	// PURPOSE: Add a reference to this archetype and add ref to any HD assets loaded
	virtual void AddHDRef(bool bRenderRef);
	virtual void RemoveHDRef(bool bRenderRef);
	u32 GetNumHdRefs() { return(m_numHDUpdateRefs + m_numHDRenderRefs); }


	// HD stuff
	// drawable + txd parent
	void SetupHDFiles(const char* pName);
 	void ConfirmHDFiles();
 
	void AddToHDInstanceList(size_t id);
	void RemoveFromHDInstanceList(size_t id);
	void FlushHDInstanceList() { m_HDInstanceList.Flush(); }
 
 	inline	s32 GetHDDrawableIndex() const { return m_HDDrwbIdx; }
 	inline	s32 GetHDTxdIndex() const { return m_HDtxdIdx; }
	bool	SetupHDCustomShaderEffects();


	void	SetHDDist(float HDDist) { m_HDDistance = HDDist; }
	float	GetHDDist(void) const { return m_HDDistance; } 
	bool	GetHasHDFiles(void) const { return m_HDDistance > 0.0f; }
	bool	GetRequestLoadHDFiles(void) const { return m_bRequestLoadHDFiles; }
	void 	SetRequestLoadHDFiles(bool bSet) { m_bRequestLoadHDFiles = bSet; }
	bool	GetAreHDFilesLoaded(void) const { return m_bAreHDFilesLoaded; }
	void	SetAreHDFilesLoaded(bool bSet) { m_bAreHDFilesLoaded = bSet; }
	bool	GetDampingTunedViaTunableObjects(void) const { return m_bDampingTunedViaTunableObjects; }

	CCustomShaderEffectWeaponType * GetHDMasterCustomShaderEffectType() { return m_pHDShaderEffectType; }
	rmcDrawable *GetHDDrawable();

 	static void UpdateHDRequests();

	virtual void SetExpressionSet(fwMvExpressionSetId expressionSet) {m_expressionSet = expressionSet;}
	virtual fwMvExpressionSetId GetExpressionSet() const {return m_expressionSet;}

private:

	void RequestAndRefHDFiles();
	void ReleaseRequestOrRemoveRefHD();
	void ReleaseHDFiles();

	s32 m_nBoneIndices[MAX_WEAPON_NODES];

	s32  m_HDDrwbIdx;
	s32  m_HDtxdIdx;
	float m_HDDistance;
	CCustomShaderEffectWeaponType *m_pHDShaderEffectType;

	s16			m_numHDUpdateRefs;
	s16			m_numHDRenderRefs;

	fwPtrListSingleLink	m_HDInstanceList;

	fwMvExpressionSetId m_expressionSet;

	u8		m_bRequestLoadHDFiles	        :1;
	u8		m_bAreHDFilesLoaded		        :1;
	u8		m_bRequestHDFilesRemoval        :1;
	u8		m_bDampingTunedViaTunableObjects:1;
	ATTR_UNUSED u8		m_unused04				:4;

	ATTR_UNUSED u8		m_padding[3];


	static fwPtrListSingleLink ms_HDStreamRequests;
};

#endif
