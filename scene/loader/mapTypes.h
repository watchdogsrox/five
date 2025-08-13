//
// scene/loader/maptypes.h
//
// Copyright (C) 1999-2011 Rockstar Games. All Rights Reserved.
//

#ifndef SCENE_LOADER_MAPTYPES_H
#define SCENE_LOADER_MAPTYPES_H

#include "parser/macros.h"
#include "data/base.h"

#include "atl/array.h"
#include "string/stringhash.h"

#include "scene/loader/mapdata.h"
#include "fwscene/mapdata/maptypes.h"
#include "fwscene/mapdata/maptypescontents.h"
#include "fwscene/mapdata/mapdata.h"


#define MAPTYPE_NAMELEN 64

// --- Forward Declarations -------------------------------------------------
// --- Enumerations ---------------------------------------------------------


enum eCompEntityEffectType
{
	CE_EFFECT_PTFX				= 0,
//	CE_EFFECT_AUDIO
};

// --- Classes --------------------------------------------------------------

class CCompEntityEffectsData 
{
public:
	eCompEntityEffectType GetFxType() const { return(m_fxType); }
	Vec3V_Out GetFxOffsetPos() const {return m_fxOffsetPos;}
	Vec4V_Out GetFxOffsetRot() const {return m_fxOffsetRot;}

	u32 GetBoneTag() const { return(m_boneTag); }
	float GetStartPhase() const { return(m_startPhase); }
	float GetEndPhase() const { return(m_endPhase); }

	// particle effect specific
	bool GetPtFxIsTriggered() const {return m_ptFxIsTriggered;}
	const char* GetPtFxTag() const {return m_ptFxTag;}
	u32 GetPtFxTagHash() const {return atStringHash(m_ptFxTag);}
	float GetPtFxScale() const {return m_ptFxScale;}
	float GetPtFxProbability() const {return m_ptFxProbability;}
	bool GetPtFxHasTint() const {return m_ptFxHasTint;}
	u8 GetPtFxTintR() const {return m_ptFxTintR;}
	u8 GetPtFxTintG() const {return m_ptFxTintG;}
	u8 GetPtFxTintB() const {return m_ptFxTintB;}
	Vec3V_Out GetPtFxSize() const {return m_ptFxSize;}

private:
	eCompEntityEffectType m_fxType;
	Vec3V m_fxOffsetPos;
	Vec4V m_fxOffsetRot;

	u32 m_boneTag;
	float m_startPhase;
	float m_endPhase;

	// particle effect specific
	bool m_ptFxIsTriggered;
	char m_ptFxTag[MAPTYPE_NAMELEN];
	float m_ptFxScale;
	float m_ptFxProbability;
	bool m_ptFxHasTint;
	u8 m_ptFxTintR;
	u8 m_ptFxTintG;
	u8 m_ptFxTintB;
	Vec3V m_ptFxSize;

	PAR_SIMPLE_PARSABLE;
};

class CCompEntityAnims
{
public:
	CCompEntityAnims(void) { m_punchInPhase = 0.0f; m_punchOutPhase = 1.0f;}

	const char* GetAnimDict() const { return m_AnimDict; }
	const char* GetAnim() const		{ return m_AnimName; }
	const char* GetAnimatedModel()	const	{ return m_AnimatedModel; }
	atArray<CCompEntityEffectsData>& GetEffectsData() { return (m_effectsData); }

	char		m_AnimDict[MAPTYPE_NAMELEN];
	char		m_AnimName[MAPTYPE_NAMELEN];
	char		m_AnimatedModel[MAPTYPE_NAMELEN];

	float		m_punchInPhase;		// position in phase to start drawing this animating object
	float		m_punchOutPhase;	// position in phase to stop drawing this animating object

	atArray<CCompEntityEffectsData>		m_effectsData;

	PAR_SIMPLE_PARSABLE;
};

class CCompositeEntityType
{
public:
	CCompositeEntityType();
	void Init(const char* name);

	void SetName(const char* name);
	u32 GetNameHash() const {return m_NameHash;}

	void OnPostLoad();
	

	const char* GetStartModel() const		{ return m_StartModel; }
	const char* GetEndModel()	const	{ return m_EndModel; }
	const atHashString GetStartImapFile() const		{ return m_StartImapFile; }
	const atHashString GetEndImapFile()	const	{ return m_EndImapFile; }

	const atHashString GetPtFxAssetName() const {return m_PtFxAssetName;}

	const char* GetName() const { return m_Name; }
	const float	GetLodDist() const { return m_lodDist; }
	const u32	GetFlags() const { return m_flags; }
	const u32	GetSpecialAttribute() const { return m_specialAttribute; }
	const Vector3 GetBBMin() const { return m_bbMin; }
	const Vector3 GetBBMax() const { return m_bbMax; }
	const Vector3 GetBSCentre() const { return m_bsCentre; }
	const float	GetBSRadius() const { return m_bsRadius; }
	atArray<CCompEntityAnims>& GetAnimData() { return (m_Animations); }


private:
	char		m_Name[MAPTYPE_NAMELEN];
	float		m_lodDist;
	u32			m_flags;
	u32			m_specialAttribute;
	Vector3		m_bbMin;
	Vector3		m_bbMax;
	Vector3		m_bsCentre;
	float		m_bsRadius;
	u32			m_NameHash;
	char		m_StartModel[MAPTYPE_NAMELEN];
	char		m_EndModel[MAPTYPE_NAMELEN];

	atHashString		m_StartImapFile;
	atHashString		m_EndImapFile;

	atHashString m_PtFxAssetName;

	atArray<CCompEntityAnims>			m_Animations;

	// ---


	PAR_SIMPLE_PARSABLE;
};

class CMapTypesContents : public fwMapTypesContents
{
public:
	~CMapTypesContents() {}
};

class CMapTypes : public fwMapTypes
{
public:
	virtual fwMapTypesContents* CreateMapTypesContents() { return rage_new CMapTypesContents(); }

	void Init();
	void Load(const char* filename);

	virtual void PreConstruct(s32 mapTypeDefIndex,  fwMapTypesContents* contents);
	virtual void PostConstruct(s32 mapTypeDefIndex,  fwMapTypesContents* contents);

	void ConstructCompositeEntities(fwMapTypesContents* contents);

	atArray <CCompositeEntityType> m_compositeEntityTypes;

private:
	virtual void AllocArchetypeStorage(s32 mapTypeDefIndex);

	virtual void ConstructLocalExtensions(s32, fwArchetype* , fwArchetypeDef* );

	PAR_PARSABLE;
};
 
#endif // SCENE_LOADER_MAPTYPES_H
