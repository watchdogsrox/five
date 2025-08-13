#include "Network/NetworkTypes.h"

#if __NET_SHOP_ACTIVE

#include "GameTransactionsTasks.h"

#include "data/aes.h"
#include "diag/seh.h"
#include "system/param.h"
#include "system/timer.h"

#include "rline/ros/rlros.h"
#include "rline/rltitleid.h"
#include "rline/ros/rlrostitleid.h"

#include "fwnet/netchannel.h"

#include "Network/NetworkInterface.h"
#include "Network/Shop/Catalog.h"
#include "Network/Shop/GameTransactionsSessionMgr.h"
#include "Network/Shop/NetworkGameTransactions.h"
#include "debug/Debug.h"

NETWORK_OPTIMISATIONS();
NETWORK_SHOP_OPTIMISATIONS();

#undef __net_channel
#define __net_channel net_shop_gameTransactions

XPARAM(netshoplatency);
XPARAM(netGameTransactionsNoToken);
PARAM(netGameTransactionsServerApp, "Server app to use");
PARAM(netGameTransactionHostName,"host name to use for connecting to game server");
PARAM(netGameTransactionTitleName,"title name to use for connecting to game server");
PARAM(netGameTransactionTitleVersion,"title version to use for connecting to game server");
#if !__FINAL
XPARAM(catalogVersion);
#endif //!__FINAL

#ifdef __NET_GAMETRANS_FAKE_LATENCY
#include "Network/Shop/NetworkShopping.h"	
#endif // __NET_GAMETRANS_FAKE_LATENCY


#if __BANK

ForceErrorHelper DeleteSlotTransactionHttpTask::sm_errors;

const char* GetGameServerErrorCodeName(const int value)
{
	switch (value)
	{
	case GS_ERROR_NOT_IMPLEMENTED:            return "ERROR_NOT_IMPLEMENTED";
	case GS_ERROR_SYSTEM_ERROR:               return "ERROR_SYSTEM_ERROR";
	case GS_ERROR_INVALID_DATA:               return "ERROR_INVALID_DATA";
	case GS_ERROR_INVALID_PRICE:              return "ERROR_INVALID_PRICE";
	case GS_ERROR_INCORRECT_FUNDS:            return "ERROR_INCORRECT_FUNDS";
	case GS_ERROR_INSUFFICIENT_INVENTORY:     return "ERROR_INSUFFICIENT_INVENTORY";
	case GS_ERROR_INVALID_ITEM:               return "ERROR_INVALID_ITEM";
	case GS_ERROR_INVALID_PERMISSIONS:        return "ERROR_INVALID_PERMISSIONS";
	case GS_ERROR_INVALID_CATALOG_VERSION:    return "ERROR_INVALID_CATALOG_VERSION";
	case GS_ERROR_INVALID_TOKEN:              return "ERROR_INVALID_TOKEN";
	case GS_ERROR_INVALID_REQUEST:            return "ERROR_INVALID_REQUEST";
	case GS_ERROR_INVALID_ACCOUNT:            return "ERROR_INVALID_ACCOUNT";
	case GS_ERROR_INVALID_NONCE:              return "ERROR_INVALID_NONCE";
	case GS_ERROR_EXPIRED_TOKEN:              return "ERROR_EXPIRED_TOKEN";
	case GS_ERROR_CACHE_OPERATION:            return "ERROR_CACHE_OPERATION";
	case GS_ERROR_INVALID_LIMITER_DEFINITION: return "ERROR_INVALID_LIMITER_DEFINITION";
	case GS_ERROR_GAME_TRANSACTION_EXCEPTION: return "ERROR_GAME_TRANSACTION_EXCEPTION";
	case GS_ERROR_EXCEED_LIMIT:               return "ERROR_EXCEED_LIMIT";
	case GS_ERROR_EXCEED_LIMIT_MESSAGE:       return "ERROR_EXCEED_LIMIT_MESSAGE";
	case GS_ERROR_AUTH_FAILED:                return "ERROR_AUTH_FAILED";
	case GS_ERROR_STALE_DATA:                 return "ERROR_STALE_DATA";
	case GS_ERROR_BAD_SIGNATURE:              return "ERROR_BAD_SIGNATURE";
	case GS_ERROR_NET_MAINTENANCE:            return "ERROR_NET_MAINTENANCE";
	case GS_ERROR_USER_FORCE_BAIL:            return "ERROR_USER_FORCE_BAIL";
	case GS_ERROR_CLIENT_FORCE_FAILED:        return "ERROR_CLIENT_FORCE_FAILED";
	case GS_ERROR_INVALID_GAME_VERSION:       return "ERROR_INVALID_GAME_VERSION";
    case GS_ERROR_INVALID_ENTITLEMENT:        return "ERROR_INVALID_ENTITLEMENT";

	default:
		break;
	}

	gnetAssertf(0, "GetGameServerErrorCodeName:: [eGameServerErrorCodes] Invalid error id='%d'", value);

	return "INVALID";
}

