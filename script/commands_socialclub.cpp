#include "script/script.h"
#include "script/wrapper.h"
#include "script/script_channel.h"
#include "script/script_hud.h"
#include "rline/socialclub/rlsocialclubcommon.h"
#include "commands_socialclub.h"
#include "rline/scachievements/rlscachievements.h"

#include "atl/array.h"

#include "fwnet/netprofanityfilter.h"
#include "fwnet/netlicenseplatecheck.h"

#include "event/EventNetwork.h"
#include "Network/Live/livemanager.h"
#include "Network/NetworkInterface.h"
#include "Network/Live/NetworkSCPresenceUtil.h"
#include "Network/SocialClubServices/GamePresenceEventDispatcher.h"
#include "Network/SocialClubServices/GamePresenceEvents.h"
#include "Network/SocialClubServices/SocialClubCommunityEventMgr.h"
#include "Network/SocialClubServices/SocialClubEmailMgr.h"
#include "Network/SocialClubServices/SocialClubInboxManager.h"
#include "Network/SocialClubServices/SocialClubNewsStoryMgr.h"

NETWORK_OPTIMISATIONS()
SCRIPT_OPTIMISATIONS()

//Shitty faux protection to handle unity vs. non-unity 
#ifndef __SCR_TEXT_LABEL_DEFINED
#define __SCR_TEXT_LABEL_DEFINED
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrTextLabel63);
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrTextLabel31);
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrTextLabel15);
#endif

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrUGCStateUpdate_Data);
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBountyInboxMsg_Data);
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sScriptEmailDescriptor);

// Should match with CNetworkSCNewsStoryRequest's data
// Keep in sync with SC_NEWS_STORY_DATA
struct sNewsStoryData
{
	scrValue m_title;
	scrValue m_subtitle;
	scrValue m_content;
	scrValue m_url;
	scrValue m_headline; // footer
	scrValue m_textureName;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sNewsStoryData);

namespace socialclub_commands
{
	int CommandInboxGetMessageCount()
	{
		return CSocialClubInboxMgrSingleton::GetInstance().GetTotalNumberOfMessages();
	}

	int CommandInboxGetNewMessageCount()
	{
		return CSocialClubInboxMgrSingleton::GetInstance().GetNumberOfNewMessages();
	}

	const char* CommandInboxGetRAWMessageTypeAtIndex(int index)
	{
		static char s_EventNameReturnValue[netGamePresenceEvent::MAX_EVENT_NAME_SIZE];
		const char* eventTypeName = CSocialClubInboxMgrSingleton::GetInstance().GetEventTypeNameAtIndex(index, s_EventNameReturnValue);
		if (eventTypeName)
		{
			return s_EventNameReturnValue;
		}

		return "";
	}

	int CommandInboxGetMessageTypeHashAtIndex(int index)
	{
		const char* eventTypeName = CommandInboxGetRAWMessageTypeAtIndex(index);

		scriptDebugf1("%s : SC_INBOX_GET_MESSAGE_TYPE_AT_INDEX - index='%d', eventTypeName='%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), index, eventTypeName);

		return atStringHash(eventTypeName);
	}

	bool CommandInboxGetIsReadAtIndex(int index)
	{
		return CSocialClubInboxMgrSingleton::GetInstance().GetIsReadAtIndex(index);
	}

	bool CommandInboxSetAsReadAtIndex(int index)
	{
		return CSocialClubInboxMgrSingleton::GetInstance().SetAsReadAtIndex(index);
	}

	bool CommandInboxIsMessageValid(int index)
	{
		return CSocialClubInboxMgrSingleton::GetInstance().IsIndexValid(index);
	}

	bool CommandInboxGetDataInt(int index, const char* name, int& value)
	{
		return CSocialClubInboxMgrSingleton::GetInstance().GetDataValueForMessage(index, name, value);
	}

	bool CommandInboxGetDataFloat(int index, const char* name, float& value)
	{
		return CSocialClubInboxMgrSingleton::GetInstance().GetDataValueForMessage(index, name, value);
	}

	bool CommandInboxGetDataString(int index, const char* name, scrTextLabel63* value)
	{
		return CSocialClubInboxMgrSingleton::GetInstance().GetDataValueForMessage(index, name, *value);
	}

	bool CommandInboxGetDataBool(int index, const char* name)
	{
		char returnValue[64];
		if (CSocialClubInboxMgrSingleton::GetInstance().GetDataValueForMessage(index, name, returnValue))
		{
			if (returnValue[0] == 'T' || returnValue[0] == 't')
			{
				return true;
			}
		}

		return false;
	}

	bool CommandInboxDoApplyOnEvent(int index)
	{
		return CSocialClubInboxMgrSingleton::GetInstance().DoApplyOnEvent(index);
	}

	void CommandInboxSendUGCUpdateToFriends(const char* contentId, int score)
	{
		CUGCStatUpdatePresenceEvent::Trigger(contentId, score, SendFlag_Inbox | SendFlag_AllFriends);
	}

	static const u32 InboxSendAllFriendsHash = ATSTRINGHASH("SC_Send_AllFriends", 0x33c3bcdf );
	static const u32 InboxSendCrewHash		= ATSTRINGHASH("SC_Send_Crew", 0x8DD639E0 );
	static const u32 InboxSendRecipListHash	= ATSTRINGHASH("SC_Send_RecipList", 0xBC5D8479 );
	
	atFixedArray<rlGamerHandle, RL_MAX_GAMERS_PER_SESSION> s_InboxRecipList;
	bool CommandInboxCanAddGamerToRecipientList()
	{
		return !s_InboxRecipList.IsFull();
	}

	void CommandInboxPushGamerToRecipientList(int& hGamerData)
	{
		rlGamerHandle hGamer;
		if(!CTheScripts::ImportGamerHandle(&hGamer, hGamerData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "SC_INBOX_PUSH_GAMER_TO_RECIP_LIST")))
			return;

		// this can be an assert for now, disable if it fires too much or i
		if(s_InboxRecipList.IsFull())
		{
			scriptErrorf("Recipient list full. Maximum of %d recipients - check SC_INBOX_CAN_PUSH_GAMER_TO_RECIP_LIST", RL_MAX_GAMERS_PER_SESSION);
			return;
		}

		s_InboxRecipList.Push(hGamer);
	}

	void CommandInboxSendUGCUpdateToRecipientListFull(scrUGCStateUpdate_Data* data)
	{
		if (!scriptVerifyf(s_InboxRecipList.GetCount() > 0, "No recipients in recipients list.  Use SC_INBOX_PUSH_GAMER_TO_RECIP_LIST"))
		{
			return;
		}

		//Send the event via the INBOX only.  
		CUGCStatUpdatePresenceEvent::Trigger(s_InboxRecipList.GetElements(), s_InboxRecipList.GetCount(), data);

		//Reset the recipient list to be ready for next time.
		s_InboxRecipList.Reset();
	}

	bool CommandInboxGetUGCUpdateData(int index, scrUGCStateUpdate_Data* data)
	{
		//Get the object at index
		//Make sure it's a UGCStatUPdate object
		CUGCStatUpdatePresenceEvent evt;
		if(!scriptVerifyf(CSocialClubInboxMgrSingleton::GetInstance().GetObjectAsType(index, evt), "SC_INBOX_MESSAGE_GET_UGCDATA - Message at index %d is either invalid or of the wrong type.", index))
		{
			return false;
		}
		
		//scriptAssertf(sizeof(data.m_gamerHandle) == SCRIPT_GAMER_HANDLE_SIZE, "Incorrect size of gamer handle int array in scrUGCStateUpdate_Data");
		CompileTimeAssert(sizeof(data->m_gamerHandle) == SCRIPT_GAMER_HANDLE_SIZE * sizeof(scrValue));
		const rlGamerHandle& gh = evt.GetGamerHandle();
		if(gh.IsValid() && !CTheScripts::ExportGamerHandle(&gh, data->m_gamerHandle[0].Int, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "SC_INBOX_MESSAGE_GET_UGCDATA")))
			return false;
		
