//
// Filename:	PedDamage.h
// Description:	Handles new Ped Damage, including blood, scars,  bruises and Tattoos.
// Written by:	SteveR
//
//	2010/10/25 - SteveR - initial;
//
//
 
#ifndef __PEDDAMAGE_H__
#define __PEDDAMAGE_H__

// rage includes
#include "atl/vector.h"
#include "data/base.h"
#include "fwanimation/boneids.h"
#include "grcore/device.h"
#include "grcore/effect.h"
#include "grmodel/shadergroupvar.h"
#include "math/float16.h" 
#include "paging/ref.h"
#include "parser/macros.h"
#include "rline/clan/rlclancommon.h"
#include "streaming/streamingmodule.h"
#include "vector/vector3.h"
#include "vector/vector4.h"
#include "squish/squishspu.h"

// game includes
#include "audio/gameobjects.h"
#include "control/replay/replay.h"
#include "control/replay/Effects/ProjectedTexturePacket.h"
#include "network/Crews/NetworkCrewEmblemMgr.h"
#include "scene/RegdRefTypes.h"

#define COMPRESS_DECORATIONS_ON_GPU_THREAD	(RSG_PC || RSG_DURANGO)
#define PED_DECORATIONS_MULTI_GPU_SUPPORT	(RSG_PC && 1)

namespace rage
{
	class grcViewport;
	class rmcDrawable;
	class bkBank;
	class fragInst;
	class phIntersection;
	class crSkeleton;
	class crSkeletonData;
	class grcTexture;
	class grcRenderTarget;
	class Color32;
	class grmShader;
};	


class CPed;

enum ePedDamageTypes
{ 
	// These are the real "types" of damage
	kDamageTypeBlood=0,				// blood splat/soak style
	kDamageTypeStab,				// blood stretched in a direction	
	kDamageTypeSkinDecoration,		// color only skin decoration (tattoo, bruise) NOTE: new non decoration types should go before this entry
	kDamageTypeSkinBumpDecoration,  // bumped mapped skin decoration (scar, or ?)  
	kDamageTypeClothDecoration,		// dirt or armor damage 
	kDamageTypeClothBumpDecoration, // dirt or armor with bump
	
	kNumDamageTypes,
	kDamageTypeInvalid = kNumDamageTypes
};

enum ePedDamageTextures
{
	kBloodTexSplat = 0,
	kBloodTexStab,
	kNumBloodTextureTypes
};

enum ePedDamageZones
{
	kDamageZoneTorso=0,	
	kDamageZoneHead,
	kDamageZoneLeftArm,
	kDamageZoneRightArm,
	kDamageZoneLeftLeg,
	kDamageZoneRightLeg,
	kDamageZoneMedals,	
	kDamageZoneNumZones,	
	kDamageZoneInvalid=kDamageZoneNumZones,
	kDamageZoneNumBloodZones=kDamageZoneRightLeg+1
};

enum {kHiResDamageTarget = 0, kMedResDamageTarget = 1, kLowResDamageTarget = 2};

enum { kInvalidPedDamageSet = 0xff };

enum { kLowResTargetsPerMedResTarget = 4, kMedResTargetsPerHighResTarget = 4};		// 4 low res fit in 1 medium res on 360, 4 med to 1 high (keep this at 4, so we skip over things properly


// currently the low res target over lap the medium res, in the same pool. if we get a different pool for the low res render targets, there can be more of them
// NOTE: kMaxHiResBloodRenderTargets can only be 2 for now, since it will not fit in the pools otherwise...
enum { kMaxHiResBloodRenderTargets = 2, kMaxMedResBloodRenderTargets = 7+(kMaxHiResBloodRenderTargets*kMedResTargetsPerHighResTarget), kMaxLowResBloodRenderTargets=(kMaxMedResBloodRenderTargets PS3_ONLY(+1))*kLowResTargetsPerMedResTarget}; // PS3 gets a couple extra lowres target (because they fit in the pool, plus it does not get as many per medium res)

enum { kMaxDamagedPeds = 66, kMaxCompressedDamagedPeds = 64, kMaxCompressedTextures=32 };  // 66 for max Damage peds, so we're a little bigger than 2x multiplayer count (they are cloned during races), there are 32 max Compressed damage peds, but only 32 compressed textures, since the clone can share the compressed texture

enum DamageBlitRotType {kNoRotation, kRandomRotation, kGravityRotation, kAutoDetectRotation};

enum DamageDecalCoverageType {kSkinOnly=0,kClothOnly=1}; // there is no "both" yet

enum eCompressedPedDamageSetState {
	kInvalid = 0,
	kCompressed,
	kWaitingOnTextures,
	kTexturesLoaded,
	kWaitingOnBlit,
	kBlitting,
	kWaitingOnCompression,
};

enum ePushType {
	kPushRadialU, kPushSine, kPushRadialUDress
};


class CPedDamagePushArea
{
public:
	bool CalcPushAmount(const Vector2 &inCoord, float radius, Vector2 & outPush, bool hasDress) const;
	void DebugDraw(bool hasDress) const;
public:
	ePushType m_Type;	
	Vector2 m_Center;	
	Vector2 m_RadiusOrAmpDir;   
	PAR_SIMPLE_PARSABLE;
};


class CPedDamageCylinderInfo
{
public:
	Vector3 baseOffset;
	Vector3 topOffset;
	float rotation;
	float push; 
	float lengthScale;
	float radius;
	eAnimBoneTag bone1;
	eAnimBoneTag bone2;
	eAnimBoneTag topBone;

	atArray<CPedDamagePushArea> pushAreas;

	PAR_SIMPLE_PARSABLE;
};


class CPedDamageCylinderInfoSet
{
public:
	CPedDamageCylinderInfoSet() {}
public:
	PedTypes type;
	CPedDamageCylinderInfo torsoInfo;
	CPedDamageCylinderInfo headInfo;
	CPedDamageCylinderInfo leftArmInfo;
	CPedDamageCylinderInfo rightArmInfo;
	CPedDamageCylinderInfo leftLegInfo;
	CPedDamageCylinderInfo rightLegInfo;
	// so we can index by zone
	CPedDamageCylinderInfo &operator[](int index) {return *(static_cast<CPedDamageCylinderInfo*>(static_cast<void*>(&(this->torsoInfo))) + index); }
	PAR_SIMPLE_PARSABLE;
};


class CPedDamageTexture
{
public:
	CPedDamageTexture();
	int CalcFixedFrameOrSequence(int requested = -1) const;  // returns a random frame between m_FrameMin and m_FrameMax, for non animated textures (-1 for animated)
	
	atHashString		m_TextureName;
	atFinalHashString	m_TxdName;
	float				m_TexGridCols;			// number of frames across in the texture grid  (These were ints, but we waste a lot of time in the rendering converting to floats)
	float				m_TexGridRows;			// number of frames across in the texture grid
	float				m_FrameMin;				// min frame to use for animation or random selection
	float				m_FrameMax;				// max frame to use for animation or random selection
	float				m_AnimationFrameCount;	// The number of frames in the animation (if less than 1+m_FrameMax-m_FrameMin, there are multiple animation sequences)
	float				m_AnimationFPS;			// if 0.0, we're not animated, play the frames an animation back at this rate otherwise pick as random variations
	bool				m_AllowUFlip;
	bool				m_AllowVFlip;
	Vector2				m_CenterOffset;			// offset for the center of rotation of the texture

	// not saved in xml
	grcTextureHandle	m_pTexture;
	s32					m_txdId;
	u16					m_uniqueId;

	PAR_SIMPLE_PARSABLE;

};


class CPedBloodDamageInfo
{
public:
	CPedBloodDamageInfo();
#if __BANK
	void			PreAddWidgets(bkBank& bank);
	void			PostAddWidgets(bkBank& bank);
#endif

	atHashString	m_Name;		// the name of the effect. (DamageTypeBulletSmall, DamageTypeBulletLarge, etc)

	CPedDamageTexture m_SoakTexture;
	CPedDamageTexture m_SoakTextureGravity;
	CPedDamageTexture m_WoundTexture;
	CPedDamageTexture m_SplatterTexture;

	DamageBlitRotType m_RotationType; // how should we rotate the blood blits?
	bool			  m_SyncSoakWithWound;

	float			m_WoundMinSize;
	float			m_WoundMaxSize;
	float			m_WoundIntensityWet;
	float			m_WoundIntensityDry;
	float			m_WoundDryingTime;
	
	float			m_SplatterMinSize;
	float			m_SplatterMaxSize;
	float			m_SplatterIntensityWet;
	float			m_SplatterIntensityDry;
	float			m_SplatterDryingTime;

	float			m_SoakStartSize;
	float			m_SoakEndSize;
	float			m_SoakStartSizeGravity;
	float			m_SoakEndSizeGravity;

	float			m_SoakScaleTime;
	float			m_SoakIntensityWet;
	float			m_SoakIntensityDry;
	float			m_SoakDryingTime;

	float			m_BloodFadeStartTime;
	float			m_BloodFadeTime;

	atHashString	m_ScarName;			// the name of the scar this blood will turn into (could be NULL)
	float			m_ScarStartTime;	// time when the scar should appear

	PAR_SIMPLE_PARSABLE;
	
public:										// the following are not in the xml file
	int				m_ScarIndex;			// direct index, so we don't need to look up by hash value all the time.
	float			m_SoakScaleMin;			// this is automatically calculated to scale the soak based on the wound/splatter min/max scales

	u16				m_UniqueTexComboIDGravity;		// used to determine if the wound/soak/splater texturess are the some as another CPedBloodDamageInfo(), when using the gravity soak
	u16				m_UniqueTexComboIDNonGravity;	// used to determine if the wound/soak/splater texturess are the some as another CPedBloodDamageInfo(), when using the nongravity soak
};