bool  ForceErrorHelper::Serialize(RsonWriter& wr) const
{
	for (int i=0; i<GS_ERROR_MAX; i++)
	{
		if (m_bits.IsSet(i))
			return wr.WriteString("forceerror", GetGameServerErrorCodeName(i));
	}

	return true;
}

#endif // __BANK


namespace rage
{
	extern const rlTitleId* g_rlTitleId;
}

const char* GameTransactionBaseHttpTask::GetServiceName() const
{
	return "GameTransactions.asmx";
}

const char* GameTransactionBaseHttpTask::GetServerAppName() const
{
	return "GamePlayServices";
}


const char* GameTransactionBaseHttpTask::ContentType() const
{
	return "Content-Type: application/text\r\n";
}

const char* GameTransactionBaseHttpTask::GetUrlHostName( char* hostnameBuf, const unsigned sizeofBuf ) const
{
	const char* returnHN = safecpy(hostnameBuf,  "", sizeofBuf);

#if !__FINAL
	const char* paramHostName = NULL;
	if (PARAM_netGameTransactionHostName.Get(paramHostName))
	{
        returnHN = safecpy(hostnameBuf, paramHostName, sizeofBuf);
		return paramHostName;
	}

	if(PARAM_netGameTransactionsNoToken.Get())
	{
		//No token means no conductor...fake it.
		returnHN = safecpy(hostnameBuf, "dev.p01ewr.pod.rockstargames.com", sizeofBuf);
		return returnHN;
	}

#endif

#if RLROS_USE_SERVICE_ACCESS_TOKENS
	//We need to make sure we have a valid token
	const rlRosServiceAccessInfo& tokenInfo = rlRos::GetServiceAccessInfo(m_localGamerIndex);
	if (gnetVerify(tokenInfo.IsValid()))
	{
		returnHN = safecpy(hostnameBuf,  tokenInfo.GetServerUrl(), sizeofBuf);
		return returnHN;
	}
#elif !__FINAL
	rlTaskError("Attempting to use RS Access tokens on unsupported platform.  Add -netGameTransactionsNoToken");
#endif

	return nullptr;
}

bool GameTransactionBaseHttpTask::GetServicePath( char* svcPath, const unsigned maxLen ) const
{
	const char* serverAppName = GetServerAppName();

#if !__FINAL
	const char* paramTitleName = NULL;
	int paramTitleVersion = 0;

	if (PARAM_netGameTransactionTitleName.Get(paramTitleName) && PARAM_netGameTransactionTitleVersion.Get(paramTitleVersion))
	{
		formatf(svcPath, maxLen, "%s/%d/%s/%s/%s",
									paramTitleName,
									paramTitleVersion,
									serverAppName,
									GetServiceName(),
									GetServiceEndpoint());
	}
	else
#endif // !__FINAL
	{
		formatf(svcPath, maxLen, "%s/%d/%s/%s/%s",
		g_rlTitleId->m_RosTitleId.GetTitleName(),
		g_rlTitleId->m_RosTitleId.GetTitleVersion(),
		serverAppName,
		GetServiceName(),
											GetServiceEndpoint());
	}

	return true;
}

#if !__NO_OUTPUT
const PlayerAccountId GetAccountIdHelper(const int localGamerIndex)
{
	const rlRosCredentials& cred = rlRos::GetCredentials( localGamerIndex );
	if(gnetVerify(cred.IsValid()))
	{
		return cred.GetPlayerAccountId();
	}

	return InvalidPlayerAccountId;
}

#endif

