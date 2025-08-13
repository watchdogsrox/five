#ifndef LIGHT_ENTITY_H_
#define	LIGHT_ENTITY_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage headers
#include "vector/vector3.h"

// framework headers
#include "fwutil/idgen.h"
#include "fwscene/scan/VisibilityFlags.h"

// game headers
#include "scene/RegdRefTypes.h"
#include "Scene/Entity.h"
#include "scene/2deffect.h"
#include "renderer/lights/lights.h"

#include "vfx/misc/LODLightManager.h"


// =============================================================================================== //
// DEFINES
// =============================================================================================== //

#define LIGHTENTITY_FLAG_SCISSOR_LIGHT					(1 << 0)

// =============================================================================================== //
// CLASS
// =============================================================================================== //

class CLightExtension : public fwExtension
{
public:

	FW_REGISTER_CLASS_POOL(CLightExtension);
#if !__SPU
	EXTENSIONID_DECL(CLightExtension, fwExtension);
#endif //__SPU

	CLightExtension() {}

	virtual void InitEntityExtensionFromDefinition(const fwExtensionDef* definition, fwEntity* entity);
	CLightAttr* GetLightAttr(u8 hashCode);

	atArray<CLightAttr> m_instances;
};

class CLightEntity : public CEntity
{	
	public: ///////////////////////////
		FW_REGISTER_CLASS_PAGEDPOOL(CLightEntity);

		CLightEntity(const eEntityOwnedBy ownedBy);
		PPUVIRTUAL ~CLightEntity();

		PPUVIRTUAL Vector3	GetBoundCentre() const;
		PPUVIRTUAL void GetBoundCentre(Vector3& centre) const;
		PPUVIRTUAL float GetBoundRadius() const;
		PPUVIRTUAL float GetBoundCentreAndRadius(Vector3& centre) const;
		PPUVIRTUAL void GetBoundSphere(spdSphere& sphere) const;
		PPUVIRTUAL const Vector3& GetBoundingBoxMin() const;
		PPUVIRTUAL const Vector3& GetBoundingBoxMax() const;
		PPUVIRTUAL FASTRETURNCHECK(const spdAABB &) GetBoundBox(spdAABB& box) const;

		PPUVIRTUAL fwDrawData* AllocateDrawHandler(rmcDrawable* pDrawable);
		
		void ProcessLightSource(const bool addLightToScene, const bool addCorona, bool addToPreviouslights=false);

		void SetParent(CEntity* parent) { m_parentEntity=parent; }	
		CEntity* GetParent() const { return m_parentEntity; }	

		void ProcessVisibleLight(const fwVisibilityFlags& visFlags);

		float ProcessOne2dEffectLight(
			CLightSource& lightSource, 
			const Matrix34& mtx, 
			float intensityMult,
			fwInteriorLocation intLoc, 
			s32 texDictionary, 
			const CEntityFlags &entityFlags,
			const u32 lightOverrideFlags,
			const CLightAttr* lightData);

		float ProcessOne2dEffectLightCoronaOnly(
			CLightSource& lightSource, 
			const Matrix34& mtx, 
			float intensityMult,
			fwInteriorLocation intLoc,
			const u32 lightOverrideFlags,
			const CLightAttr* lightData);		

		fwUniqueObjId CalcShadowTrackingID();
		float CalcLightIntensity(const bool ignoreDayNightSettings, const CLightAttr* lightData);
		void ApplyEffectsToLight(const Matrix34 &mtx, float &intensityMult, bool &lightOn, u8 flashiness);
		static void ApplyEffectsToLightCommon(u8 flashiness, const Matrix34 &mtx, float &intensityMult, bool &lightOn);
		void AddCorona(const CLightSource &light, CLightAttr* lightData, bool bInteriorLocationValid);

		CLightAttr* GetLight() const;
		CLightAttr* GetLight(CLightAttr* pEffect) const;
		CLightShaftAttr* GetLightShaft() const;
		C2dEffect* Get2dEffect() const;

		void SetupLight(CEntity* ASSERT_ONLY(entity));
		void SetupLightShaft();

		bool GetWorldMatrix(Matrix34& mat) const;
		bool GetWorldMatrix(Matrix34& mat, C2dEffect *pEffect, CLightAttr* lightData) const;
		bool GetWorldPosition(Vector3& out) const;

		void SetLightBounds(const CLightAttr* attr, const Vector3& pos, const Vector3 &dir);
		void SetLightShaftBounds(const CLightShaftAttr* attr, bool addLightToScene = false);

		s32 Get2dEffectId() const { return (m_boundBox.GetUserInt1() >> 16) & 0xFFFF; }
		e2dEffectType Get2dEffectType() const { return (e2dEffectType)(m_boundBox.GetUserInt1() & 0xFF); }
		void Set2dEffectIdAndType(s32 effectId, u8 effectType) { m_boundBox.SetUserInt1(0 << 24 | effectId << 16 | effectType); }

		bool GetCustomFlag(u32 flag) { return ((m_boundBox.GetUserInt2() & flag) != 0); }
		void SetCustomFlag(u32 flag) { int flags = m_boundBox.GetUserInt2(); flags |= flag; m_boundBox.SetUserInt2(flags); }
		void ClearCustomFlag(u32 flag) { int flags = m_boundBox.GetUserInt2(); flags &= ~flag; m_boundBox.SetUserInt2(flags); }
		u32 GetCustomFlags() { return m_boundBox.GetUserInt2(); }
		void SetCustomFlags(u32 flags) { m_boundBox.SetUserInt2(flags); }

#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
		static bool ms_enableFogVolumes;
#endif

		// light color control:
		// B*4709986 - Scripting support - Override colour of rage lights inside a prop
		Color32	GetOverridenLightColor()	const					{ return m_overridenLightColor; }
		Color32	GetOriginalLightColor()		const;
		bool	IsLightColorOverriden()		const					{ return(m_overridenLightColor.GetAlpha()==255); }
		void	OverrideLightColor(bool enable, u8 r, u8 g, u8 b)	{ m_overridenLightColor.SetBytes(r,g,b, enable? 255:0); }

	private:
		RegdEnt m_parentEntity;
		spdAABB m_boundBox;
		Color32	m_overridenLightColor;	// if alpha=255, then light color has been overriden by script
};

class LightEntityMgr
{	
	public:
		static void Init(unsigned int);
		static void Shutdown(unsigned int);

		static void Update();	

		static u32 AddAttachedLights(CEntity* pEntity);
		static void RemoveAttachedLights(CEntity* pEntity);

		static void RequestFlush() { ms_flushLights = true; }
		static u32 Flush();

		static void UpdateLightsForEntity(const fwEntity *pEntity);
		static void UpdateLightsLocationFromEntity(const CEntity *pEntity);

	private:
		
		static void Remove(CLightEntity *lightEntity);
			
		static fwModelId ms_lightModelId;
		static bool ms_flushLights;
};

#endif // LIGHTOBJECTS_H