class CPedDamageDecalInfo
{
public:
	CPedDamageDecalInfo();
	virtual ~CPedDamageDecalInfo();

	virtual atHashValue GetNameHash() const {return m_Name.GetHash();} // this should be part of a base class!
	virtual const CPedDamageTexture & GetTextureInfo() const {return m_Texture;}
	virtual const CPedDamageTexture & GetBumpTextureInfo() const {return m_BumpTexture;}

	float GetFadeInTime()			{return m_FadeInTime;}			// how long does the fade in take
	float GetFadeOutTime()			{return m_FadeOutTime;}			// how long does the fade out take
	float GetFadeOutStartTime()		{return m_FadeOutStartTime;}	// when should the fade out start.

#if __BANK
	void			PreAddWidgets(bkBank& bank);
	void			PostAddWidgets(bkBank& bank);
#endif

	atHashString			m_Name;				// the name of the Damage Decal
	CPedDamageTexture		m_Texture;			// the Damage Decal texture data
	CPedDamageTexture		m_BumpTexture;		// the Damage Decal bumpmap texture (if "None" then no bumpmap)
	float					m_MinSize;			// the min max range this Damage Decal (will pick randomly between)
	float					m_MaxSize;			
	float					m_MinStartAlpha;	// for random alpha start (set the same for no random alpha, set to 1.0if not alpha by default)
	float					m_MaxStartAlpha;			
	float					m_FadeInTime;
	float					m_FadeOutTime;
	float					m_FadeOutStartTime;
	DamageDecalCoverageType	m_CoverageType;		// kSkinOnly is used for Scars/bruises/bandages/tattoos/dirt on skin, kClothOnly for pathes/logos/dirt/armor damage, NOTE: they cannot overlap

	PAR_SIMPLE_PARSABLE;
};


class CPedDamagePackEntry : public datBase
{
public:
	enum DamagePackEntryType {kDamagePackEntryTypeBlood,kDamagePackEntryTypeDamageDecal,kkDamagePackEntryTypeInvalid};
	
	CPedDamagePackEntry() : m_DamageName(ATSTRINGHASH("None",0x1d632ba1)),m_Zone(kDamageZoneTorso), m_uvPos(0.0f,0.0f), m_Rotation(0.0f), m_Scale(1.0f), m_ForcedFrame(-1), m_Type(kkDamagePackEntryTypeInvalid) {};
	void Apply(CPed * pPed, float preAge, float alpha);
	void PreCheckType(const char * packName);

#if __BANK
	void DamagePackUpdated();
#endif

public:	
	atHashString m_DamageName;
	ePedDamageZones m_Zone;
	Vector2 m_uvPos;
	float m_Rotation;
	float m_Scale;
	int m_ForcedFrame;

	PAR_SIMPLE_PARSABLE;

	u8 m_Type;			// not in parser file
};


class CPedDamagePack
{
public:
	CPedDamagePack() : m_Name(ATSTRINGHASH("None",0x1d632ba1)) {};
	void Apply(CPed * pPed, float preAge, float alpha);  // NOTE: alpha is only for decoration, blood does not support specifying alpha
	void PreCheckType();

public:
	atHashString m_Name;
	atArray<CPedDamagePackEntry> m_Entries;

	PAR_SIMPLE_PARSABLE;
};


class CPedDamageData
{
public:
	CPedDamageData();
	void Load(const char* fileName);
	void AppendUniques(const char* fileName);
	void UnloadAppend(const char* fileName);
	void OnPostLoad();

	void ClearArrays();

	template <typename T>
	int ElementExistsInArray(const T* theElement, const atArray<T*>& theArray) const
	{
		for(int j=0; j<theArray.GetCount(); ++j)
		{
			if(theArray[j]->m_Name == theElement->m_Name)
			{
				return j;
			}
		}
		return -1;
	}

	template <typename T>
	void AppendUniqueElements(atArray<T*>& to, atArray<T*>& from)
	{
		for(int i=0; i<from.GetCount(); ++i)
		{
			if( Verifyf((ElementExistsInArray<T>(from[i], to) == -1), 
				"Trying to append Ped Damage Data element with a hashname that already exists (%s). Delete entry or use override functionality instead. No warranty that unload will work properly!", 
				from[i]->m_Name.TryGetCStr()) )
			{
				to.PushAndGrow(from[i]);
			}
			else
			{
				delete from[i];
				from[i] = NULL;
			}
		}
	}

	template <typename T>
	void CleanupAppendedElements(atArray<T*>& to, atArray<T*>& from)
	{
		for(int i=0; i<from.GetCount(); ++i)
		{
			int index = ElementExistsInArray<T>(from[i], to);
			if(index != -1)
			{
				delete to[index];
				to[index] = NULL;
				to.Delete(index);
			}

			delete from[i];
			from[i] = NULL;
		}
	}

	template <typename T>
	void ClearPointerArray(atArray<T*>& arrayToDelete)
	{
		for(int i=0; i<arrayToDelete.GetCount(); ++i)
		{
			if(arrayToDelete[i])
			{
				delete arrayToDelete[i];
				arrayToDelete[i] = NULL;
			}
		}
		arrayToDelete.Reset();
	}

	int GetBloodDamageInfoIndex( atHashWithStringBank bloodNameHash);
	int GetDamageDecalInfoIndex( atHashWithStringBank decalNameHash);
	
	CPedBloodDamageInfo* GetBloodDamageInfoByIndex(int bloodDamageIndex);
	CPedDamageDecalInfo*  GetDamageDecalInfoByIndex(int damageDecalIndex);

	CPedBloodDamageInfo* GetBloodDamageInfoByHash(atHashWithStringBank bloodNameHash);
	CPedDamageDecalInfo*  GetDamageDecalInfoByHash(atHashWithStringBank decalNameHash);

	CPedDamageCylinderInfoSet* GetCylinderInfoSetByIndex(int cylInfSetIndex);
	CPedDamageCylinderInfoSet* GetCylinderInfoSetByPedType(PedTypes cylInfSetType);

	
#if __BANK
	void AddWidgets(rage::bkBank& bank);
	void SaveXML();
#endif

	Vector3 m_BloodColor;    // NOTE: at some point we should fix these values are bake them into the shader
	Vector3 m_SoakColor;

	float	m_BloodSpecExponent; 
	float	m_BloodSpecIntensity; 
	float	m_BloodFresnel;
	float	m_BloodReflection;

	float	m_WoundBumpAdjust;

	float   m_HiResTargetDistanceCutoff; 
	float   m_MedResTargetDistanceCutoff;
	float   m_LowResTargetDistanceCutoff;
	float   m_CutsceneLODDistanceScale;
	int		m_MaxMedResTargetsPerFrame; 
	int		m_MaxTotalTargetsPerFrame;
	int		m_NumWoundsToScarsOnDeathSP;
	int		m_NumWoundsToScarsOnDeathMP;
	int		m_MaxPlayerBloodWoundsSP;
	int		m_MaxPlayerBloodWoundsMP;
	Vector4 m_TattooTintAdjust;

	atArray<CPedBloodDamageInfo*> m_BloodData;
	atArray<CPedDamageDecalInfo*> m_DamageDecalData;
	atArray<CPedDamagePack*> 	  m_DamagePacks;
	atArray<CPedDamageCylinderInfoSet *> m_CylinderInfoSets;

	PAR_SIMPLE_PARSABLE;

private:
	void	SetUniqueTexId(CPedDamageTexture & damageTexture, int index, u16 &nextUniqueId);
};


class CPedDamageCylinder
{
public:
	CPedDamageCylinder();

	void Set(const Vector3 & base, const Vector3 & up, float height, float radius, float rotation, int zone)
	{
		m_Base = base;
		m_Up = up;
		m_Height = height;
		m_Radius = radius;
		m_Rotation = rotation;
		m_Zone = zone;
	}

	void Reset()
	{
		Set(ORIGIN,YAXIS,1,1,0,0);
	}

	void SetFromSkelData(const crSkeletonData &sd, s32 baseBone1, s32 baseBone2, s32 topBone2, float radius, const Vector3 & baseOffset, const Vector3 & topOffset, float rotation, int zone, float push=0.0f, float lengthScale=1.0f);
	void SetFromSkelData(const crSkeletonData &sd, s32 baseBone1, s32 baseBone2, s32 topBone, int zone, const CPedDamageCylinderInfo &cylInfo);

	Vector2 MapLocalPosToUV(const Vector3 & inPos);
	Vector2 AdjustForPushAreas(const Vector2 &uv, float radius, bool & limitScale, bool hasDress, Vector2 * uvPush=NULL, bool bFromWeapon=false);

	float GetHeight() const					{ return m_Height; }

	float GetRadius() const					{ return m_Radius; }

	const Vector3& GetBase() const			{ return m_Base; }

	const Vector3& GetInflateCenter() const { return m_Up; }		 	

	float GetInflateScale() const			{ return m_Height; }

	float GetRotation() const				{ return m_Rotation; }

	float CalcScale() const;
	float CalcAspectRatio() const;

#if __BANK
	void  DebugDraw(const Matrix34 &mtx);
	const atArray<CPedDamagePushArea> * GetPushArray() const { return m_PushAreaArray;}
#endif

private:
	Vector3	m_Base;
	Vector3 m_Up;			
	float	m_Height;		
	float	m_Radius;
	float	m_Rotation;
	int		m_Zone;
	const atArray<CPedDamagePushArea> * m_PushAreaArray;
};

class CPedDamageBlitBase
{
public:
	CPedDamageBlitBase():m_Done(1),m_RefAdded(0),m_DelayedClanTextureLoad(0) {}
	