bool GameTransactionBaseHttpTask::Configure(const int localGamerIndex, const u32 challengeResponseInfo, RsonWriter& wr)
{
	rtry
	{

#if __NET_GAMETRANS_FAKE_LATENCY
		PARAM_netshoplatency.Get(m_FakeLatency);
		if( NETWORK_SHOPPING_MGR.Bank_GetOverrideLatency( ) ) 
			m_FakeLatency = NETWORK_SHOPPING_MGR.Bank_GetLatency(); 
#endif // __NET_GAMETRANS_FAKE_LATENCY

		rverify(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex),
			catchall,
			rlTaskError("Illegal local gamer index: %d", localGamerIndex));

		m_localGamerIndex = localGamerIndex;
		m_sessionCri      = challengeResponseInfo;

		//Finalize RsonWriter - Add base class data.
		if(HasTransactionData())
		{
			rverify(wr.ToString()
				,catchall
				,rlTaskError("NULL buffer for transaction Data."));

			Serialize(wr);

			rcheck(wr.End()
				,catchall
				,rlTaskError("GameTransactionBaseHttpTask RsonWriter::End - %s", wr.ToString()));
		}

        //Always use the Ros Ssl context for ros http tasks
        m_HttpRequest.SetSslContext(rlRos::GetRosSslContext());

		rlHttpTaskConfig finalConfig;
		finalConfig.m_AddDefaultContentTypeHeader = false;
		finalConfig.m_HttpHeaders = ContentType();
		finalConfig.m_HttpVerb = GetHttpVerb();

		rverify(rlHttpTask::Configure(&finalConfig),
			catchall,
			rlTaskError("Failed to configure base class"));

		rverify(ApplyServiceTokenHeader(localGamerIndex)
			,catchall
			,rlTaskError("Failed ApplyServiceTokenHeader()"));

		if(wr.ToString())
		{
			rverify(m_HttpRequest.AppendContent(wr.ToString(), wr.Length()),
				catchall,
				rlTaskError("Failed to Append data for event."));
		}

		rlTaskDebug1("Sending '%s' (AcctId '%d') : '%s.'", GetServiceEndpoint(), GetAccountIdHelper(localGamerIndex), wr.ToString());

		return true;
	}
	rcatchall
	{

	}

	return false;
}

bool GameTransactionBaseHttpTask::Serialize(RsonWriter& wr) const
{
	bool retVal = true;

	//Game Version
	float version = 0.0f;
	sscanf(CDebug::GetVersionNumber(), "%f", &version);
	retVal &= wr.WriteFloat("GameVersion", version);

	//if (UseChallengeResponseInfo() && 0 < m_sessionCri)
	//{
	//	retVal &= wr.WriteUns("cri", m_sessionCri);
	//}

	return retVal;
}

bool GameTransactionBaseHttpTask::ProcessResponseCatalogInfo(const RsonReader& responseReader)
{
#if !__FINAL
	//Ignore 'CurrentCatalogVersion' when we want to override it.
	int catalogVersion = 0;
	if(!PARAM_catalogVersion.Get(catalogVersion))
		catalogVersion = 0;
	
	if(catalogVersion > 0)
		return true;
#endif //!__FINAL

	u32 currentCatalogCRC = 0;
	u32 latestCatalogVersion = 0;
	if(responseReader.ReadUns("CurrentCatalogVersion", latestCatalogVersion) && responseReader.ReadUns("CatalogCRC", currentCatalogCRC))
	{
		if (latestCatalogVersion > 0 && !GameTransactionSessionMgr::Get().IsInvalid())
		{
			netCatalog::GetInstance().SetLatestVersion(latestCatalogVersion, currentCatalogCRC);
		}
	}

	return true;
}

bool GameTransactionBaseHttpTask::ProcessChallengeResponseInfo(const RsonReader& UNUSED_PARAM(rr))
{
/*
	rtry
	{
		//Now we can check for the Challenge-Response of the Game Server.
		if (UseChallengeResponseInfo() && 0 < m_sessionCri)
		{
			u32 cri = 0;

			rcheck(rr.ReadUns("cri", cri)
				,catchall
				,rlTaskError("Failed to read 'cri'."));

			AES aes(AES_KEY_ID_SAVEGAME);
			rcheck(aes.Decrypt(&cri, sizeof(u32))
				,catchall
				,rlTaskError("aes.Encrypt() - FAILED."));

			rcheck(memcmp(&cri, &m_sessionCri, sizeof(u32)) == 0
				,catchall
				,rlTaskError("Failed to authenticate the Game Server."));
		}

		return true;
	}
	rcatchall
	{
		//gnetDebug1("GameTransactionBaseHttpTask::ProcessChallengeResponseInfo.");
		//GameTransactionSessionMgr::Get().Bail(GameTransactionSessionMgr::FAILED_SESSION_RESTART, BAIL_GAME_SERVER_FORCE_BAIL);
	}
*/

	return true;
}