		//convert it to the script object
		safecpy(data->m_missionName, evt.GetMissoinName());
		safecpy(data->m_contentId, evt.GetContentId());
		safecpy(data->m_fromGamerTag, evt.GetFromGamerTag());
		data->m_score.Int = evt.GetScore();
		safecpy(data->m_CoPlayerName , evt.GetCoPlayer());
		evt.GetMissionTypeInfo(data->m_MissionType.Int, data->m_MissionSubType.Int, data->m_Laps.Int);
		data->m_swapSenderWithCoPlayer.Int = evt.GetSwapSenderWithCoPlayer();

		return true;
	}

	bool CommandInboxGetBountyDataAtIndex(int index, scrBountyInboxMsg_Data* data)
	{
		//Get the object as the index and make sure it's the right type
		CBountyPresenceEvent evt;
		if(!scriptVerifyf(CSocialClubInboxMgrSingleton::GetInstance().GetObjectAsType(index, evt), "SC_INBOX_MESSAGE_GET_BOUNTY_DATA - Message at index %d is either invalid or of the wrong type.", index))
		{
			return false;
		}

		safecpy(data->m_fromGTag, evt.m_tagFrom);
		safecpy(data->m_targetGTag, evt.m_tagTarget);
		data->m_iCash.Int = evt.m_iCash;
		data->m_iOutcome.Int = evt.m_iOutcome;
		data->m_iRank.Int = evt.m_iRank;
		data->m_iTime.Int = evt.m_iTime;

		return true;
	}

	void CommandEmailRetrieveEmails(int startIndex, int numEmails)
	{
		if (!scriptVerifyf(NetworkInterface::IsGameInProgress(), "SC_EMAIL_RETRIEVE_EMAILS - a network game is not in progress"))
		{
			return;
		}

		CNetworkEmailMgr::Get().RetrieveEmails(startIndex, numEmails);
	}

	int	CommandEmailGetRetrievalStatus()
	{
		if (!scriptVerifyf(NetworkInterface::IsGameInProgress(), "SC_EMAIL_GET_RETRIEVAL_STATUS - a network game is not in progress"))
		{
			return -1;
		}

		return (int)CNetworkEmailMgr::Get().GetRetrievalStatus();
	}

	int	CommandEmailGetNumRetrievedEmails()
	{
		if (!scriptVerifyf(NetworkInterface::IsGameInProgress(), "SC_EMAIL_GET_NUM_RETRIEVED_EMAILS - a network game is not in progress"))
		{
			return 0;
		}

		return CNetworkEmailMgr::Get().GetNumRetrievedEmails();
	}

	bool CommandEmailGetEmailAtIndex(int index, sScriptEmailDescriptor* emailData)
	{
		if (!scriptVerifyf(NetworkInterface::IsGameInProgress(), "SC_EMAIL_GET_EMAIL_AT_INDEX - a network game is not in progress"))
		{
			return false;
		}

		return CNetworkEmailMgr::Get().GetEmailAtIndex(index, emailData);
	}

	void CommandEmailDeleteEmails(int& emailIdArray, int numIds)
	{
		int *emailIds = &emailIdArray;

		// skip array prefix
		emailIds+=SCRTYPES_ALIGNMENT;

		if (!scriptVerifyf(NetworkInterface::IsGameInProgress(), "SC_EMAIL_DELETE_EMAILS - a network game is not in progress"))
		{
			return;
		}

		if (!scriptVerifyf(numIds > 0, "SC_EMAIL_DELETE_EMAILS - numIds <= 0"))
		{
			return;
		}

		CNetworkEmailMgr::Get().DeleteEmails(emailIds, numIds);
	}

	atArray<rlGamerHandle> s_EmailRecipList;
	void CommandEmailPushGamerToRecipientList(int &hGamerData)
	{
		rlGamerHandle hGamer;
		if(!CTheScripts::ImportGamerHandle(&hGamer, hGamerData, SCRIPT_GAMER_HANDLE_SIZE  ASSERT_ONLY(, "SC_EMAIL_MESSAGE_PUSH_GAMER_TO_RECIP_LIST")))
			return;

		s_EmailRecipList.PushAndGrow(hGamer);
	}

	void CommandEmailClearRecipientList()
	{
		s_EmailRecipList.ResetCount();
	}

	void CommandEmailSendEmail(sScriptEmailDescriptor* emailData)
	{
		if (!scriptVerifyf(NetworkInterface::IsGameInProgress(), "SC_EMAIL_SEND_EMAIL - a network game is not in progress"))
		{
			return;
		}

		if (!scriptVerifyf(s_EmailRecipList.GetCount() > 0, "SC_EMAIL_SEND_EMAIL: No recipients in email recipients list.  Use SC_EMAIL_MESSAGE_PUSH_GAMER_TO_RECIP_LIST"))
		{
			return;
		}

		CNetworkEmailMgr::Get().SendEmail(SendFlag_Inbox, emailData, s_EmailRecipList.GetElements(), s_EmailRecipList.GetCount());

		//Reset the recipient list to be ready for next time.
		s_EmailRecipList.ResetCount();
	}

	bool CommandEmailSetCurrentEmailTag(const char* tag)
	{
		const bool result = CNetworkEmailMgr::Get().SetCurrentEmailTag(tag);
		scriptAssertf(result, "%s:SC_EMAIL_SET_CURRENT_EMAIL_TAG - Invalid tag [%s].",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), tag);

		return result;
	}

	bool CommandInboxSendBountyMsgToRecipientList(scrBountyInboxMsg_Data* pData)
	{
		if (!scriptVerifyf(s_InboxRecipList.GetCount() > 0, "No recipients in recipients list.  Use SC_INBOX_PUSH_GAMER_TO_RECIP_LIST"))
		{
			return false;
		}

		//Send the event via the INBOX only.  
		CBountyPresenceEvent::Trigger(s_InboxRecipList.GetElements(), s_InboxRecipList.GetCount(), pData);

		//Reset the recipient list to be ready for next time.
		s_InboxRecipList.Reset();

		return true;
	}

	bool CommandInboxSendAwardMsg(
		int NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(sendToTypeHash), 
		const char* NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(awardType),
		float NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(awardAmount))
	{
#if NET_GAME_PRESENCE_SERVER_DEBUG
		u32 sendToType = (u32)sendToTypeHash;

		SendFlag sendFlags = SendFlag_Inbox;
		if(sendToType == InboxSendRecipListHash)
		{
			bool success = CGameAwardPresenceEvent::Trigger_Debug(atStringHash(awardType), awardAmount, sendFlags, s_InboxRecipList.GetElements(), s_InboxRecipList.GetCount());
			
			//Reset the recipient list to be ready for next time.
			s_InboxRecipList.Reset();

			return success;
		}
		else if(sendToType == InboxSendAllFriendsHash || sendToType == InboxSendCrewHash)
		{
			if(sendToType == InboxSendAllFriendsHash)
			{
				sendFlags |= SendFlag_AllFriends;
			}
			else
			{
				sendFlags |= SendFlag_AllCrew;
			}

			return CGameAwardPresenceEvent::Trigger_Debug(atStringHash(awardType), awardAmount, sendFlags, NULL, 0);
		}

		return false;
#else
		return false;
#endif
 	}

	void CommandSCCacheNewRockstarMsg(bool bCache)
	{
		CRockstarMsgPresenceEvent::CacheNewMessages(bCache);
	}

	bool CommandSCHasNewRockstarMsg()
	{
		return CRockstarMsgPresenceEvent::HasNewMessage();
	}

	const char* CommandSCGetNewRockstarMsg()
	{
		return CRockstarMsgPresenceEvent::GetLastMessage();
	}

	bool CommandSCPresenceAttrSetInt(int attrId, int value)
	{
		return SCPRESENCEUTIL.Script_SetPresenceAttributeInt(attrId, value);
	}

	bool CommandSCPresenceAttrSetFloat(int attrId, float value)
	{
		return SCPRESENCEUTIL.Script_SetPresenceAttributeFloat(attrId, value);
	}

	bool CommandSCPresenceAttrSetString(int attrId, const char* value)
	{
		return SCPRESENCEUTIL.Script_SetPresenceAttributeString(attrId, value);
	}

	bool CommandSCPresenceSetActivityAttribute(int nActivityID, float fRating)
	{
		return SCPRESENCEUTIL.Script_SetActivityAttribute(nActivityID, fRating);
	}

	//////////////////////////////////////////////////////////////////////////
	//		SC Gamer Data
	bool CommandSCGamerDataGetInt(const char* name, int& value)
	{
		return CLiveManager::GetSCGamerDataMgr().GetValue(atStringHash(name), value);
	}

	bool CommandSCGamerDataGetFloat(const char* name, float& value)
	{
		return CLiveManager::GetSCGamerDataMgr().GetValue(atStringHash(name), value);
	}

	bool CommandSCGamerDataGetBool(const char* name)
	{
		bool value = false;
		if(CLiveManager::GetSCGamerDataMgr().GetValue(atStringHash(name), value))
		{
			return value;
		}

		return false;
	}

	bool CommandSCGamerDataGetString(const char* name, scrTextLabel63* value)
	{
		return CLiveManager::GetSCGamerDataMgr().GetValue(atStringHash(name), *value);
	}

	bool CommandSCGamerData_GetActiveXPBonus(float& xpBonus)
	{
		return CLiveManager::GetSCGamerDataMgr().GetActiveXPBonus(xpBonus);
	}

	bool CommandSCGamerData_GetLicensePlate(scrTextLabel15* licensePlate)
	{
		return CLiveManager::GetSCGamerDataMgr().GetValue(ATSTRINGHASH("licensePlate",0xF74208C2), *licensePlate);
	}

	void CommandSCGamerData_DebugSetCheater(bool BANK_ONLY(cheaterOverride))
	{
#if __BANK
		CLiveManager::GetSCGamerDataMgr().OverrideCheaterFlag(cheaterOverride);
#endif
	}

	void CommandSCGamerData_DebugSetCheaterRating(int BANK_ONLY(cheaterOverride))
	{
#if __BANK
		CLiveManager::GetSCGamerDataMgr().OverrideCheaterRating(cheaterOverride);
#endif
	}

	

	//////////////////////////////////////////////////////////////////////////
	//
	//	Profanity String checking
	//////////////////////////////////////////////////////////////////////////
	bool CommandCheckText(const char* inString, int& outToken)
	{
		if(inString && strlen(inString)>0)
		{
			return netProfanityFilter::VerifyStringForProfanity(inString, CText::GetScLanguageFromCurrentLanguageSetting(), false, outToken);
		}
		scriptAssertf(0, "%s - Checking Profanity of a NULL or empty string.",  CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}


	bool CommandCheckTextUGC(const char* inString, int& outToken)
	{
		if(inString && strlen(inString)>0)
		{
			return netProfanityFilter::VerifyStringForProfanity(inString, CText::GetScLanguageFromCurrentLanguageSetting(), true, outToken);
		}
		scriptAssertf(0, "%s - Checking UGC Profanity of a NULL or empty string.",  CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	bool CommandCheckText_IsValid(int token)
	{
		netProfanityFilter::ReturnCode status = netProfanityFilter::GetStatusForRequest(token);
		return status != netProfanityFilter::RESULT_INVALID_TOKEN;
	}

	bool CommandCheckText_IsPending(int token)
	{
		netProfanityFilter::ReturnCode status = netProfanityFilter::GetStatusForRequest(token);
		return status == netProfanityFilter::RESULT_PENDING;
	}

	bool CommandCheckText_PassedProfanityCheck(int token)
	{
		netProfanityFilter::ReturnCode status = netProfanityFilter::GetStatusForRequest(token);
		scriptAssertf(status == netProfanityFilter::RESULT_STRING_OK || status == netProfanityFilter::RESULT_STRING_FAILED || status == netProfanityFilter::RESULT_ERROR, 
			"%s - Checking Profanity status of a request [0x%8X] that is pending or invalid [%d].",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), token, status);

		return status == netProfanityFilter::RESULT_STRING_OK;
	}

	int CommandCheckText_GetStatusCode(int token)
	{
		netProfanityFilter::ReturnCode status = netProfanityFilter::GetStatusForRequest(token);
		return (int)status;
	}

	bool CommandCheckText_GetProfaneWord(int token, scrTextLabel63* outProfaneWord)
	{
		netProfanityFilter::ReturnCode status = netProfanityFilter::GetStatusForRequest(token);
		scriptAssertf(status == netProfanityFilter::RESULT_STRING_FAILED,
			"%s - Getting profane word on a request [0x%8X] that didnt fail [%d].",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), token, status);

		const char* profaneWord = netProfanityFilter::GetProfaneWordForRequest(token);
		if(profaneWord)
		{
			safecpy(*outProfaneWord, profaneWord);
			return true;
		}
		return status == netProfanityFilter::RESULT_STRING_FAILED;
	}

	//////////////////////////////////////////////////////////////////////////
	//
	//	License Plate Checking
	//////////////////////////////////////////////////////////////////////////
	bool CommandCheckLicensePlate(const char* inString, int& outToken)
	{
		return netLicensePlateCheck::ValidateLicensePlate(inString, CText::GetScLanguageFromCurrentLanguageSetting(), outToken);
	}

	bool CommandCheckLicensePlate_IsValid(int token)
	{
		netLicensePlateCheck::ReturnCode status = netLicensePlateCheck::GetStatusForRequest(token);
		return status != netLicensePlateCheck::RESULT_INVALID_TOKEN;
	}

	bool CommandCheckLicensePlate_IsPending(int token)
	{
		netLicensePlateCheck::ReturnCode status = netLicensePlateCheck::GetStatusForRequest(token);
		return status == netLicensePlateCheck::RESULT_PENDING;
	}

	bool CommandCheckLicensePlate_PassedValidityCheck(int token)
	{
		netLicensePlateCheck::ReturnCode status = netLicensePlateCheck::GetStatusForRequest(token);
		scriptAssertf(status == netLicensePlateCheck::RESULT_STRING_OK || status == netLicensePlateCheck::RESULT_STRING_FAILED, "%s - Checking Profanity status of a request [0x%8X] that is pending or invalid [%d].",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), token, status);
		return status == netLicensePlateCheck::RESULT_STRING_OK;
	}

	int CommandCheckLicensePlate_GetStatusCode(int token)
	{
		netLicensePlateCheck::ReturnCode status = netLicensePlateCheck::GetStatusForRequest(token);
		return (int)status;
	}

	int CommandCheckLicensePlate_GetCount(int token) 
	{
		return netLicensePlateCheck::GetCount(token);
	}

	const char* CommandCheckLicensePlate_GetPlate(int token, int index)
	{
		return netLicensePlateCheck::GetPlate(token, index);
	}

	int CommandGetChangeLicensePlateDataStatus()
	{
		return netLicensePlateChange::GetStatusForRequest( );
	}

	const char* CommandCheckLicensePlate_GetPlateData(int token, int index)
	{
		return netLicensePlateCheck::GetPlateData(token, index);
	}

	bool CommandCheckLicensePlate_GetAddData(int token, const char* inString, const char* inData)
	{
		scriptAssertf(inString, "%s - SC_LICENSEPLATE_GET_ADD_PLATE - NULL inString.",  CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(inData, "%s - SC_LICENSEPLATE_GET_ADD_PLATE - NULL inData.",  CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(netLicensePlateCheck::GetCount(token) < netLicensePlateCheck::MAX_NUM_PLATES, "%s - SC_LICENSEPLATE_GET_ADD_PLATE - Max number of plates %d.",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), netLicensePlateCheck::GetCount(token));
		
		if (inString && inData && netLicensePlateCheck::GetCount(token) < netLicensePlateCheck::MAX_NUM_PLATES)
		{
			rlScLicensePlateInfo plate;
			sysMemSet(plate.m_plateText, 0, rlScLicensePlateInfo::MAX_PLATE_NAME_CHARS);
			sysMemSet(plate.m_plateData, 0, rlScLicensePlateInfo::MAX_PLATE_DATA_CHARS);

			formatf(plate.m_plateText, rlScLicensePlateInfo::MAX_PLATE_NAME_CHARS, inString);
			formatf(plate.m_plateData, rlScLicensePlateInfo::MAX_PLATE_NAME_CHARS, inData);

			return netLicensePlateCheck::UpdateWithAddPlate(token, plate);
		}

		return false;
	}

	bool CommandChangeLicensePlateData(const char* oldPlatetext, const char* newPlatetext, const char* plateData)
	{
		scriptAssertf(oldPlatetext, "%s - SC_LICENSEPLATE_SET_PLATE_DATA - NULL oldPlatetext.",  CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(newPlatetext, "%s - SC_LICENSEPLATE_SET_PLATE_DATA - NULL newPlatetext.",  CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(plateData, "%s - SC_LICENSEPLATE_SET_PLATE_DATA - NULL plateData.",  CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (plateData && oldPlatetext && newPlatetext)
		{
			const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
			const rlRosCredentials& cred = rlRos::GetCredentials(localGamerIndex);
			if (cred.IsValid())
			{
				return netLicensePlateChange::ChangeLicensePlate(localGamerIndex, oldPlatetext, newPlatetext, plateData);
			}
		}

		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	//
	//	License Plate Add
	//////////////////////////////////////////////////////////////////////////
	bool CommandAddLicensePlate(const char* inString, const char* inData, int& outToken)
	{
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		const rlRosCredentials& cred = rlRos::GetCredentials(localGamerIndex);
		if (cred.IsValid())
			return netLicensePlateAdd::AddLicensePlate(localGamerIndex, inString, inData, CText::GetScLanguageFromCurrentLanguageSetting(), outToken);
	
		return false;
	}

	bool CommandAddLicensePlate_IsPending(int token)
	{
		netLicensePlateAdd::ReturnCode status = netLicensePlateAdd::GetStatusForRequest(token);
		return status == netLicensePlateAdd::RESULT_PENDING;
	}

	int CommandAddLicensePlate_GetStatusCode(int token)
	{
		netLicensePlateAdd::ReturnCode status = netLicensePlateAdd::GetStatusForRequest(token);
		return (int)status;
	}

	//////////////////////////////////////////////////////////////////////////
	//
	//	License Plate IsValid
	//////////////////////////////////////////////////////////////////////////
	bool CommandIsValidLicensePlate(const char* inString, int& outToken)
	{
		return netLicensePlateIsValid::IsValidLicensePlate(inString, CText::GetScLanguageFromCurrentLanguageSetting(), outToken);
	}

	bool CommandIsValidLicensePlate_IsPending(int token)
	{
		netLicensePlateIsValid::ReturnCode status = netLicensePlateIsValid::GetStatusForRequest(token);
		return status == netLicensePlateIsValid::RESULT_PENDING;
	}

	int CommandIsValidLicensePlate_GetStatusCode(int token)
	{
		netLicensePlateIsValid::ReturnCode status = netLicensePlateIsValid::GetStatusForRequest(token);
		return (int)status;
	}

	//////////////////////////////////////////////////////////////////////////


	bool CommandSocialClubEvent_IsActiveForType(const char* eventType)
	{
		return SocialClubEventMgr::Get().IsEventActive(eventType);
	}

	int CommandSocialClubEvent_GetEventIdForType(const char* eventType)
	{
		return SocialClubEventMgr::Get().GetActiveEventCode(eventType);
	}

	bool CommandSocialClubEvent_GetExtraDataIntForType(const char* name, int& value, const char* eventType)
	{
		return SocialClubEventMgr::Get().GetExtraEventData(name, value, eventType);
	}

	bool CommandSocialClubEvent_GetExtraDataFloatForType(const char* name, float& value, const char* eventType)
	{
		return SocialClubEventMgr::Get().GetExtraEventData(name, value, eventType);
	}

	bool CommandSocialClubEvent_GetExtraDataStringForType(const char* name, scrTextLabel63* value, const char* eventType)
	{
		return SocialClubEventMgr::Get().GetExtraEventData(name, *value, eventType);
	}

	bool CommandSocialClubEvent_GetDisplayNameForType(scrTextLabel63* value, const char* eventType)
	{
		return SocialClubEventMgr::Get().GetEventDisplayName(*value, eventType);
	}

	bool CommandSocialClubEvent_GetDisplayNameById(int eventId, scrTextLabel63* value)
	{
		return SocialClubEventMgr::Get().GetEventDisplayNameById(eventId, *value);
	}

	bool CommandSocialClubEvent_GetExtraDataFloatById(int eventId, const char* name, float &value )
	{
		return SocialClubEventMgr::Get().GetExtraEventData(eventId,name, value);
	}

	bool CommandSocialClubEvent_GetExtraDataStringById(int eventId,const char* name,  scrTextLabel63* value)
	{
		return SocialClubEventMgr::Get().GetExtraEventData(eventId, name, *value);
	}

	bool CommandSocialClubEvent_GetExtraDataIntById(int eventId, const char* name, int& value)
	{
		return SocialClubEventMgr::Get().GetExtraEventData(eventId,name, value);
	}

	bool CommandSocialClubEvent_IsActiveById(int eventId)
	{
		return SocialClubEventMgr::Get().IsEventActive(eventId);
	}

	bool CommandSocialClubEvent_IsActive()
	{
		return CommandSocialClubEvent_IsActiveForType(NULL);
	}

	int CommandSocialClubEvent_GetEventId()
	{
		return CommandSocialClubEvent_GetEventIdForType(NULL);
	}

	bool CommandSocialClubEvent_GetExtraDataInt(const char* name, int& value)
	{
		return CommandSocialClubEvent_GetExtraDataIntForType(name, value, NULL);
	}

	bool CommandSocialClubEvent_GetExtraDataFloat(const char* name, float& value)
	{
		return CommandSocialClubEvent_GetExtraDataFloatForType(name, value, NULL);
	}

	bool CommandSocialClubEvent_GetExtraDataString(const char* name, scrTextLabel63* value)
	{
		return CommandSocialClubEvent_GetExtraDataStringForType(name, value, NULL);
	}

	bool CommandSocialClubEvent_GetDisplayName(scrTextLabel63* value)
	{
		return CommandSocialClubEvent_GetDisplayNameForType(value, NULL);
	}

	//	SC RePopulate Event handling
	void CommandSocialClubEvent_RepopulateDoRequest()
	{
		SocialClubEventMgr::Get().DoScheduleRequest();
	}

	bool CCommandSocialClubEvent_RepopulateIsRequestPending()
	{
		return SocialClubEventMgr::Get().Pending();
	}

	bool CommandSocialClubEvent_RepopulateDidLastRequestSucceed()
	{
		return SocialClubEventMgr::Get().GetDataReceived();
	}

	//////////////////////////////////////////////////////////////////////////
	//
	// Social Club Ticker News
	//
	//////////////////////////////////////////////////////////////////////////

	bool CommandShowNextNewsItem(int UNUSED_PARAM(delaySecsBeforeNextShow))
	{
		// Deprecated
		return false;
	}

	bool CommandShowPrevNewsItem(int UNUSED_PARAM(delaySecsBeforeNextShow))
	{
		// Deprecated
		return false;
	}
	
	//////////////////////////////////////////////////////////////////////////
	//
	// Social Club Transition News
	//
	//////////////////////////////////////////////////////////////////////////
	bool CommandQueueTransitionStory()
	{
		return CNetworkTransitionNewsController::Get().ReadyNextRequest(false); // return success so script can know to turn off gametips
	}

    bool ShowTransitionNewsCommon( s32 const movieID, CNetworkTransitionNewsController::eMode const mode )
    {
        return CNetworkTransitionNewsController::Get().InitControl( CScriptHud::GetScaleformMovieID(movieID), mode ); // return success so script can know to turn off gametips
    }
	
	bool CommandShowTransitionNews(s32 const movieID)
	{
		return ShowTransitionNewsCommon( movieID, CNetworkTransitionNewsController::MODE_SCRIPT_INTERACTIVE );
	}

	bool CommandShowTransitionNewsTimed(s32 const movieID, int iStoryOnscreenDuration)
	{
        CNetworkTransitionNewsController::eMode const c_mode = iStoryOnscreenDuration != 0 ? CNetworkTransitionNewsController::MODE_SCRIPT_CYCLING : CNetworkTransitionNewsController::MODE_SCRIPT_INTERACTIVE;
		return ShowTransitionNewsCommon( movieID, c_mode ); 
	}

	bool CommandShowNextTransitionNewsItem()
	{
		return CNetworkTransitionNewsController::Get().SkipCurrentNewsItem();
	}
	
	bool CommandShowPrevTransitionNewsItem ()
	{
		return false;
	}

	bool CommandTransitionNewsItemHasExtraData()
	{
		return CNetworkTransitionNewsController::Get().CurrentNewsItemHasExtraData();
	}

	bool CommandTransitionNewsItemGetExtraDataInt(const char* name, int& value)
	{
		return CNetworkTransitionNewsController::Get().GetCurrentNewsItemExtraData(name, value);
	}

	bool CommandTransitionNewsItemGetExtraDataFloat(const char* name, float& value)
	{
		return CNetworkTransitionNewsController::Get().GetCurrentNewsItemExtraData(name, value);
	}

	bool CommandTransitionNewsItemGetExtraDataString(const char* name, scrTextLabel63* value)
	{
		return CNetworkTransitionNewsController::Get().GetCurrentNewsItemExtraData(name, *value);
	}

	void CommandEndTransitionNews()
	{
		CNetworkTransitionNewsController::Get().Reset();
	}

	//////////////////////////////////////////////////////////////////////////
	//
	// Social Club Pause Menu News
	//
	//////////////////////////////////////////////////////////////////////////
	bool CommandInitStarterPackPauseNewsControl( bool isOwned )
	{
		return CNetworkPauseMenuNewsController::Get().InitControl( isOwned ? NEWS_TYPE_STARTERPACK_OWNED_HASH : NEWS_TYPE_STARTERPACK_NOTOWNED_HASH );
	}

	// Text only is exposed but currently ignored.
	bool CommandInitStoryTypePauseNewsControl(int key, bool /*textOnly*/)
	{
		return CNetworkPauseMenuNewsController::Get().InitControl(static_cast<u32>(key));
	}

	int CommandGetNumPauseNewsStories()
	{
		int iNumStories = CNetworkPauseMenuNewsController::Get().GetNumStories();
		return iNumStories;
	}

	int CommandGetPendingPauseNewsStoryIndex()
	{
		int iPendingStoryIndex = CNetworkPauseMenuNewsController::Get().GetPendingStoryIndex();
		return iPendingStoryIndex;
	}

	bool CommandGetPendingPauseNewsStory(sNewsStoryData* newsData)
	{
		bool success = false;

		CNetworkPauseMenuNewsController& newsController = CNetworkPauseMenuNewsController::Get();
		if(newsController.IsPendingStoryReady())
		{
			CNetworkSCNewsStoryRequest* newsStory = newsController.GetPendingStory();
			newsData->m_title.String		= newsStory->GetTitle();
			newsData->m_subtitle.String		= newsStory->GetSubtitle();
			newsData->m_content.String		= newsStory->GetContent();
			newsData->m_url.String			= newsStory->GetURL();
			newsData->m_headline.String		= newsStory->GetHeadline();
			newsData->m_textureName.String	= newsStory->GetTxdName();

			newsStory->MarkAsShown();

			success = true;
		}

		return success;
	}

	bool CommandCycleNewsStory(bool cycleForward)
	{
		return CNetworkPauseMenuNewsController::Get().RequestStory(cycleForward ? 1 : -1);
	}

	void CommandShutdownPauseNewsControl()
	{
		CNetworkPauseMenuNewsController::Get().Reset();
	}

	//////////////////////////////////////////////////////////////////////////
	//
	// Social Club Account info
	//
	//////////////////////////////////////////////////////////////////////////

	const char* CommandAccountInfoGetNickname()
	{
		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s: SC_ACCOUNT_INFO_GET_NICKNAME - Local player is not online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsLocalPlayerOnline() && NetworkInterface::HasValidRosCredentials())
		{
			const rlRosCredentials& cred = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());
			scriptAssertf(cred.IsValid(), "%s: SC_ACCOUNT_INFO_GET_NICKNAME - Local player does not have valid credentials.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			if (cred.IsValid())
			{
				const rlScAccountInfo& accinfo = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex()).GetRockstarAccount();
				return accinfo.m_Nickname;
			}
		}

		return "UNKNOWN";
	}


	//////////////////////////////////////////////////////////////////////////
	//
	// Social Club Achievement info
	//
	//////////////////////////////////////////////////////////////////////////
	
	bool  CommandSocialClubAchievementInfoStatus(int& status)
	{
		status = -1;

#if RSG_PC
		AchMgr& achMgr = CLiveManager::GetAchMgr();
		if (achMgr.HasPendingOperations())
		{
			scriptDebugf1("%s : SC_ACHIEVEMENT_INFO_STATUS - Has Pending operations. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter());
			status = 1;
			return false;
		}
		else if (achMgr.NeedToRefreshAchievements())
		{
			scriptDebugf1("%s : SC_ACHIEVEMENT_INFO_STATUS - Needs to refresh achievements. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter());
			status = 1;
			return false;
		}
		else
		{
			scriptDebugf1("%s : SC_ACHIEVEMENT_INFO_STATUS - Social Club Achievements are avaialable. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter());
			status = 0;
			return true;
		}
#else
		const AchSocialClubReader& achmgr = CLiveManager::GetAchMgr().GetSocialClubAchs();

		if (achmgr.m_status.Pending())
		{
			scriptDebugf1("%s : SC_ACHIEVEMENT_INFO_STATUS - Social Club Achievements reader is pending. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter());
			status = 1;
		}
		else if (achmgr.m_status.Failed())
		{
			scriptErrorf("%s : SC_ACHIEVEMENT_INFO_STATUS - Social Club Achievements reader failed. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter());
			status = 2;
		}
		else if (achmgr.m_status.Canceled())
		{
			scriptErrorf("%s : SC_ACHIEVEMENT_INFO_STATUS - Social Club Achievements reader canceled. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter());
			status = 3;
		}
		else if (achmgr.m_status.Succeeded())
		{
			scriptDebugf1("%s : SC_ACHIEVEMENT_INFO_STATUS - Social Club Achievements are avaialable. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter());
			status = 0;
		}

		return (achmgr.m_status.Succeeded());
#endif
	}

	bool CommandSocialClubAchievementSynchronize( )
	{
#if RSG_PC
		// On PC, all achievements come from the social club and are synced prior to sign in
		scriptDebugf1("%s : SC_ACHIEVEMENT_SYNCHRONIZE - Social Club Achievements Synchronize START. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return true;
#else
		const AchMgr& achmgr = CLiveManager::GetAchMgr();

		if (rlScAchievements::Synchronize(NetworkInterface::GetLocalGamerIndex()
											,achmgr.GetAchievementsInfo()
											,achmgr.GetNumberOfAchievements()
											,NULL)) //No status; it's fire-and-forget
		{
			scriptDebugf1("%s : SC_ACHIEVEMENT_SYNCHRONIZE - Social Club Achievements Synchronize START. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return true;
		}

		scriptErrorf("%s : SC_ACHIEVEMENT_SYNCHRONIZE - Social Club Achievements Synchronize call FAILED. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter());

		return false;
#endif
	}

	bool CommandSocialClubHasAchievementBeenPassed(int id)
	{
#if RSG_PC
		// On PC, all achievements come from the social club.
		AchMgr& achMgr = CLiveManager::GetAchMgr();
		if (achMgr.HasAchievementBeenPassed(id))
		{
			scriptDebugf1("%s : SC_HAS_ACHIEVEMENT_BEEN_PASSED - Achievement='%d', PASSED. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter(), id);
			return true;
		}
		else
		{
			scriptDebugf1("%s : SC_HAS_ACHIEVEMENT_BEEN_PASSED - Achievement='%d', NOT PASSED. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter(), id);
			return false;
		}
#else
		const AchSocialClubReader& achmgr = CLiveManager::GetAchMgr().GetSocialClubAchs();
		if (achmgr.m_status.Succeeded())
		{
			for(u32 i=0; i<MAX_ACHIEVEMENTS; i++)
			{
				const rlScAchievementInfo& a = achmgr.m_achsInfo[i];

				if (id == a.m_Id)
				{
					scriptDebugf1("%s : SC_HAS_ACHIEVEMENT_BEEN_PASSED - Achievement='%d', PASSED. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter(), id);
					return true;
				}
			}

			scriptDebugf1("%s : SC_HAS_ACHIEVEMENT_BEEN_PASSED - Achievement='%d', NOT PASSED. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter(), id);
			return false;
		}

		scriptErrorf("%s : SC_HAS_ACHIEVEMENT_BEEN_PASSED - Social Club Achievements operatin did not SUCCEEDED. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
#endif
	}

	void SetupScriptCommands()
	{
		//////////////////////////////////////////////////////////////////////////
		// Registration function
		SCR_REGISTER_SECURE(SC_INBOX_GET_TOTAL_NUM_MESSAGES,0x760892c3297ba019, CommandInboxGetMessageCount);
		SCR_REGISTER_UNUSED(SC_INBOX_GET_NUM_NEW_MESSAGES,0x9b98ce8028ae2ec7, CommandInboxGetNewMessageCount);
		SCR_REGISTER_SECURE(SC_INBOX_GET_MESSAGE_TYPE_AT_INDEX,0x905892ad8906fdda, CommandInboxGetMessageTypeHashAtIndex);
		SCR_REGISTER_SECURE(SC_INBOX_GET_MESSAGE_IS_READ_AT_INDEX,0x3f17eb622eb3d931, CommandInboxGetIsReadAtIndex);
		SCR_REGISTER_SECURE(SC_INBOX_SET_MESSAGE_AS_READ_AT_INDEX,0x4d50e76db46856be, CommandInboxSetAsReadAtIndex);
		SCR_REGISTER_UNUSED(SC_INBOX_GET_MESSAGE_IS_VALID_AT_INDEX,0x89d49e095fd8336c, CommandInboxIsMessageValid);

		SCR_REGISTER_SECURE(SC_INBOX_MESSAGE_GET_DATA_INT,0x53e079a99d1d5d10, CommandInboxGetDataInt);
		SCR_REGISTER_UNUSED(SC_INBOX_MESSAGE_GET_DATA_FLOAT,0xe0e10f370ddf4eeb, CommandInboxGetDataFloat);
		SCR_REGISTER_SECURE(SC_INBOX_MESSAGE_GET_DATA_BOOL,0x2ac15d104b353af3, CommandInboxGetDataBool);
		SCR_REGISTER_SECURE(SC_INBOX_MESSAGE_GET_DATA_STRING,0x6ddf5a0c0be5196f, CommandInboxGetDataString);

		SCR_REGISTER_SECURE(SC_INBOX_MESSAGE_DO_APPLY,0xd6e798eb9f6b8156, CommandInboxDoApplyOnEvent);

		DLC_SCR_REGISTER_SECURE(SC_INBOX_MESSAGE_GET_RAW_TYPE_AT_INDEX,0xcc8ccc7d32770f5b, CommandInboxGetRAWMessageTypeAtIndex);
		SCR_REGISTER_UNUSED(SC_INBOX_CAN_PUSH_GAMER_TO_RECIP_LIST, 0xd77319f56db0ebb8, CommandInboxCanAddGamerToRecipientList);
		SCR_REGISTER_SECURE(SC_INBOX_MESSAGE_PUSH_GAMER_T0_RECIP_LIST, 0x52ebd349a5063d3f, CommandInboxPushGamerToRecipientList);

		SCR_REGISTER_SECURE(SC_INBOX_SEND_UGCSTATUPDATE_TO_RECIP_LIST,0x206bb1e46df7a9e6, CommandInboxSendUGCUpdateToRecipientListFull);

		SCR_REGISTER_SECURE(SC_INBOX_MESSAGE_GET_UGCDATA,0x0110e2cc5c76c791, CommandInboxGetUGCUpdateData);

		SCR_REGISTER_UNUSED(SC_INBOX_SEND_AWARD_MSG,0x4f37752cba432541, CommandInboxSendAwardMsg);

		SCR_REGISTER_SECURE(SC_INBOX_SEND_BOUNTY_TO_RECIP_LIST,0x617ddc361a45b42b, CommandInboxSendBountyMsgToRecipientList);
		SCR_REGISTER_SECURE(SC_INBOX_GET_BOUNTY_DATA_AT_INDEX,0x86251d087efda1b1, CommandInboxGetBountyDataAtIndex);

		//////////////////////////////////////////////////////////////////////////
		//
		//	SC Email
		//
		//////////////////////////////////////////////////////////////////////////

		SCR_REGISTER_SECURE(SC_EMAIL_RETRIEVE_EMAILS,0x2ba03554ad79aa4c, CommandEmailRetrieveEmails);
		SCR_REGISTER_SECURE(SC_EMAIL_GET_RETRIEVAL_STATUS,0x3eaddb16d5bd2cba, CommandEmailGetRetrievalStatus);
		SCR_REGISTER_SECURE(SC_EMAIL_GET_NUM_RETRIEVED_EMAILS,0x36da2f4516577796, CommandEmailGetNumRetrievedEmails);
		SCR_REGISTER_SECURE(SC_EMAIL_GET_EMAIL_AT_INDEX,0xa44368a741c80fb8, CommandEmailGetEmailAtIndex);
		SCR_REGISTER_SECURE(SC_EMAIL_DELETE_EMAILS,0x5518d197833303c8, CommandEmailDeleteEmails);
		SCR_REGISTER_SECURE(SC_EMAIL_MESSAGE_PUSH_GAMER_TO_RECIP_LIST,0xf21a2f0928b0611e, CommandEmailPushGamerToRecipientList);
		SCR_REGISTER_SECURE(SC_EMAIL_MESSAGE_CLEAR_RECIP_LIST,0xc226569e8ae3b12f, CommandEmailClearRecipientList);
		SCR_REGISTER_SECURE(SC_EMAIL_SEND_EMAIL,0x22c4d9470feca02f, CommandEmailSendEmail);
		SCR_REGISTER_SECURE(SC_EMAIL_SET_CURRENT_EMAIL_TAG,0x447cb94777f3ebb0, CommandEmailSetCurrentEmailTag);

		//////////////////////////////////////////////////////////////////////////
		//
		//	SC Rockstar Message Presence functions
		//
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(SC_CACHE_NEW_ROCKSTAR_MSGS,0xa368680985014a94, CommandSCCacheNewRockstarMsg);
		SCR_REGISTER_SECURE(SC_HAS_NEW_ROCKSTAR_MSG,0x6224e1e91811520e, CommandSCHasNewRockstarMsg);
		SCR_REGISTER_SECURE(SC_GET_NEW_ROCKSTAR_MSG,0x435f23fafd94822c, CommandSCGetNewRockstarMsg);

		//////////////////////////////////////////////////////////////////////////
		//
		//	SC Presence related functions
		//
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(SC_PRESENCE_ATTR_SET_INT,0x0b0cea604d2360ab, CommandSCPresenceAttrSetInt);
		DLC_SCR_REGISTER_SECURE(SC_PRESENCE_ATTR_SET_FLOAT,0x39a629d663853569, CommandSCPresenceAttrSetFloat);
		SCR_REGISTER_SECURE(SC_PRESENCE_ATTR_SET_STRING,0x148a0559a9e2d543, CommandSCPresenceAttrSetString);

		SCR_REGISTER_SECURE(SC_PRESENCE_SET_ACTIVITY_RATING,0x795eef0392d65e0a, CommandSCPresenceSetActivityAttribute);

		//////////////////////////////////////////////////////////////////////////
		//
		//	SC Gamer Data
		//
		//////////////////////////////////////////////////////////////////////////
		DLC_SCR_REGISTER_SECURE(SC_GAMERDATA_GET_INT,0xe530979b4add2f80, CommandSCGamerDataGetInt);
		DLC_SCR_REGISTER_SECURE(SC_GAMERDATA_GET_FLOAT,0xf62752900e6ad1fd, CommandSCGamerDataGetFloat);
		DLC_SCR_REGISTER_SECURE(SC_GAMERDATA_GET_BOOL,0x24734f3518b2bbf2, CommandSCGamerDataGetBool);
		DLC_SCR_REGISTER_SECURE(SC_GAMERDATA_GET_STRING,0x83cb3ffb98d67e00, CommandSCGamerDataGetString);
		DLC_SCR_REGISTER_SECURE(SC_GAMERDATA_GET_ACTIVE_XP_BONUS,0xce5978ddb0cb1d76, CommandSCGamerData_GetActiveXPBonus);
		SCR_REGISTER_UNUSED(SC_GAMERDATA_GET_LICENSE_PLATE,0x6f0a62a7df104fba, CommandSCGamerData_GetLicensePlate);
		SCR_REGISTER_UNUSED(SC_GAMERDATA_BANK_CHEATER_OVERRIDE,0xdca255535c360373, CommandSCGamerData_DebugSetCheater);
		SCR_REGISTER_UNUSED(SC_GAMERDATA_BANK_CHEATER_RATING_OVERRIDE,0xc00e786ec57382bd, CommandSCGamerData_DebugSetCheaterRating);
			
		//////////////////////////////////////////////////////////////////////////
		//
		//	Profanity Checking
		//
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(SC_PROFANITY_CHECK_STRING,0xcc5050a709f16570, CommandCheckText);
		SCR_REGISTER_SECURE(SC_PROFANITY_CHECK_STRING_UGC,0xdd48701476147cf4, CommandCheckTextUGC);
		SCR_REGISTER_SECURE(SC_PROFANITY_GET_CHECK_IS_VALID,0xf4e7b156e5a4b520, CommandCheckText_IsValid);
		SCR_REGISTER_SECURE(SC_PROFANITY_GET_CHECK_IS_PENDING,0x82531db473114086, CommandCheckText_IsPending);
		SCR_REGISTER_SECURE(SC_PROFANITY_GET_STRING_PASSED,0x3b16c45d63fdb882,	CommandCheckText_PassedProfanityCheck);
		SCR_REGISTER_SECURE(SC_PROFANITY_GET_STRING_STATUS,0x043879210cb3356d,	CommandCheckText_GetStatusCode);
		SCR_REGISTER_UNUSED(SC_PROFANITY_GET_PROFANE_WORD,0xc025033f28aae59d,	CommandCheckText_GetProfaneWord);

		//////////////////////////////////////////////////////////////////////////
		//
		//	License Plate Checking
		//
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(SC_LICENSEPLATE_CHECK_STRING,0xd1956b5a20ca5580, CommandCheckLicensePlate);
		SCR_REGISTER_SECURE(SC_LICENSEPLATE_GET_CHECK_IS_VALID,0xbe869dfec7b44d4b, CommandCheckLicensePlate_IsValid);
		SCR_REGISTER_SECURE(SC_LICENSEPLATE_GET_CHECK_IS_PENDING,0xa2d53ed0bef60c91, CommandCheckLicensePlate_IsPending);
		SCR_REGISTER_UNUSED(SC_LICENSEPLATE_GET_STRING_PASSED,0x07a006d8de323afb, CommandCheckLicensePlate_PassedValidityCheck);
		SCR_REGISTER_UNUSED(SC_LICENSEPLATE_GET_STRING_STATUS,0xede682d0d32d7fee, CommandCheckLicensePlate_GetStatusCode);
		SCR_REGISTER_SECURE(SC_LICENSEPLATE_GET_COUNT,0x27e3ea5e1a5d8359, CommandCheckLicensePlate_GetCount);
		SCR_REGISTER_SECURE(SC_LICENSEPLATE_GET_PLATE,0x2bc4091f0e7d790f, CommandCheckLicensePlate_GetPlate);
		SCR_REGISTER_SECURE(SC_LICENSEPLATE_GET_PLATE_DATA,0x8964657c740d2d02,	CommandCheckLicensePlate_GetPlateData);
		SCR_REGISTER_UNUSED(SC_LICENSEPLATE_GET_ADD_PLATE,0x8462d6184bfec8f8,	CommandCheckLicensePlate_GetAddData);
		SCR_REGISTER_SECURE(SC_LICENSEPLATE_SET_PLATE_DATA,0x1850a282903b7f14, CommandChangeLicensePlateData);
		SCR_REGISTER_UNUSED(SC_LICENSEPLATE_SET_PLATE_STATUS,0xa347ef64b241baf6, CommandGetChangeLicensePlateDataStatus);
		
		//////////////////////////////////////////////////////////////////////////
		//
		//	License Plate Add
		//
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(SC_LICENSEPLATE_ADD,0x3fedb6c23a0cadc3, CommandAddLicensePlate);
		SCR_REGISTER_SECURE(SC_LICENSEPLATE_GET_ADD_IS_PENDING,0x077d5881cef1e026, CommandAddLicensePlate_IsPending);
		SCR_REGISTER_SECURE(SC_LICENSEPLATE_GET_ADD_STATUS,0xc5a506671fed32c1, CommandAddLicensePlate_GetStatusCode);

		//////////////////////////////////////////////////////////////////////////
		//
		//	License Plate IsValid
		//
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(SC_LICENSEPLATE_ISVALID,0x1b8f82cfbaefcf2a, CommandIsValidLicensePlate);
		SCR_REGISTER_SECURE(SC_LICENSEPLATE_GET_ISVALID_IS_PENDING,0x200bd2b668159774, CommandIsValidLicensePlate_IsPending);
		SCR_REGISTER_SECURE(SC_LICENSEPLATE_GET_ISVALID_STATUS,0xd07a6d6812cf15d5, CommandIsValidLicensePlate_GetStatusCode);

		//////////////////////////////////////////////////////////////////////////
		//
		//  Social Club Community Events
		//
		//////////////////////////////////////////////////////////////////////////
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_IS_ACTIVE,0xd5a3cfd28abc1dcd, CommandSocialClubEvent_IsActive);
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_GET_EVENT_ID,0xe95ef73b1eb447e9, CommandSocialClubEvent_GetEventId);
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_GET_EXTRA_DATA_INT,0xc9497291fe019bdd,	CommandSocialClubEvent_GetExtraDataInt);
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_GET_EXTRA_DATA_FLOAT,0xff6eb2bfa3722d5b, CommandSocialClubEvent_GetExtraDataFloat);
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_GET_EXTRA_DATA_STRING,0x0969b80bd6ef0448, CommandSocialClubEvent_GetExtraDataString);
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_GET_DISPLAY_NAME,0xae88927b2e59627e, CommandSocialClubEvent_GetDisplayName);

		// Same stuff, but for a given scevent type
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_IS_ACTIVE_FOR_TYPE,0xd271bdf216949c05,  CommandSocialClubEvent_IsActiveForType);
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_GET_EVENT_ID_FOR_TYPE,0xacb1492c0ac1508e, CommandSocialClubEvent_GetEventIdForType);
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_GET_EXTRA_DATA_INT_FOR_TYPE,0x1faf6e2303414888,	CommandSocialClubEvent_GetExtraDataIntForType);
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_GET_EXTRA_DATA_FLOAT_FOR_TYPE,0x899d6dd23b6333b7, CommandSocialClubEvent_GetExtraDataFloatForType);
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_GET_EXTRA_DATA_STRING_FOR_TYPE,0x7c3d8ecbf2948404, CommandSocialClubEvent_GetExtraDataStringForType);
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_GET_DISPLAY_NAME_FOR_TYPE,0x715d80baa018340a, CommandSocialClubEvent_GetDisplayNameForType);

		//Same stuff, but now by ID...
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_IS_ACTIVE_BY_ID,0x7c6a47cf803393f4,  CommandSocialClubEvent_IsActiveById);
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_GET_EXTRA_DATA_INT_BY_ID,0xbe0a744f66c123fa,	CommandSocialClubEvent_GetExtraDataIntById);
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_GET_EXTRA_DATA_FLOAT_BY_ID,0xa44df352c7f035f4, CommandSocialClubEvent_GetExtraDataFloatById);
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_GET_EXTRA_DATA_STRING_BY_ID,0x8c7e520683702281, CommandSocialClubEvent_GetExtraDataStringById);
		DLC_SCR_REGISTER_SECURE(SC_COMMUNITY_EVENT_GET_DISPLAY_NAME_BY_ID,0x906a3ddd892efd32, CommandSocialClubEvent_GetDisplayNameById);

		//	SC RePopulate Event handling
		SCR_REGISTER_UNUSED(SC_COMMUNITY_EVENT_REPOPULATE_DO_REQUEST,0x7a096aaeebc56f8e, CommandSocialClubEvent_RepopulateDoRequest);
		SCR_REGISTER_UNUSED(SC_COMMUNITY_EVENT_REPOPULATE_IS_REQUEST_PENDING,0xaa36c94a36a35ced, CCommandSocialClubEvent_RepopulateIsRequestPending);
		SCR_REGISTER_UNUSED(SC_COMMUNITY_EVENT_REPOPULATE_DID_LAST_REQUEST_SUCCEED,0xb8f683b1f203be95, CommandSocialClubEvent_RepopulateDidLastRequestSucceed);

		//////////////////////////////////////////////////////////////////////////
		//
		// Social Club Ticker News
		//
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_UNUSED(SC_NEWS_SHOW_NEXT_ITEM,0xbd9ef1411241346b, CommandShowNextNewsItem);
		SCR_REGISTER_UNUSED(SC_NEWS_SHOW_PREV_ITEM,0x6db23ae3b73c62de,	CommandShowPrevNewsItem);

		//////////////////////////////////////////////////////////////////////////
		//
		// Social Club Transition News
		//
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_UNUSED(SC_TRANSITION_NEWS_QUEUE_STORY,0x5739fbea8e2780cb, CommandQueueTransitionStory);
		SCR_REGISTER_SECURE(SC_TRANSITION_NEWS_SHOW,0x0d2a179a72de9835, CommandShowTransitionNews);
		SCR_REGISTER_SECURE(SC_TRANSITION_NEWS_SHOW_TIMED,0x1d7ff1570191d740, CommandShowTransitionNewsTimed);
		SCR_REGISTER_SECURE(SC_TRANSITION_NEWS_SHOW_NEXT_ITEM,0xd3d69d182eb4f47b, CommandShowNextTransitionNewsItem);
		SCR_REGISTER_UNUSED(SC_TRANSITION_NEWS_SHOW_PREV_ITEM,0xcdab77e920bbd01f, CommandShowPrevTransitionNewsItem);
		SCR_REGISTER_SECURE(SC_TRANSITION_NEWS_HAS_EXTRA_DATA_TU,0x4a80e5be1282b9f6, CommandTransitionNewsItemHasExtraData);
		SCR_REGISTER_SECURE(SC_TRANSITION_NEWS_GET_EXTRA_DATA_INT_TU,0x173f6700006f426f, CommandTransitionNewsItemGetExtraDataInt);
		SCR_REGISTER_UNUSED(SC_TRANSITION_NEWS_GET_EXTRA_DATA_FLOAT_TU,0x83f8b60ec7067eaf, CommandTransitionNewsItemGetExtraDataFloat);
		SCR_REGISTER_UNUSED(SC_TRANSITION_NEWS_GET_EXTRA_DATA_STRING_TU,0xe8f8013da034fd4e, CommandTransitionNewsItemGetExtraDataString);
		SCR_REGISTER_SECURE(SC_TRANSITION_NEWS_END,0x5689e28a3cf63014, CommandEndTransitionNews);

		//////////////////////////////////////////////////////////////////////////
		//
		// Social Club Pause Menu News
		//
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(SC_PAUSE_NEWS_INIT_STARTER_PACK,0x60172adf60baa91e, CommandInitStarterPackPauseNewsControl);
		SCR_REGISTER_UNUSED(SC_PAUSE_NEWS_INIT_STORY_TYPE,0x0571f1c333b15078, CommandInitStoryTypePauseNewsControl);
		SCR_REGISTER_UNUSED(SC_PAUSE_NEWS_GET_NUM_STORIES,0xfef27588e68c17b3, CommandGetNumPauseNewsStories);
		SCR_REGISTER_UNUSED(SC_PAUSE_NEWS_GET_PENDING_STORY_INDEX,0x5a675aaca79f0b23, CommandGetPendingPauseNewsStoryIndex);
		SCR_REGISTER_SECURE(SC_PAUSE_NEWS_GET_PENDING_STORY,0xf349a7e5fb1a1df0, CommandGetPendingPauseNewsStory);
		SCR_REGISTER_UNUSED(SC_PAUSE_NEWS_CYCLE_STORY,0xa61064c58220b56b, CommandCycleNewsStory);
		SCR_REGISTER_SECURE(SC_PAUSE_NEWS_SHUTDOWN,0x45de407092cb2039, CommandShutdownPauseNewsControl);

		//////////////////////////////////////////////////////////////////////////
		//
		// Social Club Account info
		//
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(SC_ACCOUNT_INFO_GET_NICKNAME,0xb2acfc559a277939, CommandAccountInfoGetNickname);

		//////////////////////////////////////////////////////////////////////////
		//
		// Social Club Achievement info
		//
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(SC_ACHIEVEMENT_INFO_STATUS,0x0b032b4a939192d3, CommandSocialClubAchievementInfoStatus);
		SCR_REGISTER_UNUSED(SC_ACHIEVEMENT_SYNCHRONIZE,0x5b726816f402dec9, CommandSocialClubAchievementSynchronize);
		SCR_REGISTER_SECURE(SC_HAS_ACHIEVEMENT_BEEN_PASSED,0xbea131afb70a18a2, CommandSocialClubHasAchievementBeenPassed);
	}

} //namespace socialclub_commands