	void				Set(const Vector4 & uvs, ePedDamageZones zone, ePedDamageTypes type, float alpha=1.0f, float preAge=0.0f, u8 flipUVflags=0x0);


	ePedDamageTypes		GetType() const					{return (ePedDamageTypes)m_Type;}
	ePedDamageZones		GetZone() const					{return (ePedDamageZones)m_Zone;}

	float				GetBirthTime() const			{return m_BirthTime;}
	Vec::Vector_4V_Out	GetUVCoords() const				{return Vec::V4Float16Vec4Unpack(&m_UVCoords);}
	float				GetAlphaIntensity() const		{return m_AlphaIntensity/63.0f;}
	u8					GetUVFlipFlags() const			{return m_FlipUVFlags;}

	bool				IsDone() const					{return m_Done==1;}
	void 				ForceDone()						{m_Done=1;}

	void				ClearRefAddedFlag()				{m_RefAdded = 0;}

protected:
	float			m_BirthTime;
	
	u8				m_Type:3;
	u8				m_Zone:3;
	u8				m_Done:1;
	u8				m_RefAdded:1;	

	u8				m_AlphaIntensity:6;		// not used for blood
	u8				m_FlipUVFlags:2;		// if the wound should flip the u and/or v direction of the textures if possible.

	u16				m_DelayedClanTextureLoad : 1;
	u16				m_Dummy2 : 15;			// unused byte
	
	Float16Vec4		m_UVCoords;
};

class CPedDamageBlitBlood : public CPedDamageBlitBase	
{
public:
	CPedDamageBlitBlood() {
		CPedDamageBlitBase::Set(Vector4(0.5f,0.5f,0.5f,0.5f), kDamageZoneTorso, kDamageTypeBlood, 0.0f, 1.0f, (u8)0x0);
	}
	
	void			Set(const Vector4 & uvs, ePedDamageZones zone, ePedDamageTypes type, CPedBloodDamageInfo * pBloodInfo, float scale, bool limitScale, bool enableSoakTextureGravity, float preAge=0.0, int forcedFrame=-1, u8 flipUVflags=0x0);
	float			GetScale() const				{return m_Scale;}
	void			SetScale(float scale)			{m_Scale=scale;}
	void			SetFromWeapon(bool fromWeapon)	{m_FromWeapon = fromWeapon;}
	void			SetReduceBleeding(bool reduce)	{m_ReduceBleeding = reduce;}
	void			SetLimitScale(bool limited)		{m_LimitScale = limited;}

	float			GetReBirthTime() const			{return m_ReBirthTime;}

	bool			IsScaleLimited() const			{return m_LimitScale==1;}
	bool			IsCloseTo(Vec2V_In uv) const;
	void			RenewHit(float preAge);
	void			Reset();
	bool			UpdateDoneStatus();
	void			FreezeFade(float time, float delta);

	void			UpdateScar(CPed * pPed);
	void			CreateScarFromWound(CPed * pPed, bool fadeIn, bool bScarOverride = false, float scarU = 0.f, float scarV = 0.f, float scarScale = 0.f);
	bool			CanLeaveScar();
	bool			DoesSoakUseGravity() const {return m_UseSoakTextureGravity!=0;}

	bool			RenderBegin(int bloodPass, int count) const;
	void			RenderEnd(int count) const;
	int				Render(int bloodPass, float scale, float aspectRatio, bool horizontalFlip, float currentTime) const;
	

	const CPedDamageTexture & GetSoakTextureInfo() const {return m_UseSoakTextureGravity ? m_BloodInfoPtr->m_SoakTextureGravity : m_BloodInfoPtr->m_SoakTexture;}
	const CPedDamageTexture & GetWoundTextureInfo() const {return m_BloodInfoPtr->m_WoundTexture;}
	const CPedDamageTexture & GetSplatterTextureInfo() const {return m_BloodInfoPtr->m_SplatterTexture;}
	const float				  GetSoakStartSize() const {return m_BloodInfoPtr->m_SoakStartSize;}
	const float				  GetSoakEndSize() const {return m_BloodInfoPtr->m_SoakEndSize;}

	const CPedBloodDamageInfo *GetBloodInfoPtr() const {return m_BloodInfoPtr;}

#if __BANK
	static	void	ReloadTextures();
	void			DumpScriptParams(bool dumpTitle) const;
	bool			DumpDamagePackEntry(int zone, bool first) const;
#endif

	void			DumpDecorations() const;

	bool			LoadDecorationTexture();

private:

	float			m_ReBirthTime;		// same as birth time the first time, but this can get reset when the same spot is hit again
	float			m_Scale;		
	u32				m_FixedSoakFrameOrAnimSequence:4;	  // if it's not animated: the fixed frame to use for Soak (0-15), if animated: the animation sequence index  
	u32				m_FixedWoundFrameOrAnimSequence:4;	  // if it's not animated: the fixed frame to use from Wound (0-15), if animated: the animation sequence index  
	u32				m_FixedSplatterFrameOrAnimSequence:4; // if it's not animated: the fixed frame to use from Splatter (0-15), if animated: the animation sequence index  
	u32				m_LimitScale:1;		
	u32				m_NeedsScar:1;
	u32				m_UseSoakTextureGravity:1;
	u32				m_FromWeapon:1;
	u32				m_ReduceBleeding:1;					 // when ped is shot after being dead, they should not bleed out.
	u32				m_Dummy:15;							 // unused bits for alignemnt

	CPedBloodDamageInfo * m_BloodInfoPtr;
};

struct PedDamageDecorationTintInfo
{
	PedDamageDecorationTintInfo() { Reset(); };
	~PedDamageDecorationTintInfo() { Reset(); };
	PedDamageDecorationTintInfo(const PedDamageDecorationTintInfo & that)  {*this = that;}

	PedDamageDecorationTintInfo & operator=(const PedDamageDecorationTintInfo & other);
	void Copy(const PedDamageDecorationTintInfo & other);

	u32					headBlendHandle;
	s32					paletteSelector;
	s32					paletteSelector2;
	grcTexture*			pPaletteTexture;
	bool				bValid;
	bool				bReady;

	void Reset();
};

class CPedDamageBlitDecoration : public CPedDamageBlitBase
{
public:
	CPedDamageBlitDecoration() : m_TxdId(-1)
	{
		atHashString nullHash;
		m_DelayedClanTextureLoad = 0;
		Set(nullHash, nullHash, NULL, NULL, Vector4(0.5f,0.5f,0.5f,0.5f), Vector2(1.0f, 1.0f), kDamageZoneTorso, kDamageTypeSkinDecoration, nullHash, nullHash, 1.0f, NULL);
	}
	
	~CPedDamageBlitDecoration() {Reset();} // takes care of refcounts, etc.
	CPedDamageBlitDecoration(const CPedDamageBlitDecoration & that)  {*this = that;}

	CPedDamageBlitDecoration & operator=(const CPedDamageBlitDecoration & other);
	void Copy(const CPedDamageBlitDecoration & other);

	bool			Set(atHashString colName, atHashString presetName, const PedDamageDecorationTintInfo* pTintInfo, const EmblemDescriptor* pEmblemDesc, const Vector4 & uvs, const Vector2 & scale, ePedDamageZones zone, ePedDamageTypes type, const strStreamingObjectName txdHash, const strStreamingObjectName txtHash, float alpha=1.0f, CPedDamageDecalInfo * pSourceInfo=NULL, float fadeInTime = 0.0f, float fadeOutStartTime=-1.0f, float fadeOutTime=0.0f, float preAge = 0.0f, int forcedFrame=-1, u8 flipUVflags=0x0);

	u32				GetTxdHash() const				{return m_TxdHash.GetHash();}
	u32				GetTxtHash() const				{return m_TxtHash.GetHash();}
	s32				GetTxdId() const				{return m_TxdId;}

	u32				GetFixedFrame() const			{return m_FixedFrameOrAnimSequence;}

	Vector2			GetScale() const				{return m_Scale;}
	
	const EmblemDescriptor& GetEmblemDescriptor() const { return m_EmblemDesc; }

	bool			HasBumpMap() const				{return m_Type==kDamageTypeSkinBumpDecoration || m_Type==kDamageTypeClothBumpDecoration;}
	bool			IsOnClothOnly() const			{return m_Type==kDamageTypeClothDecoration || m_Type==kDamageTypeClothBumpDecoration;}
	bool			IsOnSkinOnly() const			{return m_Type==kDamageTypeSkinDecoration || m_Type==kDamageTypeSkinBumpDecoration;}

	bool			LoadDecorationTexture(const CPed* pPed);
	grcTexture *	GetDecorationTexture() const		{return m_DecorationTexture;}
	bool			WaitingForDecorationTexture() const;
	void			ResetDecorationTexture()			{m_DecorationTexture = NULL;}

	void			Reset();
	bool			UpdateDoneStatus();				

	bool			RenderBegin(int pass, int count, s32 cachedTintPalSelector1, s32 cachedTintPalSelector2) const;
	void			RenderEnd(int count) const;
	int				Render(float aspectRatio, bool horizontalFlip, float currentTime) const;

	const CPedDamageTexture* GetTextureInfo() const { return (m_pSourceInfo) ? &(m_pSourceInfo->GetTextureInfo()) : NULL; }
	const CPedDamageTexture* GetBumpTextureInfo() const { return (m_pSourceInfo) ? &(m_pSourceInfo->GetBumpTextureInfo()) : NULL; }
	atHashValue		GetSourceNameHash()	const { return (m_pSourceInfo) ? m_pSourceInfo->GetNameHash() : atHashValue::Null(); }
	const CPedDamageDecalInfo* GetSourceInfo() const { return m_pSourceInfo; }