bool GameTransactionBaseHttpTask::ProcessGameVersionInfo(const RsonReader& UNUSED_PARAM(rr))
{
	bool result = true;

/*
	//rtry
	{
		float gameVersion = 0.0f;

		rr.ReadFloat("GameVersion", gameVersion);
		//rcheck(rr.ReadFloat("ValidGameVersion", validGameVersion)
		//	,catchall
		//	,rlTaskError("Failed to read 'ValidGameVersion'."));

		float serverversion = 0.0f;
		sscanf(CDebug::GetVersionNumber(), "%f", &serverversion);

		if (gameVersion > serverversion)
		{
			gnetDebug1("GameTransactionBaseHttpTask::ProcessGameVersionInfo.");
			GameTransactionSessionMgr::Get().Bail(GameTransactionSessionMgr::FAILED_GAME_VERSION, BAIL_GAME_SERVER_GAME_VERSION);
		}
	}
	//rcatchall
	//{
	//	result = false;
	//}
*/

	return result;
}

bool GameTransactionBaseHttpTask::ProcessResponse( const char* response, int& resultCode )
{
	bool success = false;
	resultCode = -1;

	rtry
	{
		rlTaskDebug("GameTransactionBaseHttpTask::ProcessResponse:: %s", response);

		RsonReader responseReader;

		//We need the length of the response, which is contained in the grow buffer.
		unsigned int responseLength = m_GrowBuffer.Length();
		responseReader.Init(response, responseLength);

		//Because we love IIS, check if everything is nested in the magic 'd' object
		responseReader.GetMember("d", &responseReader);

		int status = 0;
		rcheck(responseReader.ReadInt("Status", status)
			,catchall
			,rlTaskError("No 'Status' element"));

		rcheck(ProcessResponseCatalogInfo(responseReader)
			,catchall
			,rlTaskError("Failed to process ProcessResponseCatalogInfo."));

		rcheck(ProcessChallengeResponseInfo(responseReader)
			,catchall
			,rlTaskError("Failed to process ProcessChallengeResponseInfo."));

		rcheck(ProcessGameVersionInfo(responseReader)
			,catchall
			,rlTaskError("Failed to process ProcessGameVersionInfo."));

		if (status == 1)
		{
			if (!GameTransactionSessionMgr::Get().IsInvalid())
			{
				success = ProcessSuccess(responseReader);
			}
			else
			{
				success = true;
			}

			if (success)
			{
				resultCode = 0;
			}
			else
			{
				//We received a response, but we failed processing it
				rlTaskError("Failed to process successful response");
				resultCode = -1;
			}
		}
		else
		{
			//Our error hash
			int responseCodeHash = 0;

			RsonReader responseCodeRR;
			rverify(responseReader.GetMember("ResponseCode", &responseCodeRR)
				,catchall
				,rlTaskError("No 'ResponseCode' element"));

			rverify(responseCodeRR.AsInt(responseCodeHash)
				,catchall
				,rlTaskError("Failed to retrieve 'ResponseCode' AsInt."));

			atHashString codestringhash(responseCodeHash);
			resultCode = GetTransactionEventErrorCode(codestringhash);

#if !__FINAL
			atString responseCodeString;
			responseReader.ReadString("ResponseCodeStr", responseCodeString);
			rlTaskDebug1("Error: '%s', Code:%d", responseCodeString.c_str(), resultCode);
#endif // !__FINAL

			rverify(0 != responseCodeHash
				,catchall
				,rlTaskError("Invalid 'ResponseCode' '%d'.", responseCodeHash));

			//If we can act on the ERROR lets do that.
			if (GameTransactionSessionMgr::Get().HasInitSucceeded())
			{
				//If the error was an invalid nonce, trigger that we need to do a refresh
				if (   responseCodeHash == (int)ATSTRINGHASH("ERROR_INVALID_NONCE", 0x1cd73f03)
					|| responseCodeHash == (int)ATSTRINGHASH("ERROR_EXPIRED_TOKEN", 0x3ef00d26)
					|| responseCodeHash == (int)ATSTRINGHASH("ERROR_CACHE_OPERATION", 0x428b9a9a)
					|| responseCodeHash == (int)ATSTRINGHASH("ERROR_INVALID_TOKEN", 0x7685f402)   )
				{
					if (CanFlagForSessionRestart())
					{
						GameTransactionSessionMgr::Get().FlagForSessionRestart();
					}
				}
				//If the error was an invalid catalog, trigger that we need to do a refresh
				else if (responseCodeHash == (int)ATSTRINGHASH("ERROR_INVALID_CATALOG_VERSION", 0xce55b13c))
				{
					if (CanTriggerCatalogRefresh())
					{
						GameTransactionSessionMgr::Get().TriggerCatalogRefresh();
					}
				}
			}

			//Server maintenance
			if (responseCodeHash == (int)ATSTRINGHASH("ERROR_NET_MAINTENANCE", 0xae309aa7))
			{
				gnetDebug1("GameTransactionBaseHttpTask::ProcessResponse - ERROR_NET_MAINTENANCE.");
				GameTransactionSessionMgr::Get().Bail(GameTransactionSessionMgr::FAILED_ACCESS_TOKEN, BAIL_GAME_SERVER_MAINTENANCE, BAIL_CTX_NONE);
			}
			//Kick this guy in the butt hole
			else if (responseCodeHash == (int)ATSTRINGHASH("ERROR_USER_FORCE_BAIL", 0x7344d255))
			{
				gnetDebug1("GameTransactionBaseHttpTask::ProcessResponse - ERROR_USER_FORCE_BAIL.");
				GameTransactionSessionMgr::Get().Bail(GameTransactionSessionMgr::FAILED_ACCESS_TOKEN, BAIL_GAME_SERVER_FORCE_BAIL, BAIL_CTX_NONE);
			}
			//Kick this guy due to running a invalid game version
			else if (responseCodeHash == (int)ATSTRINGHASH("ERROR_INVALID_GAME_VERSION", 0x15c1301e))
			{
				gnetDebug1("GameTransactionBaseHttpTask::ProcessResponse - ERROR_INVALID_GAME_VERSION.");
				GameTransactionSessionMgr::Get().Bail(GameTransactionSessionMgr::FAILED_GAME_VERSION, BAIL_GAME_SERVER_GAME_VERSION, BAIL_CTX_NONE);
			}

			ProcessError(responseReader);

			success = false;
		}

	}
	rcatchall
	{
	}

	return success;
}

