#ifndef ISLAND_HOPPER_H_
#define ISLAND_HOPPER_H_

#include "atl/hashstring.h"
#include "data/base.h"

#include "islandhopperdata.h"

namespace rage
{
	class bkBank;
}

class CIslandHopperData;


class CIslandHopper : public rage::datBase
{
public:

	static CIslandHopper& GetInstance() { return sm_islandHopper; }

	void Init();
	void InitTunables();

	void LoadFile(char const* filename);
	void UnloadFile(char const* filename);

	void Update();

#if __BANK
	void InitWidgets(bkBank& bank);
#endif // __BANK

	void RequestToggleIsland(atHashString islandName, bool enable, bool waitForLoadscene = true);
	bool AreCurrentIslandIPLsActive() const;
	atHashString GetCurrentActiveIsland() const;
	bool IsAnyIslandActive() const;

	void DisableAllPreemptedImaps();

	void SetHeistIslandInteriors(bool interiorsEnabled);

private:

	void ToggleIsland(atHashString islandName, bool enable);

	void DebugToggleHeistIsland();

	CIslandHopperData const* FindIslandData(atHashString const& Name) const;

	void ActivateIsland(CIslandHopperData const& data);
	void DeactivateIsland(CIslandHopperData const& data);

	void ActivateHeistIsland();
	void DeactivateHeistIsland();

	void EnableHeistIslandImaps();
	void DisableHeistIslandImaps();
	void CheckIPLsActive();

	void RequestIpl(atHashString pName);
	void RemoveIpl(atHashString pName);
	void SetMapDataCullBoxEnabled(const char* pName, bool enabled);
	void EnableImaps(atHashString const* imaps, int imapCount);
	void DisableImaps(atHashString const* imaps, int imapCount);
	void PrintGrass();

	static CIslandHopper sm_islandHopper;

	bool m_requestEnableIsland;
	atHashString m_requestedIsland;
	bool m_waitForLoadscene;

	atHashString m_currentIslandEnabled;
	bool m_allIplsActiveDebug;

	static bool ms_tunabledSkipLoadSceneWait;

	atArray<CIslandHopperData*> m_islands;

};






#endif // ISLAND_HOPPER_H_