	atHashString	GetCollectionNameHash() const { return m_CollectionHash; };
	atHashString	GetPresetNameHash() const { return m_PresetHash; };

#if __BANK
	static	void	ReloadTextures();
	void			DumpScriptParams(bool dumpTitle) const;
	bool			DumpDamagePackEntry(int zone, bool first) const;
#endif

private:

	Vector2						m_Scale;

	grcTextureHandle			m_DecorationTexture;
	strStreamingObjectName		m_TxdHash;
	strStreamingObjectName		m_TxtHash;
	s32							m_TxdId;
	
	CPedDamageDecalInfo			*m_pSourceInfo;		// we need to know the source to access the texture animation info, and for save games.
	
	atHashString				m_CollectionHash;
	atHashString				m_PresetHash;

	u16							m_FixedFrameOrAnimSequence:5;	// save the random frame we pick when the blit was created
	u16							m_FadeInTime:11;				// how long to take to fade in 100 seconds max (saved as seconds*10)
	u16							m_FadeOutStartTime;				// when should this scar fade out (stored in seconds * 4, 0xffff means no fade needed)
	u16							m_FadeOutTime;					// how long does it take to fade out

	EmblemDescriptor			m_EmblemDesc;
	PedDamageDecorationTintInfo m_TintInfo;

};


class CPedDamageCompressedBlitDecoration : public CPedDamageBlitBase
{

public:
	CPedDamageCompressedBlitDecoration()  
	{
		atHashString nullHash;
		Set(nullHash, nullHash, NULL, NULL, Vector4(0.5f,0.5f,0.5f,0.5f), Vector2(1.0f, 1.0f), kDamageZoneTorso, kDamageTypeSkinDecoration, nullHash, nullHash);
	}

	~CPedDamageCompressedBlitDecoration() {Reset();}

	bool			Set(atHashString colName, atHashString presetName, const PedDamageDecorationTintInfo* pTintInfo, const EmblemDescriptor* pEmblemDesc, const Vector4 & uvs, const Vector2 & scale, ePedDamageZones zone, ePedDamageTypes type, const strStreamingObjectName txdHash, const strStreamingObjectName txtHash);

	u32				GetTxdHash() const				{return m_TxdHash.GetHash();}
	u32				GetTxtHash() const				{return m_TxtHash.GetHash();}

	bool			IsOnClothOnly() const			{return m_Type==kDamageTypeClothDecoration || m_Type==kDamageTypeClothBumpDecoration;}
	bool			IsOnSkinOnly() const			{return m_Type==kDamageTypeSkinDecoration || m_Type==kDamageTypeSkinBumpDecoration;}

	bool			LoadDecorationTexture(const CPed* pPed);
	void			ReleaseDecorationTexture();
	grcTexture *	GetDecorationTexture() const		{return m_DecorationTexture;}
	bool			WaitingForDecorationTexture() const;

	void			Reset();

	bool			RenderBegin(int pass, int count) const;
	void			RenderEnd(int count) const;
	int				Render(float aspectRatio, bool horizontalFlip) const;

	const CPedDamageTexture* GetTextureInfo() const { return (m_pSourceInfo) ? &(m_pSourceInfo->GetTextureInfo()) : NULL; }
	atHashValue			GetSourceNameHash()	  const { return (m_pSourceInfo) ? m_pSourceInfo->GetNameHash() : atHashValue::Null(); }

	atHashString		GetCollectionNameHash() const { return m_CollectionHash; };
	atHashString		GetPresetNameHash()	 const { return m_PresetHash; };

private:

	Vector2						m_Scale;
	grcTextureHandle			m_DecorationTexture;	// these 3 items are only needed for decorations, not blood, so we could move them to a separate class
	strStreamingObjectName		m_TxdHash;
	strStreamingObjectName		m_TxtHash;		 
	atHashString				m_CollectionHash;
	atHashString				m_PresetHash;
	EmblemDescriptor			m_EmblemDesc;

	CPedDamageDecalInfo			*m_pSourceInfo;		// we need to know the source to access the texture animation info, and for save games.
	PedDamageDecorationTintInfo	m_TintInfo;

};

//------------------------------------------------------------------------------
//	PURPOSE
//		manager the blood render target for a specific creature
//
class CPedDamageSetBase
{
	friend CPedDamageCompressedBlitDecoration;

public:

	CPedDamageSetBase() : m_BirthTimeStamp(0.0f), m_HasDress(false) {};
	virtual ~CPedDamageSetBase() {};

public:

	bool 						SetPed(CPed * pPed);
	CPed *						GetPed() {return m_pPed;}
	virtual void 				ReleasePed(bool)	{};
	virtual void				Reset()				{};

	static void					AllocateTextures();
	static void					ReleaseTextures();

	static void					DeviceLost();
	static void					DeviceReset();

	static void					InitViewports();
	static void					ReleaseViewports();

	static void					InitShaders();

	static void					SetupViewport(grcViewport *vp, int zone, bool sideWays, bool dualTarget, int pass);

#if __BANK
	static void					AddWidgets(rage::bkBank& bank);
	void						DebugDraw();
	static grcRenderTarget*		GetDebugTarget() {return sm_DebugTarget;}
#endif
	atRangeArray<CPedDamageCylinder, kDamageZoneNumZones> &GetZoneArray() {return m_Zones;}
	
#if RSG_DURANGO || RSG_ORBIS || RSG_PC
	enum { kTargetSizeW = 512, kTargetSizeH = 1280 };	// NOTE: if the ratio of kTargetSizeW to kTargetSizeH changes, we need to update "BloodTexelRatio" in pedcommond.fxh 
	enum { kMaxBloodDamageBlits = 128 }; // bumped up for nextgen, it does not have the PS3 vertex buffer size limit.
#else
	enum { kTargetSizeW = 256, kTargetSizeH = 640 };	// NOTE: if the ratio of kTargetSizeW to kTargetSizeH changes, we need to update "BloodTexelRatio" in pedcommond.fxh 
	enum { kMaxBloodDamageBlits = 63 }; // this is a very high number, we might be able to come down from 64 (changed to 63 so the max vertex batch size is 63*8*16 = 8064, which is under the ps3 limit of 8122)
#endif
	enum { kMaxDecorationBlits = 24 };  // this a fairly small number, we might need to go up if we're adding a bunch of scars 
	enum { kMaxTattooBlits = 63 };		// this is the max number of "custom" tattoos, these are not from the peddamage.xml file

	static void					DelayedRemoveRefFromTXD (s32 txdIdx);
	static void					ResetBloodBlitShaderVars();
	static void					ResetDecorationBlitShaderVars();

	static grcRenderTarget *	GetMirrorDecorationTarget()		{ return sm_MirrorDecorationRT; }
	static grcRenderTarget *	GetMirrorDepthTarget()			{ return sm_MirrorDepthRT; }
	static grcRenderTarget *	GetCompressedDepthTarget()		{ return sm_DepthTargets[3]; } // magic number 3?

	static	void				AdjustPedDataForMirror(Vec4V_InOut damageData);
protected:

	static int					DrawBlit(const Vector2 & endPoints, const Vector2 & dir, const Vector2 &normal, bool horizontalFlip, bool verticalFlip, u32 color, float frame);

	static void					SetBloodBlitShaderVars(const CPedDamageTexture & soakTextureinfo, const CPedDamageTexture & woundTextureinfo, const CPedDamageTexture & splatterTextureinfo);
	static void					SetDecorationBlitShaderVars( const grcTexture *tex, const grcTexture *palTex, const Vector3 & palSel, const CPedDamageTexture * textureInfo, const CPedDamageTexture * pBumpTextureInfo, bool bIsTattoo);

	static void					SetupHiStencil(bool forReading);
	static void					DisableHiStencil();
	static void					ProcessSkinAlpha(grcViewport & orthoViewport);

	static grcRenderTarget *	GetDepthTarget(const grcRenderTarget * colorTarget);

	static u16					AllocateStreamedPool(int hSize, int vSize, int texCount, const char *name);

#if RSG_ORBIS
public:
#else
protected:
#endif

	RegdPed						m_pPed;	 // this is the ped we're assigned to
	float						m_BirthTimeStamp;	
	bool						m_HasDress;

	atRangeArray<CPedDamageCylinder, kDamageZoneNumZones>				 m_Zones; // torso, head, left Arm, right Arm, left Left, Right Left 

	static atRangeArray<grcRenderTarget *,kMaxHiResBloodRenderTargets>  sm_BloodRenderTargetsHiRes;
	static atRangeArray<grcRenderTarget *,kMaxMedResBloodRenderTargets> sm_BloodRenderTargetsMedRes;
	static atRangeArray<grcRenderTarget *,kMaxLowResBloodRenderTargets> sm_BloodRenderTargetsLowRes;

	static atRangeArray<grcRenderTarget *,4>							sm_DepthTargets;

	static int					sm_TargetSizeW;
	static int					sm_TargetSizeH;

	static grcRenderTarget *	sm_MirrorDecorationRT;
	static grcRenderTarget *	sm_MirrorDepthRT;

	static Mat34V				sm_ZoneCameras[kDamageZoneNumZones][2];  // different camera setups for normal and rotated render targets
	static grmShader *			sm_Shader;
	static grcEffectVar			sm_BlitTextureID;
	static grcEffectVar			sm_BlitWoundTextureID;
	static grcEffectVar			sm_BlitSplatterTextureID;
	static grcEffectVar			sm_BlitNoBorderTextureID;

	static grcEffectVar			sm_BlitSoakFrameInfo;
	static grcEffectVar			sm_BlitWoundFrameInfo;
	static grcEffectVar			sm_BlitSplatterFrameInfo;
	static grcEffectVar			sm_BlitDecorationFrameInfoID;
	static grcEffectVar			sm_BlitDecorationTintPaletteSelID;
	static grcEffectVar			sm_BlitDecorationTintPaletteTexID;
	static grcEffectVar			sm_BlitDecorationTattooAdjustID;