bool GameTransactionBaseHttpTask::ProcessError( const RsonReader& OUTPUT_ONLY(rr) )
{
#if !__NO_OUTPUT
	RsonReader errorReader;
	if(rr.ReadReader("Error", errorReader))
	{
		atString errorCode("UNKNOWN");
		errorReader.ReadString("ResponseCode", errorCode);

		char errorString[2048];
		errorString[0] = '\0';
		if (errorReader.ReadString("Msg", errorString))
		{
			rlTaskDebug1("[%s] Msg:%s", errorCode.c_str(), errorString);
		}

		atString errorLoc;
		if (errorReader.ReadString("Location", errorLoc))
		{
			errorLoc.Replace("\\u000d\\u000a", "\n");
			rlTaskDebug1("Location:\n%s", errorLoc.c_str());
		}
	}
#endif

	return true;
}

void GameTransactionBaseHttpTask::OnFinish(const int resultCode)
{
	//Transaction Failed so we need to RollbackNounce - when we cant reach the server.
	if (resultCode >= 400 && resultCode < 600 && HasNonce())
	{
		GameTransactionSessionMgr::Get().RollbackNounce();
	}
}

int GameTransactionBaseHttpTask::GetTransactionEventErrorCode(const atHashString& codestringhash)
{
	static const int errorCodes[] = {
		 ATSTRINGHASH("ERROR_SUCCESS", 0xae0f460b)                               /* -1374730741 */
		,ATSTRINGHASH("ERROR_UNDEFINED", 0x17a4780e)                             /* 396654606   */ 
		,ATSTRINGHASH("ERROR_NOT_IMPLEMENTED", 0x05ee3c76)                       /* 99499126    */
		,ATSTRINGHASH("ERROR_SYSTEM_ERROR", 0x0de7c974)                          /* 233294196   */
		,ATSTRINGHASH("ERROR_INVALID_DATA", 0x056df61f)                          /* 91092511    */
		,ATSTRINGHASH("ERROR_INVALID_PRICE", 0x7e3b392e)                         /* 2117810478  */
		,ATSTRINGHASH("ERROR_INSUFFICIENT_FUNDS", 0xf695f0a4)                    /* -157945692  */
		,ATSTRINGHASH("ERROR_INSUFFICIENT_INVENTORY", 0xab0603bc)                /* 2869298108  */
		,ATSTRINGHASH("ERROR_INCORRECT_FUNDS", 0xde9919a6)                       /* -560391770	*/
		,ATSTRINGHASH("ERROR_INVALID_ITEM", 0x1ae73071)                          /* 451358833	*/
		,ATSTRINGHASH("ERROR_INVALID_PERMISSIONS", 0xaaf1edc5)                   /* -1426985531	*/
		,ATSTRINGHASH("ERROR_INVALID_CATALOG_VERSION", 0xce55b13c)               /* -833244868	*/
		,ATSTRINGHASH("ERROR_INVALID_TOKEN", 0x7685f402)                         /* 1988490242	*/
		,ATSTRINGHASH("ERROR_INVALID_REQUEST", 0x096c98b0)                       /* 158111920	*/
		,ATSTRINGHASH("ERROR_INVALID_ACCOUNT", 0xeced9e87)                       /* -319971705  */
		,ATSTRINGHASH("ERROR_INVALID_NONCE", 0x1cd73f03)                         /* 483868419	*/
		,ATSTRINGHASH("ERROR_EXPIRED_TOKEN", 0x3ef00d26)                         /* 1055919398	*/
		,ATSTRINGHASH("ERROR_CACHE_OPERATION", 0x428b9a9a)                       /* 1116445338	*/
		,ATSTRINGHASH("ERROR_INVALID_LIMITER_DEFINITION", 0x3fc077f2)            /* 1069578226	*/
		,ATSTRINGHASH("ERROR_GAME_TRANSACTION_EXCEPTION", 0x27e0c1cf)            /* 669041103	*/
		,ATSTRINGHASH("ERROR_EXCEED_LIMIT", 0x265488c1)                          /* 643074241	*/
		,ATSTRINGHASH("ERROR_EXCEED_LIMIT_MESSAGE", 0x2b92cd38)                  /* 731041080	*/
		,ATSTRINGHASH("ERROR_AUTH_FAILED", 0xf1011922)                           /* -251586270  */
		,ATSTRINGHASH("ERROR_STALE_DATA", 0x02847290)                            /* 42234512	*/
		,ATSTRINGHASH("ERROR_BAD_SIGNATURE", 0x1064b68e)                         /* 275035790	*/
		,ATSTRINGHASH("ERROR_INVALID_CONFIGURATION", 0xe8f6a62b)                 /* -386488789	*/
		,ATSTRINGHASH("ERROR_NET_MAINTENANCE", 0xae309aa7)                       /* -1372546393	*/
		,ATSTRINGHASH("ERROR_USER_FORCE_BAIL", 0x7344d255)                       /* 1933890133	*/
		,ATSTRINGHASH("ERROR_CLIENT_FORCE_FAILED", 0xdd64b5fe)                   /* -580602370	*/
		,ATSTRINGHASH("ERROR_INVALID_GAME_VERSION", 0x15c1301e)                  /*  364982302	*/
        ,ATSTRINGHASH("ERROR_INVALID_ENTITLEMENT", 0xD3F91E36)                   /* -738648522  */
	};

	for (int i = 0; i < COUNTOF(errorCodes); i++)
	{
		if (errorCodes[i] == (int)codestringhash.GetHash())
		{
			return i;
		}
	}

	gnetAssertf(0, "Unspecified Error code: '%s,%d'", codestringhash.TryGetCStr(), (int)codestringhash.GetHash());
	gnetError("Unspecified Error code: '%s,%d'", codestringhash.TryGetCStr(), (int)codestringhash.GetHash());

	return -1;
}

