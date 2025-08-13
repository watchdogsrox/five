#ifndef _VEHICLEDRAWHANDLER_H_INCLUDED_
#define _VEHICLEDRAWHANDLER_H_INCLUDED_

#include "renderer/Entities/DynamicEntityDrawHandler.h"
#include "modelinfo/VehicleModelInfoVariation.h"

class CCustomShaderEffectVehicle;
class CVehicleModelInfo;
class CVehicle;

class CVehicleDrawHandler : public CDynamicEntityDrawHandler
{
public:
	CVehicleDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable);
	virtual ~CVehicleDrawHandler();
	virtual dlCmdBase* AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams);

	void ShaderEffect_HD_CreateInstance(CVehicleModelInfo* pVMI, CVehicle* pVehicle);
	void ShaderEffect_HD_DestroyInstance(CVehicle* pVehicle);

	const CVehicleVariationInstance& GetVariationInstance() const { return m_variation; }
	CVehicleVariationInstance& GetVariationInstance() { return m_variation; }

	void CopyVariationInstance(CVehicleVariationInstance& dest) { dest = m_variation; }

	dlSharedDataInfo& GetSharedVariationDataOffset() const { return m_SharedVariationDataInfo; }

	fwCustomShaderEffect* GetShaderEffectSD() const { return m_pStandardDetailShaderEffect; }

protected:
	fwCustomShaderEffect *m_pStandardDetailShaderEffect;

	CVehicleVariationInstance m_variation;
	mutable dlSharedDataInfo m_SharedVariationDataInfo;
};

#endif