	static grcVertexDeclaration	*sm_VertDecl;
	static float *				sm_ActiveVertexBuffer[NUMBER_OF_RENDER_THREADS];

	static bool					sm_Initialized;

#if __BANK
	static u16					sm_DebugRTPool;
	static grcRenderTarget *	sm_DebugTarget;
#endif

#if RSG_ORBIS
public:
#else
protected:
#endif

	static grcEffectTechnique	sm_DrawBloodTechniques[3];
	static grcEffectTechnique	sm_DrawSkinBumpTechnique;
	static grcEffectTechnique   sm_DrawDecorationTechnique[2];
};

class CPedDamageSet : public CPedDamageSetBase
{
	friend CPedDamageBlitBlood;
	friend CPedDamageBlitDecoration;
	friend CPedDamageCompressedBlitDecoration;

public:
	class CRenderTargetPtr
	{
	public:
		CRenderTargetPtr() { Set(NULL); }
		~CRenderTargetPtr() { Set(NULL); };
	public:
		void Set(grcRenderTarget **ppRenderTarget) { m_ppRenderTarget = ppRenderTarget; }
		grcRenderTarget *Get() const { return m_ppRenderTarget ? *m_ppRenderTarget : NULL; }
	private:
		grcRenderTarget **m_ppRenderTarget;
	};

public:

	CPedDamageSet();
	virtual ~CPedDamageSet();

public:

	void 						ReleasePed(bool autoRelease);
	void						Reset();
	void						SetRenderTargets(int index, int lod=kMedResDamageTarget);

	void						Clone( CPed*pPed, const CPedDamageSet & src);
	bool						Lock()     {bool temp = m_Locked; m_Locked=true; return temp;}
	void						UnLock()   {m_Locked=false;}
	bool						IsLocked() {return m_Locked;}

	int							AddDamageBlitByUVs(const Vector2 & uv, const Vector2 & uv2, ePedDamageZones zone, ePedDamageTypes type, CPedBloodDamageInfo * bloodInfo, bool limitScale, bool enableSoakTextureGravity, float scale, float preAge, int forcedFrame, u8 flipUVflags, bool bFromWeapon=false, bool bReduceBleeding=false); 
	bool						CalcUVs(Vector2 &uv, Vector2 &uv2, const Vector3 & localPos, const Vector3 & localDir, ePedDamageZones zone, ePedDamageTypes type, float radius, DamageBlitRotType rotType, bool enableSoakTextureGravity, bool bFromWeapon=false); 

	void						AddDecorationBlit ( atHashString colName, atHashString presetName, const PedDamageDecorationTintInfo* pTintInfo, const EmblemDescriptor* pEmblemDesc, ePedDamageZones zone, const Vector2 & uvPos, float rotation, const Vector2 &  scale, const strStreamingObjectName txdHash, const strStreamingObjectName txtHash, ePedDamageTypes type, float alpha = 1.0f, CPedDamageDecalInfo * pSourceInfo=NULL, float fadeInTime=0.0f, float fadeOutStart=-1.0f, float fadeOutTime=-1.0f, float preAge=0.0f, int forcedFrame=-1, u8 flipUVflags=0x0);

	void						ClearDecorations(bool keepNonStreamed=false, bool keepOnlyScarsAndBruises=false, bool keepOnlyOtherDecorations=false);
	void						ClearClanDecorations();
	void						ClearDecoration(int idx);
	void						ClearDamage();
	void						ClearDamage(ePedDamageZones zone);
#if GTA_REPLAY
	void						ReplayClearPedBlood(u32 bloodBlitID);
	bool						ReplayIsBlitIdValid(u32 bloodBlitID);
#endif //GTA_REPLAY
	void						LimitZoneBlood(ePedDamageZones zone, int limit);   // setting zone to kDamageZoneInvalid, set the limit for all zones, setting the limit to -1 mean no limit
	void						ClearDamageDecals( ePedDamageZones zone, atHashWithStringBank damageDecalNameHash); // kDamageZoneInvalid means all zones. NULL or ATSTRINGHASH("ALL",0x45835226) for name means all 

	void						SetClearColor(float redIntensity, float alphaIntensity) {m_ClearColor.Setf(redIntensity,0.0f,1-alphaIntensity,0.0f);}
	bool						PromoteBloodToScars(bool bScarOverride = false, float scarU = 0.f, float scarV = 0.f, float scarScale = 0.f);

	void 						SetReuseScore(float distanceToCamera, bool isVisible, bool isDead);
	float 						GetReuseScore()	const				{ return m_ReUseScore; }
	float						GetDistanceToCamera()const			{ return m_Distance; }
	u32 						GetLastVisibleFrame() const			{ return m_LastVisibleFrame; }
	grcRenderTarget*			GetActiveBloodDamageTarget() const	{ return m_ActiveDamageTarget.Get(); }
	grcRenderTarget*			GetActiveDecorationTarget() const	{ return m_ActiveDecorationTarget.Get(); }
	void						SetSkipDecorationTarget(bool enable){ m_NoDecorationTarget = enable;}

	void						SetForcedUncompressedDecorations(bool enable)	{ m_bForcedUncompressedDecorations = enable;}
	bool						GetForcedUncompressedDecorations() const		{ return m_bForcedUncompressedDecorations;}

	u16							CalcComponentBloodMask() const;

	u32							GetNumDecorations()	const			{ return m_DecorationBlitList.GetCount(); }
	bool						GetDecorationInfo(int idx, atHashString& colHash, atHashString& presetHash) const;

	static void					Render(const atUserArray<CPedDamageBlitBlood> & bloodArray, const atUserArray<CPedDamageBlitDecoration> &decorationArray, const grcRenderTarget * bloodDamageTarget, const grcRenderTarget *decorationTarget, const atRangeArray<CPedDamageCylinder, kDamageZoneNumZones> & zoneArray, bool bHasMedals, bool bHasDress, const Color32 & clearColor, bool bMirroMode, float currentTime, s32 cachedPalSel1, s32 cachedPalSel2);

	bool						DoesTargetNeedUpdate();
	bool						HasDecorations() const				{ return m_DecorationBlitList.GetCount()>0; }
	bool						NeedsDecorationTarget() const		{ return HasDecorations() && !m_NoDecorationTarget;}
	bool						NeedsBloodTarget() const			{ return HasBlood(); }

	bool						HasBlood() const					{ return m_BloodBlitList.GetCount()>0; }
	bool						HasMedals() const					{ return m_HasMedals; }
	bool						HasDress() const					{ return m_HasDress;}
	u8							GetBloodZoneLimit(int index) const	{ return m_BloodZoneLimits[index]; }
	Color32						GetClearColor() const				{ return m_ClearColor; }
	bool						IsForPlayer() const;
	bool						IsWaitingForTexture() const			{ return m_WaitingForTexture; }

	void						SetWasClonedFromLocalPlayer(bool clonedFromLocal) {m_ClonedFromLocalPlayer = clonedFromLocal;}
	bool						WasClonedFromLocalPlayer() const 				  {return m_ClonedFromLocalPlayer;}

	void						DumpDecorations();

	void						CheckForTexture();
	void						UpdateScars();
	void						FreezeBloodFade(float time, float delta);

	void						GetRenderTargets(grcTexture * &bloodTarget, grcTexture * &tattooTarget);
	int							GetBloodSplatCount(); // return the number of blood splats on this target (sometimes we want to limit them for mission AI
	float						GetLastDecorationTimeStamp() const { return m_LastDecorationTimeStamp;} // time stamp of last decoration from a collection.

	atVector<CPedDamageBlitDecoration> & GetDecorationList() {return m_DecorationBlitList;}
	const atVector<CPedDamageBlitDecoration> & GetDecorationList() const {return m_DecorationBlitList;}
	atFixedArray<CPedDamageBlitBlood, kMaxBloodDamageBlits> & GetPedBloodDamageList() { return m_BloodBlitList;}
	const atFixedArray<CPedDamageBlitBlood, kMaxBloodDamageBlits> & GetPedBloodDamageList() const { return m_BloodBlitList;}

	CPedDamageBlitBlood &		GetPedBloodDamageBlit(int index) { return m_BloodBlitList[index];}

	s32							GetLastValidTintPaletteSelector1() const { return m_lastValidTintPaletteSel; }
	s32							GetLastValidTintPaletteSelector2() const { return m_lastValidTintPaletteSel2; }
	void						SetLastValidTintPaletteSelector1(s32 index) { m_lastValidTintPaletteSel = index; }
	void						SetLastValidTintPaletteSelector2(s32 index) { m_lastValidTintPaletteSel2 = index; }

private:

	int							FindFreeDamageBlit();
	int							FindFreeDecorationBlit(bool bDontReuse = false);
	float						GetTypeScale(ePedDamageTypes type);
	int							SaveDamageBlit(const Vector2 & uv, const Vector2 &uv2, ePedDamageZones zone, ePedDamageTypes type, CPedBloodDamageInfo * pBloodInfo, bool limitScale, bool enableSoakTextureGravity, float scale, float preAge, int forcedFrame, u8 flipUVflags, bool bFromWeapon=false, bool bReduceBleeding=false);

private:

	atFixedArray<CPedDamageBlitBlood, kMaxBloodDamageBlits>		m_BloodBlitList; 
	atVector<CPedDamageBlitDecoration>							m_DecorationBlitList;

	float						m_Distance;
	float						m_LastDecorationTimeStamp;		// time stamp of the last decoration added (for network syncing)

	float						m_FadeAccumulation;

	bool						m_HasMedals;
	bool						m_WaitingForTexture;
	bool						m_NoDecorationTarget; // set this if the ped that uses this damage set also has compressed decorations, so we don't waste time/targets for something that will not be used.
	bool						m_Locked;
	bool						m_ClonedFromLocalPlayer;

