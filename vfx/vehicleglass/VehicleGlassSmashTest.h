// ========================================
// vfx/vehicleglass/VehicleGlassSmashTest.h
// (c) 2012 RockstarNorth
// ========================================

#ifndef VEHICLEGLASSSMASHTEST_H
#define VEHICLEGLASSSMASHTEST_H

#include "vfx/vehicleglass/VehicleGlass.h"

#if VEHICLE_GLASS_SMASH_TEST

namespace rage { class bkBank; }

class CVehicle;
class CVehicleGlassComponent;
class CVehicleGlassTriangle;

struct VfxCollInfo_s;

class CVehicleGlassSmashTestManager
{
public:
	void Init();
	void InitWidgets(bkBank* pVfxBank);

	void StoreCollision(const VfxCollInfo_s& info, int smashTestModelIndex);
	void UpdateSafe();
	void UpdateDebug();

	void SmashTest(CVehicle* pVehicle);
	void SmashTestUpdate();

	static void SmashTestAddTriangle(
		CVehicleGlassComponent* pComponent,
		const CVehicleGlassTriangle* pTriangle,
		Vec3V_In P0, Vec3V_In P1, Vec3V_In P2,
		Vec4V_In T0, Vec4V_In T1, Vec4V_In T2,
		Vec3V_In N0, Vec3V_In N1, Vec3V_In N2,
		Vec4V_In C0, Vec4V_In C1, Vec4V_In C2
	);

	void ShutdownComponent(CVehicleGlassComponent* pComponent);

	class CSmashTest
	{
	public:
		CSmashTest() {}
		CSmashTest(int modelIndex, const VfxCollInfo_s& info);

		static int CheckVehicle(const CVehicle* pVehicle);
		void Update();

		int m_modelIndex;
		atArray<VfxCollInfo_s> m_collisions;
	};

	atArray<CSmashTest> m_smashTests;
	RegdVeh             m_smashTestVehicle;
	Mat34V              m_smashTestVehicleMatrix;

	bool  m_smashTest;
	bool  m_smashTestSimple;
	bool  m_smashTestOutputWindowFlagsOnly;
	bool  m_smashTestOutputOBJsOnly;
	bool  m_smashTestConstructWindows;
	bool  m_smashTestAuto;
	u32   m_smashTestAutoTime; // time of last smash test
	float m_smashTestAutoTimeStep;
	float m_smashTestAutoRotation; // degrees over time step
	bool  m_smashTestAllGlassIsWeak;
	bool  m_smashTestAcuteAngleCheck;
	bool  m_smashTestTexcoordBoundsCheckOnly;
	bool  m_smashTestBoneNameMaterialCheck;
	bool  m_smashTestUnexpectedHierarchyIds;
	bool  m_smashTestCompactOutput;
	bool  m_smashTestDumpGeometryToFile;
	bool  m_smashTestReportOk;
	bool  m_smashTestReportSmashableGlassOnAnyNonSmashableMaterials;
	bool  m_smashTestReportSmashableGlassOnAllNonSmashableMaterials;
};

extern CVehicleGlassSmashTestManager g_vehicleGlassSmashTest;

void VehicleGlassSmashTest(CVehicle* pNewVehicle);

#endif // VEHICLE_GLASS_SMASH_TEST
#endif // VEHICLEGLASSSMASHTEST_H
