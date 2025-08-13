#pragma once

// game headers
#include "ai\debug\system\AIDebugInfo.h"
#include "modelinfo\ModelSeatInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED
#include "scene\RegdRefTypes.h"

class CTask;
class CTaskEnterVehicle;
typedef	taskRegdRef<const CTask> RegdConstTask;

class CTasksFullDebugInfo : public CAIDebugInfo
{
public:

	CTasksFullDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CTasksFullDebugInfo() {};

	virtual void Print();

private:

	bool ValidateInput();
	void PrintPlayerControlStatus();
	void PrintPedVehicleInfo();
	void PrintHealth();
	void PrintPrimaryTasks();
	void PrintMovementTasks();
	void PrintMotionTasks();
	void PrintSecondaryTasks();
	void PrintStateHistoryForTask(const CTask& rTask);
	void PrintCurrentEvent();
	void PrintStaticCounter();
	void PrintEnterVehicleDebug();
	void PrintNavMeshDebug();
	void PrintAudioDebug();
	void PrintInVehicleContextDebug();
	void PrintCoverDebug();

	RegdConstPed m_Ped;

};

class CTaskEnterVehicleDebugInfo : public CAIDebugInfo
{
public:

	CTaskEnterVehicleDebugInfo(const CTaskEnterVehicle* pEnterVehicleTask, s32 iDebugFlags, DebugInfoPrintParams printParams);
	virtual ~CTaskEnterVehicleDebugInfo() {};

	virtual void Print() {}
	virtual void Visualise() {}
	virtual const char* GetTypeName() = 0;

protected:

	bool ValidateInput();
	void PrintCommonDebug();
	bool IsFlagSet(s32 flag) const { return (m_DebugFlags & flag) ? true : false; }

	RegdConstTask m_Task;
	s32 m_DebugFlags;
};

class CSetSeatRequestTypeDebugInfo : public CTaskEnterVehicleDebugInfo
{
public:

	CSetSeatRequestTypeDebugInfo(SeatRequestType newSeatRequestType, s32 iChangeReason, const CTaskEnterVehicle* pEnterVehicleTask, s32 iDebugFlags, DebugInfoPrintParams printParams = DebugInfoPrintParams());
	virtual ~CSetSeatRequestTypeDebugInfo() {};

	virtual void Print();
	virtual void Visualise() {}
	virtual const char* GetTypeName() { return "SET_SEAT_REQUEST_TYPE"; }

private:

	SeatRequestType m_SeatRequestType;
	s32 m_ChangeReason;

};

class CSetTargetEntryPointDebugInfo : public CTaskEnterVehicleDebugInfo
{
public:

	CSetTargetEntryPointDebugInfo(s32 iNewEntryPoint, s32 iChangeReason, const CTaskEnterVehicle* pEnterVehicleTask, s32 iDebugFlags, DebugInfoPrintParams printParams = DebugInfoPrintParams());
	virtual ~CSetTargetEntryPointDebugInfo() {};

	virtual void Print();
	virtual void Visualise() {}
	virtual const char* GetTypeName() { return "SET_TARGET_ENTRY_POINT"; }

private:

	s32 m_NewEntryPoint;
	s32 m_ChangeReason;

};

class CSetTargetSeatDebugInfo : public CTaskEnterVehicleDebugInfo
{
public:

	CSetTargetSeatDebugInfo(s32 iNewSeat, s32 iChangeReason, const CTaskEnterVehicle* pEnterVehicleTask, s32 iDebugFlags, DebugInfoPrintParams printParams = DebugInfoPrintParams());
	virtual ~CSetTargetSeatDebugInfo() {};

	virtual void Print();
	virtual void Visualise() {}
	virtual const char* GetTypeName() { return "SET_TARGET_SEAT"; }

private:

	s32 m_NewSeat;
	s32 m_ChangeReason;

};

class CSetTaskFinishStateDebugInfo : public CTaskEnterVehicleDebugInfo
{
public:

	CSetTaskFinishStateDebugInfo(s32 iFinishReason, const CTaskEnterVehicle* pEnterVehicleTask, s32 iDebugFlags, DebugInfoPrintParams printParams = DebugInfoPrintParams());
	virtual ~CSetTaskFinishStateDebugInfo() {};

	virtual void Print();
	virtual void Visualise() {}
	virtual const char* GetTypeName() {  return "SET_TASK_FINISH_STATE"; }

private:

	s32 m_FinishReason;

};

class CGivePedJackedEventDebugInfo : public CAIDebugInfo
{
public:

	CGivePedJackedEventDebugInfo(const CPed* pJackedPed, const CTask* pTask, const char* szCallingFunction, DebugInfoPrintParams printParams = DebugInfoPrintParams());
	virtual ~CGivePedJackedEventDebugInfo() {};

	virtual void Print();
	virtual void Visualise() {}

private:

	bool ValidateInput();

	RegdConstPed m_JackedPed;
	RegdConstTask m_Task;
	char m_szCallingFunction[256];
};


#endif // AI_DEBUG_OUTPUT_ENABLED