	u8							m_BloodZoneLimits[kDamageZoneNumBloodZones];

	float						m_ReUseScore;
	u32							m_LastVisibleFrame;
	u16							m_TattooDecorationsCount;
	u16							m_MiscDecorationsCount;
	Color32						m_ClearColor;

	CRenderTargetPtr			m_ActiveDamageTarget;
	CRenderTargetPtr			m_ActiveDecorationTarget;

	s32							m_lastValidTintPaletteSel;
	s32							m_lastValidTintPaletteSel2;
	bool						m_bForcedUncompressedDecorations;
};

class CCompressedPedDamageSet : public CPedDamageSetBase
{
	friend CPedDamageCompressedBlitDecoration;
	friend class CompressedDecorationBuilder;

public:

	CCompressedPedDamageSet();
	virtual ~CCompressedPedDamageSet();

public:

	bool							IsAvailable() const	{return (m_RefCount==0);}
	void 							ReleasePed(bool autoRelease);
	void							Reset();
	void							PartialReset();
	void							AddRef() { m_RefCount++;}
	void							DecRef() { if (AssertVerify(m_RefCount>0)) m_RefCount--;}
	int								GetRefCount() { return m_RefCount;}

	bool							AddDecorationBlit (atHashString colName, atHashString presetName, const PedDamageDecorationTintInfo* pTintInfo, const EmblemDescriptor* pEmblemDesc, ePedDamageZones zone, const Vector2 & uvPos, float rotation, const Vector2 &  scale, const strStreamingObjectName txdHash, const strStreamingObjectName txtHash, ePedDamageTypes type);
	void							ClearDecorations();
	void							ClearDecoration(int idx);
	void							RestartCompression();
	void							ReleaseDecorationBlitTextures();

	static void						Render(const atUserArray<CPedDamageCompressedBlitDecoration> &decorationArray, const grcRenderTarget * blitTarget, const atRangeArray<CPedDamageCylinder, kDamageZoneNumZones> & zoneArray);

	u32								GetNumDecorations()	const									{ return m_DecorationBlitList.GetCount(); }
	bool							GetDecorationInfo(int idx, atHashString& colHash, atHashString& presetHash) const;

	bool							HasDecorations() const										{ return m_DecorationBlitList.GetCount()>0; }
	bool							IsWaitingForTexture() const									{ return m_State==kWaitingOnTextures; }

	bool							HasMedals() const											{ return m_HasMedals;}
	void							SetBlitTarget(grcRenderTarget* pTarget)						{ m_pBlitTarget = pTarget; }
	grcRenderTarget*				GetBlitTarget()												{ return  m_pBlitTarget; }
	void							SetTexture(grcTexture* pTexture)							{ m_pCompressedTexture = pTexture;}
	grcTexture*						GetTexture() const											{ return m_pCompressedTexture; }
	s32								GetTxdId() const											{ return m_txdSlot; }

	bool							HasAddedRefToOutputTexture() const							{ return m_bOutputTexRefAdded; }
	u16								CalcComponentMask();

	void							CheckForTexture();

	void							DumpDecorations();

	atVector<CPedDamageCompressedBlitDecoration> & GetDecorationList() { return m_DecorationBlitList; }
	const atVector<CPedDamageCompressedBlitDecoration> & GetDecorationList() const { return m_DecorationBlitList; }

	bool							SetOutputTexture(const strStreamingObjectName outTxdHash, const strStreamingObjectName outTxtHash);
	grcTexture*						GetOutputTexture() { return m_pOutTexture; };
	atHashString					GetOutputTextureHash() { return atHashString(m_OutTxtHash.GetHash()); };
	atHashString					GetOutputTxdHash() { return atHashString(m_OutTxdHash.GetHash()); };

private:

	bool							LoadOutputTexture();
	int								FindFreeDecorationBlit();

private:

	atVector<CPedDamageCompressedBlitDecoration> m_DecorationBlitList;  // this could be allocated only for "player" peds to save memory


	volatile eCompressedPedDamageSetState	m_State;				// this has no public interface, only modified by the class itself or the decoration builder.
	grcTextureHandle						m_pCompressedTexture;
	grcRenderTarget*						m_pBlitTarget;
	bool									m_bOutputTexRefAdded;
	bool									m_HasMedals;

	grcTextureHandle						m_pOutTexture;
	strStreamingObjectName					m_OutTxdHash;
	strStreamingObjectName					m_OutTxtHash;
	int										m_txdSlot;
	u16										m_ComponentMask;
	int										m_RefCount;
};

// For convienence put detail map manager in here
// with the ped damage stuff.
class PedDetailManager
{
	grcTextureHandle  m_DetailTextureVolume;
	grcTextureHandle  m_VehicleDetailTextureVolume;
public:
	void Init();
	void Shutdown();
};

//
// CCompressedTextureManager
// Auxiliary class to abstract away from client code the management
// of output textures for compressed decorations
class CCompressedTextureManager
{

public:

	CCompressedTextureManager() {};
	~CCompressedTextureManager() {};

	bool Init(bool bUseUncompressedTextures, int rtWidth, int rtHeight);
	void Shutdown();

	bool Alloc(atHashString& txdName, atHashString& txtName);
	void Release(atHashString txdName);

	bool AllocateRenderTargets(int iWidth, int iHeight);
	void ReleaseRenderTargets();

	grcRenderTarget* GetRenderTarget(atHashString& txdName);

private:

	int Find(atHashString txdName) const;

	struct CompressedTextureInfo
	{
		CompressedTextureInfo() : textureHash(0U), txdHash(0U), bInUse(false), pRT(NULL) {};
		atHashString		textureHash;
		atHashString		txdHash;
		grcRenderTarget*	pRT; // this will be NULL unless m_bEnableUncompressedTextures == true
		bool				bInUse;
	};

	atQueue<CompressedTextureInfo*,kMaxCompressedTextures> m_availableTextures;
	atFixedArray<CompressedTextureInfo, kMaxCompressedTextures> m_texturePool;

	// Storage for uncompressed decoration textures
	grcRenderTarget* m_renderTargetPool[kMaxCompressedTextures];
	bool m_bEnableUncompressedTextures;
};

//
// CompressedDecorationBuilder
// State machine that takes care of the blit and compress stages
// for a CCompressedPedDamageSet. Requests are always enqueued and 
// are processed one at a time
class CompressedDecorationBuilder
{
public:

	CompressedDecorationBuilder();
	~CompressedDecorationBuilder();

	bool IsRenderTargetAvailable();

	void Add(CCompressedPedDamageSet* pDamageSet);
	void Remove(CCompressedPedDamageSet* pDamageSet);

	void UpdateMain();
	void UpdateRender();

	void AddToDrawList();

	bool BlitPending() { return m_TargetState == kBlittingToTarget && m_RequestedBlitIndex!=m_RenderedBlitIndex;}

	bool AllocOutputTexture(atHashString& txdName, atHashString& txtName);
	void ReleaseOutputTexture(atHashString outputTexture);

	static void Init();
	static void Shutdown();
	static void RenderThreadUpdate();

	static CompressedDecorationBuilder& GetInstance() { return *smp_Instance; }

	bool		IsUsingCompressedMPDecorations()		const {return m_bEnableCompressedDecorations;}
	bool 		IsUsingHiResCompressedMPDecorations()   const {return m_bEnableHiresCompressedDecorations;}
	static bool	IsUsingUncompressedMPDecorations()		{return ms_bEnableUncompressedMPDecorations;}

#if __BANK
	void DebugDraw();
	void AddToggleWidget(bkBank&);
#endif

	enum 
	{
		kDecorationTargetAlignment = 4096,
		kDecorationTargetMips = 1, //the other mips aren't used, so having this at more than one seems a waste
		kDecorationTargetWidth = 128,
		kDecorationTargetHeight = 256,
		kDecorationTargetBpp = 32, 
		kDecorationHiResTargetRatio = 2,
		kDecorationUltraResTargetRatio = 4,
		kDecorationTargetPitch = kDecorationTargetWidth*kDecorationTargetBpp/8,
	};

	int		GetTargetHeight();
	int		GetTargetWidth();
	int		GetTargetPitch();

	u32 GetRequestedBlitIndex() const  { return m_RequestedBlitIndex;}
	void SetRequestedBlitIndex(u32 index) { m_RequestedBlitIndex = index;}

	u32 GetRenderedBlitIndex() const  { return m_RenderedBlitIndex;}
	void SetRenderedBlitIndex(u32 index) { m_RenderedBlitIndex = index;}

	CCompressedTextureManager & GetTextureManager() {return m_textureManager;}

	bool AllocateRenderTarget();
	void ReleaseRenderTarget();

#if PED_DECORATIONS_MULTI_GPU_SUPPORT
	bool IsGpuDone() const;
	bool SetGpuDoneAndCheckAll();
#endif //PED_DECORATIONS_MULTI_GPU_SUPPORT

private:

	bool IsQueueEmpty() {return (m_PendingBlitQueue.GetCount() == 0);}

	void ProcessQueue();
	void ProcessCurrentState();
	void SetUpCurrentSet();

	void AllocateRenderTargetPool();
	void ReleaseRenderTargetPool();

	static void KickOffTextureCompression(void);
	static void ProcessTextureCompression(void);

	bool IsTextureCompressionDone();

	void InitTextureManager();
	void ShutdownTextureManager();


	enum eCompressedTargetState
	{
		kBlittingToTarget,
		kBlitDone,
		kCompressingTarget,
		kAvailable,
		kNotAllocated,
	};

	enum eBlitState 
	{
		kWaitFrame0 = 0,
		kWaitFrameLast = 2,
		kWaitDone,
	};

