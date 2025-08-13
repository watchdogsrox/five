//
// Filename:	NetworkRemoteCheaterDetector.h
// Description: Remote player cheater detection helper for network games
//

#ifndef _INC_NETWORK_REMOTE_CHEATER_DETECTOR_
#define _INC_NETWORK_REMOTE_CHEATER_DETECTOR_

// Forward declarations
class CNetGamePlayer;

class NetworkRemoteCheaterDetector
{
public:

	static inline void NotifyCrcMismatch(CNetGamePlayer* pNetPlayer, u32 mismatchSystem, u32 mismatchSubsystem, u32 crcResult) { 
		RemoteCheaterDetected(pNetPlayer, CRC_COMPROMISED, mismatchSystem, mismatchSubsystem, crcResult); 
	}

	static inline void NotifyMissingCrcReply(CNetGamePlayer* pNetPlayer, u32 mismatchSystem, u32 mismatchSubsystem) { 
		RemoteCheaterDetected(pNetPlayer, CRC_NOT_REPLIED, mismatchSystem, mismatchSubsystem, 0);
	}

	static inline void NotifyScriptDetectedCheat(CNetGamePlayer* pNetPlayer, u32 cheatDescr1, u32 cheatDescr2){
		RemoteCheaterDetected(pNetPlayer, SCRIPT_CHEAT_DETECTION, cheatDescr1, cheatDescr2, 0);
	}

	static inline void NotifyCrcRequestFlood(CNetGamePlayer* pNetPlayer, u32 systemRequestOnFlood, u32 subsystemRequestOnFlood){
		RemoteCheaterDetected(pNetPlayer, CRC_REQUEST_FLOOD, systemRequestOnFlood, subsystemRequestOnFlood, 0);
	}

	static inline void NotifyExeSize(CNetGamePlayer* pNetPlayer, u32 systemRequestOnFlood, u32 subsystemRequestOnFlood){
		RemoteCheaterDetected(pNetPlayer, CRC_EXE_SIZE, systemRequestOnFlood, subsystemRequestOnFlood, 0);
	}

	static inline void NotifyCRCFail(CNetGamePlayer* pNetPlayer, u32 versionAndType, u32 address, u32 value){
		RemoteCheaterDetected(pNetPlayer, CRC_CODE_CRCS, versionAndType, address, value);
	}

	static inline void NotifyTampering(CNetGamePlayer* pNetPlayer, const u32 param){
		RemoteCheaterDetected(pNetPlayer, CODE_TAMPERING, param, 0, 0);
	}

	static inline void NotifyTelemetryBlock(CNetGamePlayer* pNetPlayer){
		RemoteCheaterDetected(pNetPlayer, TELEMETRY_BLOCK, 0, 0, 0);
	}

#if RSG_PC
	static inline void NotifyGameServerCashWallet(CNetGamePlayer* pNetPlayer, const u32 param){
		RemoteCheaterDetected(pNetPlayer, GAME_SERVER_CASH_WALLET, param, 0, 0);
	}
	static inline void NotifyGameServerCashBank(CNetGamePlayer* pNetPlayer, const u32 param){
		RemoteCheaterDetected(pNetPlayer, GAME_SERVER_CASH_BANK, param, 0, 0);
	}
	static inline void NotifyGameServerInventory(CNetGamePlayer* pNetPlayer, const u32 param){
		RemoteCheaterDetected(pNetPlayer, GAME_SERVER_INVENTORY, param, 0, 0);
	}
	static inline void NotifyGameServerIntegrity(CNetGamePlayer* pNetPlayer, const u32 param){
		RemoteCheaterDetected(pNetPlayer, GAME_SERVER_SERVER_INTEGRITY, param, 0, 0);
	}
#endif //RSG_PC

	static void NotifyPlayerLeftBadSport(rlGamerHandle& remoteHandler, u32 cheatDescr1, u32 cheatDescr2);

	// Detectable cheats list. On telemetry, each cheat is identified by its enum value, so it adds less obvious stuff for the hackers.
	// Please add any new cheats at the end of the list, or inside the "once-detectable" section [0..MAX_DETECTABLE_ONCE_CHEATS)
	enum eDETECTABLE_CHEAT
	{
		// cheats contained in here get sent only once per player/session. Their "send only once" logic is stored on CNetGamePlayer level
		CRC_REQUEST_FLOOD = 0
		,CRC_COMPROMISED
		,CRC_NOT_REPLIED
		,CRC_EXE_SIZE
		,CRC_CODE_CRCS
		,CODE_TAMPERING
		,TELEMETRY_BLOCK

#if RSG_PC
		,GAME_SERVER_CASH_WALLET
		,GAME_SERVER_CASH_BANK
		,GAME_SERVER_INVENTORY
		,GAME_SERVER_SERVER_INTEGRITY

		,MAX_DETECTABLE_ONCE_CHEATS = 16
		// No cheats should get defined between MAX_DETECTABLE_ONCE_CHEATS and FIRST_DETECTABLE_N_TIMES_CHEAT
#else
		,MAX_DETECTABLE_ONCE_CHEATS = 8
		// No cheats should get defined between MAX_DETECTABLE_ONCE_CHEATS and FIRST_DETECTABLE_N_TIMES_CHEAT
#endif // RSG_PC

	
		// NOTE: continues from 64 so, if we ever need to increase bitsize of MAX_DETECTABLE_ONCE_CHEATS (to 16, 32,or 64), and its correspondent flags atFixedBitSet 
		// (to u16, u32, or u64), this enum won't need to change.
		// We shouldn't modify this enum existing values. Ever. Seriously. It's used on telemetry events data, so modifying anything "invalidates" or confuses any previous received telemetry
		,FIRST_DETECTABLE_N_TIMES_CHEAT = 64

		// cheats contained in here get sent by code every time they are detected
		,SCRIPT_CHEAT_DETECTION = FIRST_DETECTABLE_N_TIMES_CHEAT
	};

public:
	// Expose only number of bits needed to control which of those cheats were detected. Don't need to expose whole enum, that might remain private
	enum { SIZE_OF_CHEAT_NOTIFIED_MASK	 = MAX_DETECTABLE_ONCE_CHEATS };

private:

	static void RemoteCheaterDetected(CNetGamePlayer* pNetPlayer, eDETECTABLE_CHEAT cheat, u32 cheatExtraData1, u32 cheatExtraData2, u32 cheatExtraData3);
};

#endif	// _INC_NETWORK_REMOTE_CHEATER_DETECTOR_
