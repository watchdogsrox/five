/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    stuntjump.h
// PURPOSE : Logic to deal with the stunt jump
// AUTHOR :  Greg
// CREATED : 5/03/04
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _STUNTJUMP_H_
#define _STUNTJUMP_H_

// rage headers
#include "atl/singleton.h"
#include "vector/vector3.h"
#include "vector/color32.h"

// game headers
#include "fwtl/pool.h"
#include "text/messages.h"
#include "Frontend/Scaleform/ScaleformMgr.h"
#include "script/script_areas.h"

namespace rage
{
	class fwEntity;
}

class CStuntJump
{
public:
	FW_REGISTER_CLASS_POOL(CStuntJump);

	enum { INVALID_ID = -1 };

	// AABB
	CStuntJump(const spdAABB& start, const spdAABB& end, Vec3V_In vecCamera, bool bCamOptional, int iScore, int level, int id) :
		m_startBox(start),
		m_endBox(end),
		m_vecCamera(vecCamera),
		m_bIsCameraOptional(bCamOptional),
		m_iScore(iScore),
		m_level((u8)level),
		m_id((s8)id),
		m_bIsAngled(false){}

	// Angled BB
	CStuntJump(Vec3V_In startMin, Vec3V_In startMax, float startWidth, Vec3V_In endMin, Vec3V_In endMax, float endWidth, Vec3V_In vecCamera, bool bCamOptional, int iScore, int level, int id) :
		m_vecCamera(vecCamera),
		m_bIsCameraOptional(bCamOptional),
		m_iScore(iScore),
		m_level((u8)level),
		m_bIsAngled(true),
		m_id((s8)id)
	{
		m_startAngledBox.Set(VEC3V_TO_VECTOR3(startMin), VEC3V_TO_VECTOR3(startMax), startWidth, NULL);
		m_endAngledBox.Set(VEC3V_TO_VECTOR3(endMin), VEC3V_TO_VECTOR3(endMax), endWidth, NULL);
	}

	bool ShouldActivate(const CVehicle* vehicle);
	bool IsSuccessful(const CVehicle* vehicle);
	bool IsEnabled(const s32 enabledLevels) const {return (enabledLevels & (1<<m_level)) > 0;}

#if !__FINAL
	void DebugRender();
#endif

	spdAABB	m_startBox;
	spdAABB	m_endBox;

	CScriptAngledArea	m_startAngledBox;
	CScriptAngledArea	m_endAngledBox;

	Vec3V	m_vecCamera;
	int		m_iScore;
	u8		m_level;
	s8		m_id;
	bool	m_bIsAngled;
	bool	m_bIsCameraOptional;
};

//	typedef CPool<CStuntJump> CStuntJumpPool;

//
// name:		CStuntJumpManager
// description:	Manager class for stunt jump points
class CStuntJumpManager
{
	friend class CStuntJumpSaveStructure;

public:

	enum JumpState
	{
		JUMP_SHUTTING_DOWN,
		JUMP_INACTIVE,
		JUMP_ACTIVATED,
		JUMP_JUMPING,
		JUMP_LANDED,
		JUMP_LANDED_PRINTING_TEXT
	};

	CStuntJumpManager();

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();
	static void Render();
	static bool IsAStuntjumpInProgress();
	static bool IsStuntjumpMessageShowing();

	void UpdateHelper();
	void RenderHelper();
	void Clear();

	void AbortStuntJumpInProgress();
	int AddOne(Vec3V_In startMin, Vec3V_In startMax, Vec3V_In endMin, Vec3V_In endMax, Vec3V_In vecCamera, int iScore, int level=0, bool camOptional=false);
	int AddOneAngled(Vec3V_In startMin, Vec3V_In startMax, float startWidth, Vec3V_In endMin, Vec3V_In endMax, float endWidth, Vec3V_In vecCamera, int iScore, int level=0, bool camOptional=false);

	void DeleteOne(int id);
	void SetActive(bool bNewState);
	bool GetCameraPositionForStuntJump(Vector3& CameraPos);  
	bool IsStuntCamOptional() const;
	void EnableSet(int level);
	void DisableSet(int level);
	void RecalculateNumActiveJumps();

	bool IsActive() const {return m_bActive;}
	s32 GetNumJumps() const {return m_iNumJumps;}
	s32 GetNumActiveJumps() const {return m_iNumActiveJumps;}

#if __BANK
	static void InitWidgets();
#endif

	int GetStuntJumpFoundStat();
	u64 GetStuntJumpFoundMask();
	void IncrementStuntJumpFoundStat();
	int GetStuntJumpCompletedStat();
	int GetTotalStuntJumpCompletedStat();
	u64 GetStuntJumpCompletedMask();
	void IncrementStuntJumpCompletedStat();
	bool IsStuntJumpFound(const CStuntJump* pStuntJump);
	void SetStuntJumpFound(const CStuntJump* pStuntJump);
	bool IsStuntJumpCompleted(const CStuntJump* pStuntJump);
	void SetStuntJumpCompleted(const CStuntJump* pStuntJump);

	void ValidateStuntJumpStats(bool bFixIncorrectTotals);

	bool ShowOptionalStuntCameras() const { return m_bShowOptionalStuntCameras; }
	void ToggleShowOptionalStuntCameras(bool bShow) { m_bShowOptionalStuntCameras = bShow; } 

    // Returns the id of the last successful stunt jump for this session even if it has been completed already.
    s8 GetLastSuccessfulStuntJump() const;

private:
	void ResetJumpStats(const CVehicle* pVehicle);
	void UpdateJumpStats(const CVehicle* pVehicle);
	void SetNumJumps(s32 numJumps) {m_iNumJumps = numJumps;}
	void RegisterAddedStuntJump(const CStuntJump* p_newStuntJump);
	bool CanJump(bool& outCanKeepMessageUp) const;
	void SetMovieVisible(bool isVisible);

	void SetStuntJumpFoundStat(int number_of_found_stunt_jumps);
	void SetStuntJumpFoundMask(u64 bitMask);
	void SetStuntJumpCompletedStat(int number_of_completed_stunt_jumps);
	void SetStuntJumpCompletedMask(u64 bitMask);

#if __BANK
	void UpdateBitFieldWidgets(bool &bSetBit, bool &bClearAllBits, u64 &iBitSet, char *pBitSetAsString, bool bFound);
#endif	//	__BANK

	CStuntJump*		mp_Active;
	bool			m_bActive;
	bool			m_bHitReward;
	bool			m_bPrintStatsMessage;
	bool			m_bMessageVisible;
	float			m_fTimer;
	s32				m_iNumJumps;
	s32				m_iNumActiveJumps;
	s32				m_iStuntJumpCounter;
	u32				m_levelsEnabled;		
	JumpState		m_jumpState;
	eHUD_COLOURS	m_iTweenOutColour;

	CScaleformMovieWrapper m_messageMovie;
	int			m_clearTimer;
	Vector3		m_takeOffPosition;
	float		m_maxAltitude;
	float		m_distance;

	bool		m_bShowOptionalStuntCameras;

    // Id of the last stunt jump succeeded for the session
    s8          m_lastStuntJumpId;
	
	void PrintBigMessage(const char* pTitle, const char* pText, const char* pTextLine2, int duration, s32 NumberToInsert1 = NO_NUMBER_TO_INSERT_IN_STRING);
	void PrintBigMessageStats(const char* pTitle, const char* pTextTag, float distance, float height);
};

typedef atSingleton<CStuntJumpManager> SStuntJumpManager;

#endif