	static CompressedDecorationBuilder*				smp_Instance;

	atQueue<CCompressedPedDamageSet*, kMaxCompressedDamagedPeds> m_PendingBlitQueue;
	static CCompressedPedDamageSet*					m_pCurSetToProcess;

	eCompressedTargetState							m_TargetState;
#if RSG_DURANGO
	static grcTexture*								m_pCompressionSrcTexture;
#endif
	static grcRenderTarget*							m_pCompressionSrcTarget;

	u16												m_RTPoolId;
	u32												m_RTPoolSize;

	u32												m_BlitState;
	u32												m_RequestedBlitIndex;
	u32												m_RenderedBlitIndex;	//warning: accessed from the render thread

	bool											m_bEnableCompressedDecorations;
	bool											m_bEnableHiresCompressedDecorations;
	static bool										ms_bEnableUncompressedMPDecorations;

	static sqCompressState							m_TextureCompressionState;

	static grcTexture*								m_pCurOutputTexture;

	static atHashString								m_outputTextureTxdHash;

	CCompressedTextureManager						m_textureManager;

	bool											m_bDontReleaseRenderTarget;

	static bool										m_HasFinishedCompressing;

#if PED_DECORATIONS_MULTI_GPU_SUPPORT
	u8												m_GpuDoneMask;			//warning: accessed from the render thread
#endif //PED_DECORATIONS_MULTI_GPU_SUPPORT
};

//////////////////////////////////////////////////////////////////////////

// Helper structure to cache all the data needed to re-apply scars obtained
// via GetScarDataForLocalPed
struct CPedScarNetworkData
{
	Vector2			uvPos;
	float			rotation;
	float			scale;
	float			age;
	int				forceFrame;
	u8				uvFlags;
	atHashWithStringBank	scarNameHash;
	ePedDamageZones zone;

	CPedScarNetworkData() : rotation(0.0f), scale(0.0f), age(0.0f), forceFrame(-1), uvFlags(0), scarNameHash(0U), zone(kDamageZoneInvalid) {}
};

struct CPedBloodNetworkData
{
	Vector2			uvPos;
	float			rotation;
	float			scale;
	float			age;
	u8				uvFlags;
	ePedDamageZones zone;
	atHashWithStringBank	bloodNameHash;

	CPedBloodNetworkData() : rotation(0.0f), scale(0.0f), age(0.0f), uvFlags(0), zone(kDamageZoneInvalid), bloodNameHash(0U) {}
};

// for compressed damage sets, we access through a proxy, so clones can share the compressed textures
class CCompressedPedDamageSetProxy
{
public:
	CCompressedPedDamageSetProxy() : m_pPed(NULL), m_pCompressedTextureSet(NULL) {}

	void					  SetCompressedTextureSet(CCompressedPedDamageSet* compressedTextureSet) {if (compressedTextureSet) compressedTextureSet->AddRef(); m_pCompressedTextureSet = compressedTextureSet;}
	CCompressedPedDamageSet * GetCompressedTextureSet() { return m_pCompressedTextureSet;}
	CPed*					  GetPed() { return m_pPed;}
	bool					  SetPed(CPed * pPed);
	void					  ReleasePed();
	void					  Clone(CPed * ped, const CCompressedPedDamageSetProxy * src);
	bool					  IsAvailable() const {return (m_pPed == NULL);}

public:
	CCompressedPedDamageSet * m_pCompressedTextureSet;
	RegdPed				      m_pPed;
};

//////////////////////////////////////////////////////////////////////////

class CPedDamageManager  
{
	REPLAY_ONLY(friend class CPacketPedBloodDamage;)
	REPLAY_ONLY(friend class CPacketPedBloodDamageScript;)
public:

	CPedDamageManager();
	~CPedDamageManager();

	static CPedDamageManager* smp_Instance;

public:

	static CPedDamageManager& GetInstance() { return *smp_Instance; }

	static void 	Init				(void);
	static void 	Shutdown			(void);

	static void		AddToDrawList		(bool bMainDrawList, bool bMirrorMode);
	static void		StartRenderList		(void);
	static void		PostPreRender		(void);
	static void		PreDrawStateSetup	(void);
	static void		PostDrawStateSetup	(void);

	void			CheckStreaming		(void);
	void			CheckPlayerForScars	(void);
	void			CheckCompressedSets	(void);

	void			UpdatePriority		(u8 damageIndex, float distance, bool isVisibility, bool isDead);	// called once a frame by ped to let use know hit priority info for recylcing

	void			AddPedBlood			( CPed * pPed, u16 component, const Vector3 & pos, atHashWithStringBank bloodNameHash, bool forceNotStanding=false);
	void			AddPedBlood			( CPed * pPed, ePedDamageZones zone, const Vector2 & uv, atHashWithStringBank bloodNameHash, bool forceNotStanding=false, float preAge=0.0f, int forcedFrame=-1);
	s32				AddPedBlood			( CPed * pPed, ePedDamageZones zone, const Vector2 & uv, float rotation, float scale, atHashWithStringBank bloodNameHash, bool forceNotStanding=false, float preAge=0.0f, int forcedFrame=-1, bool enableUVPush=false, u8 uvFlags = 255);

	void			AddPedStab			( CPed * pPed, u16 component, const Vector3 & pos, const Vector3 & dir,  atHashWithStringBank bloodNameHash);
	void			AddPedStab			( CPed * pPed, ePedDamageZones zone, const Vector2 & uv, const Vector2 & uv2, atHashWithStringBank bloodNameHash, float preAge=0.0f, int forcedFrame=-1);
	
	void			AddPedScar			( CPed * pPed, ePedDamageZones zone, const Vector2 & uvPos, float rotation, float scale, atHashWithStringBank scarNameHash, bool fadeIn = false, int forceFrame=-1, bool saveScar=true, float preAge = 0.0f, float alpha = -1.0f, u8 uvFlags = 255);
	void			AddPedDirt			( CPed * pPed, ePedDamageZones zone, const Vector2 & uvPos, float rotation, float scale, atHashWithStringBank dirtNameHash, bool fadeIn, float alpha = -1.0f);
	
	// this is useful for script to add damage decal from the pedDamage.xml file
	void			AddPedDamageDecal	( CPed * pPed, ePedDamageZones zone, const Vector2 & uvPos, float rotation, float scale, atHashWithStringBank damageDecalNameHash, bool fadeIn, int forceFrame=-1, float alpha=-1.0f, float preAge = 0.0f, u8 uvFlags = 255);

	void			AddPedDamageVfx		( CPed * pPed, u16 component, const Vector3 & pos, atHashWithStringBank damageNameHash, bool bReduceBleeding);

	// this version is used by script, it asks for the decoration texture by texture dictionary and texture name. it will request it be streamed in and the apply it when it's ready.
 	void			AddPedDecoration	( CPed * pPed, atHashString collectionHash, atHashString presetHash, ePedDamageZones zone, const Vector2 & uvPos, float rotation, const Vector2 &  scale, bool bRequiresTint, const strStreamingObjectName txdHash, const strStreamingObjectName txtHash, bool addToBumpMap, float alpha=1.0f, bool isClothOnly=false, int crewEmblemVariation=EmblemDescriptor::INVALID_EMBLEM_ID, CPedDamageDecalInfo * pSourceInfo=NULL, float fadeInTime = 0.0f, float fadeOutStart=-1.0f, float fadeOutTime=0.0f, float preAge=0.0f, int forcedFrame=-1, u8 flipUVflags=0x0);

	// these versions assume that the texture dictionary where the input/output texture/s live are packed in a registered RPF
	void			AddPedDecoration	( CPed * pPed, atHashString collectionHash, atHashString presetHash, ePedDamageZones zone, const Vector2 & uvPos, float rotation, const Vector2 &  scale, bool bRequiresTint, const char* txdName, const char * txtName, bool isClothOnly=false, int crewEmblemVariation=EmblemDescriptor::INVALID_EMBLEM_ID);

	void			DumpDecorations( CPed* pPed );

	void			AddPedDamagePack	( CPed * pPed, atHashWithStringBank damagePackName, float preAge, float alpha); // alpha is only for damage types that support it.

	bool			AddCompressedPedDecoration	( CPed * pPed, atHashString collectionHash, atHashString presetHash, ePedDamageZones zone, const Vector2 & uvPos, float rotation, const Vector2 &  scale, bool bRequiresTint, const char* inTxdName, const char * inTxtName, bool bApplyOnClothes, int crewEmblemVariation=EmblemDescriptor::INVALID_EMBLEM_ID);
	bool			AddCompressedPedDecoration	( CPed * pPed, atHashString collectionHash, atHashString presetHash, ePedDamageZones zone, const Vector2 & uvPos, float rotation, const Vector2 &  scale, bool bRequiresTint, const strStreamingObjectName inTxdHash, const strStreamingObjectName inTxtHash, bool bApplyOnClothes, int crewEmblemVariation=EmblemDescriptor::INVALID_EMBLEM_ID);
	bool			CheckForCompressedPedDecoration (CPed * pPed, atHashString collectionHash, atHashString presetHash);

	void			ClonePedDamage				(CPed *pPed, const CPed *source, bool bCloneCompressedDamage);
	void			SetPedBloodClearInfo		(u8 damageIndex, float redIntensity, float alphaIntensity);
	bool			WasPedDamageClonedFromLocalPlayer(u8 damageIndex) const;

	void			ClearPedBlood					(u8 damageIndex);
#if GTA_REPLAY
	void			ReplayClearPedBlood			(u8 damageIndex, u32 bloodBlitID);
	bool			ReplayIsBlitIdValid			(u8 damageIndex, u32 bloodBlitID);
#endif //GTA_REPLAY

