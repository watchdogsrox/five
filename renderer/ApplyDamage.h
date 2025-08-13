//
// filename:	ApplyDamage.h
// description: ApplyDamage class
//

#ifndef INC_APPLYDAMAGE_H_
#define INC_APPLYDAMAGE_H_

#include "shader_source/vehicles/vehicle_common_values.h"

#if GPU_DAMAGE_WRITE_ENABLED

#include "system/memory.h"
#include "vector/vector4.h"
#include "atl/queue.h"
#include "atl/map.h"
#include "renderer/RenderPhases/RenderPhase.h"

#define VEHICLE_ROLLING_TEXTURE_ARRAY (RSG_PC)

namespace rage
{
	class bkBank;
	class grmShaderGroup;
	class grcRenderTarget;
}

class CVehicle;

class DamageDrawItem;

class CVehicleDeformation;

struct VehTexCacheEntry;

struct DamageDrawVehicle
{
	DamageDrawVehicle()
	{
		m_pVehicle		= NULL;
		m_DamageRT		= NULL;
		m_DamageTexture = NULL;
#if VEHICLE_ROLLING_TEXTURE_ARRAY
		m_pTexCache = NULL;
#endif
	}

	DamageDrawVehicle(CVehicle* pVehicle);
	~DamageDrawVehicle();

	CVehicle* m_pVehicle;
	grcRenderTarget* m_DamageRT;
	grcTexture* m_DamageTexture;

#if VEHICLE_ROLLING_TEXTURE_ARRAY
	VehTexCacheEntry* m_pTexCache;
#endif

	atQueue<DamageDrawItem*, MAX_IMPACTS_PER_VEHICLE_PER_FRAME> m_DamageDrawItems;
};


class CApplyDamage : public rage::datBase
{
public:
	friend class DamageDrawItem;
	typedef CApplyDamage this_type;

	CApplyDamage();
	~CApplyDamage();

	//Update
	static void RemoveVehicleDeformation(CVehicle* pVeh);
#if __BANK
	static bool HasAnyDamageDrawItemsInPool(CVehicle* pVeh);
	static int GetNumDamageDrawItemsInPool(CVehicle* pVeh);
#endif
	static void Update();

	static void Init();
	static void ShutDown();
	static void InitializePerlinNoiseTexture();

	//Draw Pass
	static void DrawDamage();
	static bool DrawDamageItemsForVehicle(DamageDrawVehicle& damagedVehicle);
	static bool Cleanup(DamageDrawVehicle& damagedVehicle);
#if VEHICLE_ROLLING_TEXTURE_ARRAY
	static bool DownloadAvailableDamageToCPU(DamageDrawVehicle& damagedVehicle);
#else
	static bool IsDamageReady(DamageDrawVehicle& damagedVehicle, bool &bEnableDamage);
#endif
	static void ResetDamage(CVehicle* pVehicle);
	static void EnqueueDamage(CVehicle* pVehicle, bool resetDamage);
	static int GetNumDamagePending(CVehicle *pVehicle);
	static DamageDrawVehicle *InsertDamageIntoMap(atMap<CVehicle*, DamageDrawVehicle>& mapToInsertTo, CVehicle *pVehicle);

	atMap<CVehicle*, DamageDrawVehicle> m_DamagedVehicleMap;
	static rage::sysIpcMutex sm_Mutex; // to sync the queue with the game thread

#if RSG_PC && MULTIPLE_RENDER_THREADS
	static void PrimaryRenderThreadBeginFrameCallback(void* UNUSED_PARAM(userdata));
#endif
#if RSG_PC
	static bool UseSeparateRenderTarget();
#endif

public:
	grcEffectTechnique		m_ApplyDamageTechnique;	
	grcEffectVar			m_ApplyDamageVar;
	grcEffectVar			m_ApplyDamageOffsetVar;
	grcEffectVar			m_NoiseTextureVar;
	grcEffectVar			m_DamageCopyTextureVar;

	static grcTexture* GetNoiseTexture() { return ms_NoiseTexture; };

private:
	static bool ms_bApplyDamageRenderPassEnabled;
	static CApplyDamage* s_pApplyDamage;
	static grcBlendStateHandle m_ApplyDamageBlendState;
	static grcTexture* ms_NoiseTexture;

#if RSG_PC
	static int ms_iGPUCount; // for SLI
	static bool ms_bCurrentFrame;
#endif
};


#endif //GPU_DAMAGE_WRITE_ENABLED

#endif //INC_APPLYDAMAGE_H_