#if __NET_GAMETRANS_FAKE_LATENCY
void GameTransactionBaseHttpTask::Finish( const FinishType finishType, const int resultCode /*= 0*/ )
{
	//If we're doing fake latency and we have some left, push the results to finish
	if (m_FakeLatency > 0)
	{
		gnetDebug1("Delaying call to finish due to fake latentency");
		m_bFinishedRequested = true;
		m_FinishTypeRequested = finishType;
		m_FinishResultCode = resultCode;
	}
	else
	{
		rlHttpTask::Finish(finishType, resultCode);
		OnFinish(resultCode);
	}
}

void GameTransactionBaseHttpTask::Update( const unsigned timeStep )
{
	//If we're updating fake latency, to the update.
	if (m_FakeLatency > 0)
	{
		m_FakeLatency -= timeStep;
	}

	//If finish was already called, start waiting for the latency to run out
	if (m_bFinishedRequested)
	{
		if (m_FakeLatency > 0)
		{
			gnetDebug1("Waiting for Fake Latency to expire before succeeding");
		}
		else
		{
			gnetDebug1("Fake Latency expired.  Calling Finish()");
			Finish(m_FinishTypeRequested, m_FinishResultCode);
		}
	}
	else //Update like normal.
	{
		rlHttpTask::Update(timeStep);
	}
}
#else