	void			PromoteBloodToScars				(u8 damageIndex, bool bScarOverride = false, float scarU = 0.f, float scarV = 0.f, float scarScale = 0.f);
	void			LimitZoneBlood					(u8 damageIndex, ePedDamageZones zone, int limit);		// limit the number of blits drawn per zone (used for cutscenes). setting zone to kDamageZoneInvalid, set the limit for all zones, setting the limit to -1 mean no limit
	void			ClearDamageDecals				(u8 damageIndex, ePedDamageZones zone, atHashWithStringBank damageDecalNameHash); // kDamageZoneInvalid means all zones. NULL or ATSTRINGHASH("ALL",0x45835226) for name means all 

	void			ClearPedBlood					(u8 damageIndex, ePedDamageZones zone);
	void			ClearPedDecorations				(u8 damageIndex, bool keepNonStreamed=false, bool keepOnlyScarsAndBruises=false, bool keepOnlyOtherDecorations=false);
	void			ClearCompressedPedDecorations	(u8 damageIndex);
	void			ClearClanDecorations			(u8 damageIndex);
	void			ClearPedDecoration				(u8 damageIndex, int index);
	void			ClearCompressedPedDecoration	(u8 damageIndex, int index);

	void			ReleaseDamageSet			(u8 damageIndex);  // completely release the damage set
	void			ReleaseCompressedDamageSet	(u8 damageIndex);  // completely release the damage set

	void			GetRenderData		(u8 damageIndex, u8 compressedDamageIndex, grcTexture * &bloodTarget, grcTexture * &tattooTarget, Vector4& data);

	atHashString	GetCrewEmblemDecorationHash() const { return m_crewEmblemDecorationHash; }

	void			ForceDecorationsOnNewDamageSet(bool bEnable) { m_bAllowDecorationsOnNewDamageSet = bEnable; };

	bool			IsCrewEmblemTexture(atHashString txdHash) const { return (txdHash == m_crewEmblemDecorationHash); }

	void			ClearAllPlayerBlood();
	void			ClearAllPlayerDecorations();
	void			ClearAllPlayerScars(const CPed* pPed);
	void			ClearAllNonPlayerBlood();
	void			PromotePlayerBloodToScars();

	CPedDamageSet * GetPlayerDamageSet();

	const CCompressedPedDamageSet*	GetCompressedDamageSet	(const CPed* pPed)	const;
	const CPedDamageSet*			GetDamageSet			(const CPed* pPed)	const;

	CPedDamageData& GetDamageData()				{return m_DamageData;}

	const Vector4 *	GetPedDamageColors()		{return m_PedDamageColors;}

	static ePedDamageZones ConvertToNewDamageZone(int oldTargetZone);
	
	bool HasDecorations(const CPed* pPed) const;

	//////////////////////////////////////////////////////////////////////////
	// api for network synchronisation of ped scars
	bool	GetScarDataForLocalPed(CPed* pLocalPed, const atArray<CPedScarNetworkData>*& pOutScarDataArray);
	bool	GetBloodDataForLocalPed(CPed* pLocalPed, const atArray<CPedBloodNetworkData>*& pOutBloodDataArray);

	//////////////////////////////////////////////////////////////////////////
	static void		InitDLCCommands();

public:
	static u32		GetFrameID(void)									{return sm_FrameID;}
	
	static grcBlendStateHandle& GetBloodBlendState(void)				{return sm_Blood_BS;}
	static grcBlendStateHandle& GetBloodSoakBlendState(void)			{return sm_BloodSoak_BS;}
	static grcBlendStateHandle& GetBumpBlendState(void)					{return sm_Bump_BS;}
	
	static grcBlendStateHandle& GetDecorationBlendState(int index)		{return sm_Decoration_BS[index];}

	static grcBlendStateHandle& GetSkinDecorationProcessBlendState()	{return sm_SkinDecorationProcess_BS;}

	static grcDepthStencilStateHandle& GetSetStencilState()				{return sm_StencilWrite_RSS;}
	static grcDepthStencilStateHandle& GetUseStencilState(void)			{return sm_StencilRead_RSS;}

	static bool	GetClanIdForPed(const CPed* pPed, rlClanId& clanId);

	static bool		GetTintInfo					(const CPed* pPed, PedDamageDecorationTintInfo& tintInfo);

#if __BANK
	static void		TestBlood					(void);
	static void		TestBloodUV					(void);
	static void		TestStab					(void);
	static void		TestDecal					(void);
	static void		TestDamagePack				(void);
	static void		TestLimitZoneBlood			(void);
	static void		TestPromoteBloodToScars		(void);
	static void		DumpDecals					(void);
	static void		DumpDamagePack				(void);
	static void		DumpBlood					(void);
	static void		TestUpdate					(void);
	static void		TestUpdateBlood				(void);
	static void		TestUpdateBloodScriptParams	(void);
	static void		TestUpdateDecorations		(void);
	static void		TestUpdateDamagePack		(void);
	static void		TestMassiveDamage			(void);
	
	static void		UpdateDecorationTestModes	(void);

	static void		TestDamageClearPlayer		(void);
	static void		TestDamageClearNonPlayer	(void);
	static void		TestDamageBloodToScars		(void);
	static void		TestDecorationClear			(void);
	static CPed * 	GetDebugPed					(void);
	static void		UpdateDebugMarker			(void);
	static void		CheckDebugPed				(void);
	static void		TestClone					(void);

	static int 		GetDebugRenderMode			(void) {return sm_DebugRenderMode;}
	static void		SetDebugRenderMode			(int mode) {sm_DebugRenderMode=mode;}
	static bool		GetApplyDamage				(void) {return sm_ApplyDamage;}
	static void		AddWidgets					(rage::bkBank& bank);
	static void		DebugDraw					(void);
	static void		DebugDraw3D					(void);
	static void		SaveXML						(void);
	static void		SaveDebugTargets			(const grcTexture * bloodTarget, const grcTexture * tattooTarget);
	static void		ReloadTuning();
	static void		ReloadUpdate();
#endif

private:
	bool			IsPedStanding				(const CPed * pPed, bool bForceNotStanding);
	int				AddBloodDamage				( u8 damageIndex, u16 component, const Vector3 & pos, const Vector3 & dir, ePedDamageTypes type,  CPedBloodDamageInfo * pBloodInfo, bool forceNotStanding=false, bool bFromWeapon=false, bool reduceBleeding=false);
		
	ePedDamageZones ComputeLocalPositionAndZone	( u8 damageIndex, u16 component,  Vec3V_InOut pos, Vec3V_InOut dir);
	float			CalcLodScale				(void);

	CPedBloodDamageInfo * GetBloodInfoAndDamageId(CPed * pPed, atHashWithStringBank bloodNameHash, u8 & damageId);
	CPedDamageDecalInfo * GetDamageDecalAndDamageId(CPed * pPed, atHashWithStringBank scarNameHash, u8 & damageId);

	u8				GetDamageID					( CPed * pPed);

	u8				AllocateDamageSet			(CPed * ped);
	u8				AllocateCompressedDamageSetProxy(CPed * ped, bool forClone);

	void			SortPedDamageSets			(void);
	void			AddToDisplayList			(void);
	static void		RenderBefore				(void);
	static void		RenderAfter					(void);

private:
	CPedDamageData				m_DamageData;

	atRangeArray<CPedDamageSet*, kMaxDamagedPeds>	m_DamageSetPool;

	bool						m_NeedsSorting;
	static u32					sm_FrameID;

	grcTextureHandle			m_NoBloodTex;   // dummy textures when they don't have any damage
	grcTextureHandle			m_NoTattooTex;  // 

	atFixedArray<CPedDamageSet*, kMaxDamagedPeds>	m_ActiveDamageSets;
	
	Vector4						m_PedDamageColors[3];

	atHashString 				m_crewEmblemDecorationHash;

	bool						m_bAllowDecorationsOnNewDamageSet;

#if __BANK
	static const grcTexture *	sm_DebugDisplayDamageTex;
	static const grcTexture *	sm_DebugDisplayTattooTex;

	static u16					sm_DebugRTPool;
	static grcRenderTarget *	sm_DebugTarget;
	static int					sm_DebugRenderMode;
	static bool					sm_ApplyDamage;
	static bkGroup *			sm_WidgetGroup;
	static int					sm_ReloadCounter;

public:
	static bool					sm_EnableBlitAlphaOverride;
	static float				sm_BlitAlphaOverride;
private:
#endif

	static grcBlendStateHandle	sm_Decoration_BS[2];
	static grcBlendStateHandle	sm_Blood_BS;
	static grcBlendStateHandle	sm_BloodSoak_BS;
	static grcBlendStateHandle	sm_Bump_BS;
	static grcBlendStateHandle	sm_SkinDecorationProcess_BS;

	static grcDepthStencilStateHandle sm_StencilWrite_RSS;
	static grcDepthStencilStateHandle sm_StencilRead_RSS;

	PedDetailManager			m_DetailManager;

	atRangeArray<CCompressedPedDamageSetProxy*, kMaxCompressedDamagedPeds> m_CompressedDamageSetProxyPool;
	atRangeArray<CCompressedPedDamageSet*, kMaxCompressedDamagedPeds> m_CompressedDamageSetPool;

	// Cached scar data for the local ped; scars are now persistent and network code will need
	// to query this data to be able to re-apply scars
	static atArray<CPedScarNetworkData> sm_CachedLocaPedScarData;
	static bool							sm_IsCachedLocaPedScarDataValid;

	static atArray<CPedBloodNetworkData> sm_CachedLocalPedBloodData;
	static float						 sm_TimeStamp;
};

#define PEDDAMAGEMANAGER CPedDamageManager::GetInstance()
#define PEDDECORATIONBUILDER CompressedDecorationBuilder::GetInstance()

#endif
