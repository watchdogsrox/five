#pragma once

// game headers
#include "ai\debug\system\AIDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "scene\RegdRefTypes.h"

class CPedStealth;

class CPedFlagsDebugInfo : public CAIDebugInfo
{
public:

	CPedFlagsDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CPedFlagsDebugInfo() {}

	virtual void Print();

private:

	bool ValidateInput();
	void PrintConfigFlags();
	void PrintResetFlags();
	void PrintCoverFlags();

	RegdConstPed m_Ped;
};



class CPedNamesDebugInfo : public CAIDebugInfo
{
public:

	CPedNamesDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CPedNamesDebugInfo() {}

	virtual void Print();
	void PedModelPrint();
	void PedNamePrint();

private:

	bool ValidateInput();
	void PrintModelInfo();
	void PrintNames();

	RegdConstPed m_Ped;
};



class CPedGroupTextDebugInfo : public CAIDebugInfo
{
public:

	CPedGroupTextDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CPedGroupTextDebugInfo() {}

	virtual void Print();
	void GroupPrint();

private:

	bool ValidateInput();
	void PrintGroupText();

	RegdConstPed m_Ped;
};



class CStealthDebugInfo : public CAIDebugInfo
{
public:

	CStealthDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CStealthDebugInfo() {}

	virtual void Print();

private:

	void PrintDebug();
	void PrintOtherStealthFlags( const CPed& rPed );
	void PrintPedMotionInfo();
	void PrintPedPhysicsInfo();
	void PrintPedPerceptionInfo();
	void PrintPedStealthFlags( const CPedStealth& rPedStealth );
	void PrintPedStealthInfo();
	void PrintTargettingInfo();

	RegdConstPed m_pPed;
};



class CMotivationDebugInfo : public CAIDebugInfo
{
public:

	CMotivationDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams())
		: CAIDebugInfo(rPrintParams)
		, m_Ped(pPed)
	{}

	virtual ~CMotivationDebugInfo() {}

	virtual void Print();

	void MotivationPrint();

private:

	bool ValidateInput();
	void PrintMotivation();

	RegdConstPed m_Ped;
};



class CPedPersonalitiesDebugInfo : public CAIDebugInfo
{
public:

	CPedPersonalitiesDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CPedPersonalitiesDebugInfo() {}

	virtual void Print();

	void PersonalitiesPrint();

private:

	bool ValidateInput();
	void PrintPersonalities();

	RegdConstPed m_Ped;
};


class CPedPopulationTypeDebugInfo : public CAIDebugInfo
{
public:

	CPedPopulationTypeDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CPedPopulationTypeDebugInfo() {}

	virtual void Print();

	void TypePrint();

private:

	bool ValidateInput();
	void PrintPopulationText();

	RegdConstPed m_Ped;
};



class CPedGestureDebugInfo : public CAIDebugInfo
{
public: 
	CPedGestureDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CPedGestureDebugInfo() {}

	virtual void Print();

private:
	bool ValidateInput();
	void PrintGestureText();

	RegdConstPed m_Ped;
};



class CPedTaskHistoryDebugInfo : public CAIDebugInfo
{
public: 
	CPedTaskHistoryDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CPedTaskHistoryDebugInfo() {}

	virtual void Print();

private:
	bool ValidateInput();
	void PrintTaskHistory();

	RegdConstPed m_Ped;
};



class CPlayerDebugInfo : public CAIDebugInfo
{
public: 
	CPlayerDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CPlayerDebugInfo() {}

	virtual void Print();

private:
	bool ValidateInput();
	void PrintPlayerInfo();

	RegdConstPed m_Ped;
};



class CParachuteDebugInfo : public CAIDebugInfo
{
public: 
	CParachuteDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CParachuteDebugInfo() {}

	virtual void Print();

private:
	bool ValidateInput();
	void PrintParachuteInfo();

	RegdConstPed m_Ped;
};

#endif // AI_DEBUG_OUTPUT_ENABLED