void GameTransactionBaseHttpTask::Finish( const FinishType finishType, const int resultCode /*= 0*/ )
{
	rlHttpTask::Finish(finishType, resultCode);
	OnFinish(resultCode);
}

#endif //__NET_GAMETRANS_FAKE_LATENCY

bool GameTransactionBaseHttpTask::ApplyServiceTokenHeader(const int localGamerIndex)
{
#if RLROS_USE_SERVICE_ACCESS_TOKENS
	rtry
	{
		if (!PARAM_netGameTransactionsNoToken.Get())
		{
			atString authToken;
			if(UsesGSToken())
			{
				authToken = GetGSToken();
			}
			else
			{
				authToken = GetRSAccessToken(localGamerIndex);
			}

			rcheck(authToken.length() > 0,catchall,rlTaskError("Invalid gsToken"));

			rverify(m_HttpRequest.AddRequestHeaderValue("Authorization", authToken.c_str(), authToken.length() )
					,catchall
					,rlTaskError("Failed to add AddRequestHeaderValue() "));
		}

		return true;
	}
	rcatchall
	{

	}
#endif

	return false;
}

atString GameTransactionBaseHttpTask::GetGSToken()
{
	const GSToken& gsToken = GameTransactionSessionMgr::Get().GetGsToken();
	if(gsToken.IsValid())
	{
		atString authToken("GSTOKEN token=");
		authToken += gsToken;

		return authToken;
	}

	return atString();
}

atString GameTransactionBaseHttpTask::GetRSAccessToken(const int RLROS_USE_SERVICE_ACCESS_TOKENS_ONLY(localGamerIndex))
{
#if RLROS_USE_SERVICE_ACCESS_TOKENS
	const rlRosServiceAccessInfo& serviceAccessInfo = rlRos::GetServiceAccessInfo(localGamerIndex);
	if(serviceAccessInfo.IsValid())
	{
		atString authToken("RSACCESS token=");
		authToken += serviceAccessInfo.GetAccessToken();

		return authToken;
	}
#endif

	return atString();
}

bool GameTransactionHttpTask::Configure(const int localGamerIndex
										,const u32 challengeResponseInfo
										,RsonWriter& wr
										,NetworkGameTransactions::TelemetryNonce* outTelemetryNonce)
{
	rtry
	{
		//Call the 'simple' configure
		rverify(Base::Configure(localGamerIndex, challengeResponseInfo, wr)
			,catchall
			,rlTaskError("Failed to configure 'GameTransactionHttpTask'."));

		rverify(outTelemetryNonce, catchall, );
		m_pTelemetryNonce = outTelemetryNonce;

		return true;
	}
	rcatchall
	{
	}

	return false;
}

bool GameTransactionHttpTask::Configure( const int localGamerIndex
										,const u32 challengeResponseInfo
										,RsonWriter& wr
										,NetworkGameTransactions::PlayerBalance* outPB
										,NetworkGameTransactions::InventoryItemSet* outItemSet
										,NetworkGameTransactions::TelemetryNonce* outTelemetryNonce)
{
	rtry
	{
		//Call the 'simple' configure
		rverify(Base::Configure(localGamerIndex, challengeResponseInfo, wr)
			,catchall
			,rlTaskError("Failed to configure 'GameTransactionHttpTask'."));

		rverify(outTelemetryNonce, catchall, );
		m_pTelemetryNonce = outTelemetryNonce;

		m_pPlayerBalanceResponse = outPB;
		if (m_pPlayerBalanceResponse)
		{
			m_pPlayerBalanceResponse->Clear();
		}

		m_pItemSet = outItemSet;
		if (m_pItemSet)
		{
			m_pItemSet->Clear();
		}

		return true;
	}
	rcatchall
	{

	}

	return false;
}

