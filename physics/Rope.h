#ifndef ROPE_H_INC
#define ROPE_H_INC



#if __BANK


#include "atl/dlist.h"
#include "data/callback.h"

class CEntity;


namespace rage
{

class bkButton;
class ropeManager;
class ropeInstance;
class phInst;


class clothBankManager : public datBase
{
public:
	static void PickVehicle(); 
	static void PickPed(); 
	static void AttachToDrawableCallback(clothVariationData* pVarData);
	static void DetachFromDrawableCallback(clothVariationData* pVarData);	
};


class ropeBankManager : public datBase
{
public:

	ropeBankManager();
	~ropeBankManager();

	void InitWidgets( ropeManager* ptr );
	void DestroyWidgets();
	void Update(float elapsed);

	void DeleteRope( void* data );
	void DetachFromObjectA( void* data );
	void DetachFromObjectB( void* data );
	void BreakRope( void* data );
	void ToggleRopeCollision( void* data );

	void StartWinding( void* data );
	void StopWinding( void* data );

	void StartUnwindingFront( void* data );
	void StopUnwindingFront( void* data );

	void StartUnwindingBack( void* data );
	void StopUnwindingBack( void* data );

	void AttachToArray( void* data );

	void ToggleDebugInfo( void* data );

#if !__RESOURCECOMPILER
	void Load( void* data );
	void Save( void* data );
#endif

	void AddButton( const char* buttonText, ropeInstance* rope, phInst* instA, phInst* instB, rage::Member1 cbaddress );
	void DeleteAllNodes( ropeInstance* rope );

	void AddRopeGroup( ropeInstance* rope, phInst* instA, phInst* instB );
	
	void LoadFromRopeManager();
	void SetFrictionCB();

	static bkBank* GetBank();
	static void AddBank();
	static void ClearEndPos();
	static void ReloadAllNodes();

	struct RopeBankData {
		bkButton		*button;
		ropeInstance	*rope;
		phInst			*instA;
		phInst			*instB;
	};
	rage::atDList<RopeBankData>& GetRopeList() { return m_RopeList; }

private:
	int				m_RopeTypeIndex;

	CEntity*		m_pLastSelected;
	ropeManager*	m_RopeManager;
	Vector3			m_WorldPos;
	float			m_RopeLen;
	float			m_MaxRopeLen;
	float			m_MinRopeLen;
	float			m_LenA;				// when breaking a rope this is the length of the new rope attached to A
	float			m_LenB;				// when breaking a rope this is the length of the new rope attached to B
	float			m_RopeLenChangeRate;
	float			m_TimeMultiplier;		// to make rope light or heavy change this value from 0.0 ... to 10.0f ( too excesive )
	float			m_Friction;
	int				m_ComponentPartA;
	int				m_ComponentPartB;
	int				m_NumSegments;
	int				m_NumIterations;	
	bool			m_CreateEnabled;
	bool			m_Selection;
	bool			m_AttachAtPos;
	bool			m_UseDefaultPosRopeStart;
	bool			m_UseDefaultPosRopeEnd;
	bool			m_HasCollision;
	bool			m_DebugPickEdges;
	bool			m_PPUonly;
	bool			m_LockFromFront;
	bool			m_Breakable;
	bool			m_Pinned;

	rage::atDList<RopeBankData>	m_RopeList;
};


} // namespace rage

#endif


#endif // ROPE_H_INC
