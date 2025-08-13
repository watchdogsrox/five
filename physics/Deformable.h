//
// File: Deformable.h
// Started: 14/4/11
// Creator: Richard Archibald
// Purpose: Handle loading metadata which define the props that have deformable bones and properties for each
//          deformable bone.
//
#ifndef DEFORMABLE_H_
#define DEFORMABLE_H_

// RAGE headers:
#include "atl/array.h"
#include "atl/binmap.h"
#include "atl/string.h"
#include "atl/hashstring.h"
#include "parser/macros.h"
#include "physics/inst.h"

// Framework headers:
#include "grcore/debugdraw.h"

// Game headers:
#include "scene/RegdRefTypes.h"

#define DEFORMABLE_OBJECTS 1

////////////////////////////////////////////////////////
// Definition of structure to hold individual bone data 
////////////////////////////////////////////////////////
class CDeformableBoneData
{
public:
	CDeformableBoneData()
		: m_nBoneId(0),
		m_strength(1.0)
	{
	}

	// Functions called by the XML parser:
	void OnPostLoad();

	// Accessor functions:
	u16 GetBoneId() const { return m_nBoneId; }
	float GetStrength() const { return m_strength; }
	float GetRadius() const { return m_radius; }
	float GetDamageVelThreshold() const { return m_damageVelThreshold; }

private:
	// Hash of the bone name.
	u16 m_nBoneId;
	// The metadata will set this variable and we 
	atString m_boneName;
	// Physical properties used in procedural calculation of post-impact bone position.
	float m_strength;
	float m_radius;
	float m_damageVelThreshold;

	PAR_SIMPLE_PARSABLE;
};

//////////////////////////////////////////////////////////
// Stores the deformation information for a single object
//////////////////////////////////////////////////////////
class CDeformableObjectEntry
{
public:
	CDeformableObjectEntry()
	{
	}

	// Functions called by the XML parser:
	void OnPostLoad();

	// Accessor functions:
	u32 GetObjectNameHash() const { return m_objectName.GetHash(); }
	u32 GetNumDeformableBones() const { return m_DeformableBones.GetCount(); }
	u16 GetBoneId(int nIndex) const { return m_DeformableBones[nIndex].GetBoneId(); }
	float GetObjectStrength() const { return m_objectStrength; }
	float GetBoneStrength(int nIndex) const { return m_DeformableBones[nIndex].GetStrength(); }
	float GetBoneRadius(int nIndex) const { return m_DeformableBones[nIndex].GetRadius(); }
	float GetBoneDamageVelThreshold(int nIndex) const { return m_DeformableBones[nIndex].GetDamageVelThreshold(); }

private:
	// Name of this object.
	atHashString m_objectName;
	// Strength parameter for this object.
	float m_objectStrength;
	// List of the deformation properties of each deformable bone in this object.
	atArray<CDeformableBoneData> m_DeformableBones;

	PAR_SIMPLE_PARSABLE;
};

///////////////////////////////////
// Definition of the manager class
///////////////////////////////////
class CDeformableObjectManager
{
public:
	static void Load(const char* fileName);

	// Functions called by the XML parser:
	void OnPostLoad();

	// Functions to get information about the deformable objects.
	static CDeformableObjectManager& GetInstance() { return m_instance; }
	bool IsModelDeformable(atHashWithStringNotFinal hName, CDeformableObjectEntry** ppData=NULL);

	// Compute the new positions of the deformable bones for a given instance.
	void ProcessDeformableBones(phInst* pThisInst, const Vec3V& vImpulse, const Vec3V& vImpactPos, CEntity* pOtherEntity, const int &iThisComponent, const int &iOtherComponent);

	// Functions to assist in debugging and tuning the deformable objects.
#if __BANK
	static void Reload();
	void AddWidgetsToBank(bkBank& bank);
	void RenderDebug();
	void StoreFocusEntAddress();
	void PerBoneDebugDraw();
#endif // __BANK


private:
	// List of all objects which are deformable.
	atBinaryMap<CDeformableObjectEntry, atHashWithStringNotFinal> m_DeformableObjects;

	// The singleton instance.
	static CDeformableObjectManager m_instance;

	// Some constants which define the damage scaling functions.
	static float m_fDamageScaleCurveConst;
	static float m_fMaxDamage;
	static float m_fDamageSaturationLimit;

	static float m_fBoneDistanceScalingCurveConst;

	// Debug related variables.
#if __BANK
	static bool m_bEnableDebugDraw;
	static bool m_bVisualiseLimitsTrans;
	static bool m_bVisualiseDeformableBones;
	static bool m_bDisplayBoneText;
	static bool m_bVisualiseBoneRadius;
	static bool m_bDisplayImpactPos;
	static bool m_bDisplayImpactInfo;
	RegdEnt m_pFocusEnt;
	atArray<Vec3V> m_aImpactPositions;
	atArray<Vec3V> m_aImpactDirVecs;
	atArray<float> m_aImpactImpulse;
	atArray<float> m_aImpactRelVel;

public:
	void ResetImpactArrays()
	{
		m_aImpactPositions.Reset();
		m_aImpactDirVecs.Reset();
		m_aImpactImpulse.Reset();
		m_aImpactRelVel.Reset();
	}
#endif // __BANK

	PAR_SIMPLE_PARSABLE;
};

#endif // DEFORMABLE_H_