void GameTransactionHttpTask::OnFinish(const int resultCode)
{
	GameTransactionBaseHttpTask::OnFinish(resultCode);
}


bool GameTransactionHttpTask::ProcessSuccess(const RsonReader &rr)
{
	rtry
	{
		RsonReader respReader;
		if(m_pPlayerBalanceResponse)
		{
			rcheck(rr.ReadReader("playerBalance", respReader), catchall, rlTaskError("No 'playerBalance' object found"));
			rcheck(m_pPlayerBalanceResponse->Deserialize(respReader), catchall, rlTaskError("Failed to deserialize 'playerBalance'"));
		}

		if(m_pItemSet && rr.ReadReader("inventoryItems", respReader))
		{
			rcheck(m_pItemSet->Deserialize(respReader), catchall, rlTaskError("Failed to deserialize 'InventoryItems'"));
		}

		rcheck(m_pTelemetryNonce, catchall, rlTaskError("m_pTelemetryNonce is NULL"));
		if (m_pTelemetryNonce)
		{
			rcheck(rr.ReadReader("TransactionId", respReader), catchall, rlTaskError("No 'TransactionId' object found"));
			rcheck(m_pTelemetryNonce->Deserialize(respReader), catchall, rlTaskError("Failed to deserialize 'TransactionId'"));
		}

		if (ApplyServerData())
		{
			if(m_pPlayerBalanceResponse)
			{
				int charSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot();
				NetworkGameTransactions::PlayerBalanceApplicationInfo outInfo;
				rcheck(m_pPlayerBalanceResponse->ApplyDataToStats(charSlot, outInfo), catchall, rlTaskError("Failed to ApplyDataToStats 'playerBalance'"));
			}

			if(m_pItemSet)
			{
				NetworkGameTransactions::InventoryDataApplicationInfo outInfo;
				rcheck(m_pItemSet->ApplyDataToStats(outInfo), catchall, rlTaskError("Failed to ApplyDataToStats 'InventoryItems'"));
			}
		}

		return true;
	}
	rcatchall
	{
		gnetWarning("warning: Failed in GameTransactionHttpTask success");
		rlTaskError("error: failed in GameTransactionHttpTask success");
	}

	return false;

}

bool NULLGameTransactionHttpTask::Configure(int fakeLatency)
{
	m_FakeLatency = fakeLatency;

	rlHttpTaskConfig finalConfig;
	finalConfig.m_AddDefaultContentTypeHeader = false;
	finalConfig.m_HttpHeaders = "Content-Type: application/text\r\n";

	if(!gnetVerify(rlHttpTask::Configure(&finalConfig)))
	{
		return false;
	}

	return true;
}

void NULLGameTransactionHttpTask::Start()
{
	rlTaskDebug2("Start");
	rlTaskBase::Start();
	m_StartTime = sysTimer::GetSystemMsTime();

	if(m_FakeLatency <= 0)
	{
		this->Finish(FINISH_SUCCEEDED, 200);
	}
}

void NULLGameTransactionHttpTask::Update(const unsigned timeStep)
{
	if(!this->IsActive())
	{
		return;
	}

	rlTaskBase::Update(timeStep);

	if(WasCanceled())
	{
		this->Finish(FINISH_CANCELED, -1);
	}
	else
	{
		m_FakeLatency -= timeStep;
		if (m_FakeLatency <= 0)
		{
			int resultCode = -1;
			FinishType fType = FINISH_SUCCEEDED;

#if !__FINAL
			atHashString codestringhash("ERROR_SUCCESS");
#else
			atHashString codestringhash(ATSTRINGHASH("ERROR_SUCCESS", 0xae0f460b));
#endif //!__FINAL

			resultCode = GameTransactionBaseHttpTask::GetTransactionEventErrorCode(codestringhash);
			fType = FINISH_SUCCEEDED;

			this->Finish(fType, resultCode);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////
bool DeleteSlotTransactionHttpTask::Configure(const int localGamerIndex
											  ,const u32 challengeResponseInfo
											  ,RsonWriter& wr
											  ,DeleteSlotResponseData* UNUSED_PARAM(outResponseData)
											  ,NetworkGameTransactions::TelemetryNonce* outTelemetryNonce)
{
	return GameTransactionHttpTask::Configure(localGamerIndex, challengeResponseInfo, wr, outTelemetryNonce);
}

#undef __net_channel

#endif // __NET_SHOP_ACTIVE

//eof 
