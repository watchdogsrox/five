// File header
#include "Task/Scenario/Info/ScenarioInfo.h"

// Rage headers
#include "ai/aichannel.h"
#include "fwanimation/clipsets.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

static const s32 MAX_SCENARIO_INFOS = 215;

#define LARGEST_SCENARIO_INFO_CLASS (MAX(sizeof(CScenarioCoupleInfo), sizeof(CScenarioVehicleParkInfo)))

CompileTimeAssert(sizeof(CScenarioInfo)						<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioPlayAnimsInfo)			<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioWanderingInfo)			<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioWanderingInRadiusInfo)	<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioGroupInfo)				<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioCoupleInfo)				<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioControlAmbientInfo)		<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioSkiingInfo)				<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioSkiLiftInfo)				<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioMoveBetweenInfo)			<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioSniperInfo)				<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioJoggingInfo)				<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioParkedVehicleInfo)		<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioVehicleInfo)				<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioVehicleParkInfo)			<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioFleeInfo)					<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioLookAtInfo)				<= LARGEST_SCENARIO_INFO_CLASS);
CompileTimeAssert(sizeof(CScenarioDeadPedInfo)				<= LARGEST_SCENARIO_INFO_CLASS);

FW_INSTANTIATE_BASECLASS_POOL(CScenarioInfo, MAX_SCENARIO_INFOS, atHashString("CScenarioInfo",0x2bd4dd73), LARGEST_SCENARIO_INFO_CLASS);

INSTANTIATE_RTTI_CLASS(CScenarioInfo,0x2BD4DD73);
INSTANTIATE_RTTI_CLASS(CScenarioPlayAnimsInfo,0xA8D9022D);
INSTANTIATE_RTTI_CLASS(CScenarioWanderingInfo,0x944632DD);
INSTANTIATE_RTTI_CLASS(CScenarioWanderingInRadiusInfo,0xCA104544);
INSTANTIATE_RTTI_CLASS(CScenarioGroupInfo,0x497DF031);
INSTANTIATE_RTTI_CLASS(CScenarioCoupleInfo,0xCB17D229);
INSTANTIATE_RTTI_CLASS(CScenarioControlAmbientInfo,0xFB1031A5);
INSTANTIATE_RTTI_CLASS(CScenarioSkiingInfo,0x507B8D45);
INSTANTIATE_RTTI_CLASS(CScenarioSkiLiftInfo,0xB1813161);
INSTANTIATE_RTTI_CLASS(CScenarioMoveBetweenInfo,0x816FB380);
INSTANTIATE_RTTI_CLASS(CScenarioSniperInfo,0xEF9C9639);
INSTANTIATE_RTTI_CLASS(CScenarioJoggingInfo,0x80C2FD66);
INSTANTIATE_RTTI_CLASS(CScenarioParkedVehicleInfo,0x37F4EB9);
INSTANTIATE_RTTI_CLASS(CScenarioVehicleInfo,0xFB9AD9D7);
INSTANTIATE_RTTI_CLASS(CScenarioVehicleParkInfo,0xD16B3229);
INSTANTIATE_RTTI_CLASS(CScenarioFleeInfo,0x13A07A5B);
INSTANTIATE_RTTI_CLASS(CScenarioLookAtInfo,0x15671697);
INSTANTIATE_RTTI_CLASS(CScenarioDeadPedInfo,0x373E0E03);

////////////////////////////////////////////////////////////////////////////////

CScenarioInfo::CScenarioInfo() : m_LastSpawnTime(0), m_Enabled(true) {}

CScenarioPlayAnimsInfo::CScenarioPlayAnimsInfo() : m_SeatedOffset(0.0f, 0.0f, 0.0f) {}

CScenarioWanderingInfo::CScenarioWanderingInfo() {}

CScenarioWanderingInRadiusInfo::CScenarioWanderingInRadiusInfo() : m_WanderRadius(0.0f) {}

CScenarioGroupInfo::CScenarioGroupInfo() {}

CScenarioCoupleInfo::CScenarioCoupleInfo() : m_MoveFollowOffset(Vector3::ZeroType), m_UseHeadLookAt(false) {}

CScenarioControlAmbientInfo::CScenarioControlAmbientInfo() {}

CScenarioSkiingInfo::CScenarioSkiingInfo() {}

CScenarioSkiLiftInfo::CScenarioSkiLiftInfo() {}

CScenarioMoveBetweenInfo::CScenarioMoveBetweenInfo() : m_TimeBetweenMoving(0.0f) {}

CScenarioSniperInfo::CScenarioSniperInfo() {}

CScenarioJoggingInfo::CScenarioJoggingInfo() {}

CScenarioParkedVehicleInfo::CScenarioParkedVehicleInfo() {}

CScenarioVehicleInfo::CScenarioVehicleInfo() {}

CScenarioVehicleParkInfo::CScenarioVehicleParkInfo() {}

CScenarioFleeInfo::CScenarioFleeInfo() : m_SafeRadius(0.0f) {}

CScenarioLookAtInfo::CScenarioLookAtInfo() {}

CScenarioDeadPedInfo::CScenarioDeadPedInfo() {}

u32 CScenarioInfo::GetPropHash() const 
{ 
	return m_PropName.GetHash(); 
}

#if !__FINAL
bool CScenarioInfo::HasProp() const
{
	if (GetPropHash() != 0)
	{
		return true;
	}
	return m_ConditionalAnimsGroup.HasProp();
}
#endif
