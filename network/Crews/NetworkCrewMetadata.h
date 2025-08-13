//
// filename:	netcrewmetadata.h
// description:	
//

#ifndef INC_NETCREWMETADATA_H_
#define INC_NETCREWMETADATA_H_

// Rage headers
#include "data/growbuffer.h"
#include "net/status.h"
#include "rline/clan/rlclancommon.h"

//Game headers
#include "Network/Cloud/CloudManager.h"

namespace rage {
	class bkGroup;
}

class NetworkCrewMetadata
{
public:
	static const int MAX_NUM_CLAN_METADATA_ENUMS = 15;

	NetworkCrewMetadata();
	~NetworkCrewMetadata();

	void SetClanId(rlClanId clanID, bool depriacted = false);
	void Update();
	void Clear();

	bool Pending() const {return m_metadata_netStatus.Pending() || m_cloudGameMetadata.Pending();	}
	bool Succeeded() const  {return m_bMetaData_received && !Pending();  }   // m_cloudGameMetadata can error 500 by design, we consider this as success
	bool Failed() const  {return m_metadata_netStatus.Failed() || m_cloudGameMetadata.Failed();		}

	//Purpose:
	// Return the number of values available under this set name
	int GetCountForSetName(const char* setName) const;

	//PURPOSE:
	// Get the give value (by index) of a given set name
	bool GetIndexValueForSetName(const char* setName, int index, char* out_string, int maxSTR) const;
	template<int SIZE>
 	bool GetIndexValueForSetName(const char* setName, int index, char (&buf)[SIZE]) const
 	{
 		return GetIndexValueForSetName(setName, index, buf, SIZE);
 	}

	//PURPOSE:
	// Checks for the existence of a value in the 'CrewAttribute' set with the given name
	bool GetCrewHasCrewAttribute(const char* attributeName) const;

	bool GetCrewInfoValueInt(const char* valueName, int& out_value) const;
	
	bool GetCrewInfoValueString(const char* valueName, char* out_value, int maxStr) const;
	template<int SIZE>
 	bool GetCrewInfoValueString(const char* valueName, char (&buf)[SIZE]) const
 	{
 		return GetCrewInfoValueString(valueName, buf, SIZE);
 	}
	
	bool GetCrewInfoCrewRankTitle(int rank, char* out_rankString, int maxStr) const;
	template<int SIZE>
 	bool GetCrewInfoCrewRankTitle(int rank, char (&buf)[SIZE]) const
 	{
 		return GetCrewInfoCrewRankTitle(rank, buf, SIZE);
 	}

#if __BANK
	void DebugPrint();
	void DebugAddWidgets(bkGroup* toThis);
#endif

	//PURPOSE: 
	//Used to request a refresh of the data
	bool RequestRefresh();

private:

	void Cancel();
	bool DoCrewInfoRequest();

	//Small class to manage crew metadata files.
	class CrewMetaDataCloudFile : public CloudListener
	{
	public:
		CrewMetaDataCloudFile(const char* crewCloudFilePath);
		~CrewMetaDataCloudFile();

		void SetClanId(rlClanId clanId);
		void Cancel();
		void Clear();

		bool DoRequest();
		bool Succeeded() const { return m_bDataReceived; }
		bool Pending() const { return m_cloudReqID != -1; }
		bool Failed() const { return !Pending() && !Succeeded(); }

		bool GetCrewInfoValueInt(const char* valueName, int& out_value) const;
		bool GetCrewInfoValueString(const char* valueName, char* out_value, int maxStr) const;


		//PURPOSE:
		//Handler for downloading a cloud file.
		virtual void OnCloudEvent(const CloudEvent* pEvent);

		const datGrowBuffer& GetGrowBuffer() const { return m_cloudDataGB; }

	protected:
		rlClanId m_clanId;
		
		//Crew Info (from the cloud source)	
		bool		m_bDataReceived;
		const char* m_crewCloudFilePath;
		CloudRequestID m_cloudReqID;
		datGrowBuffer m_cloudDataGB;

		rlCloudWatcher m_cloudWatcher;
		void OnCloudFileModified(const char* pathSpec, const char* fullPath);
	};

	
	//Crew MetaData (proper)
	rlClanId m_clanId;
	rlClanMetadataEnum m_metaData[MAX_NUM_CLAN_METADATA_ENUMS];
	unsigned int m_uReturnCount, m_uTotalCount;
	netStatus m_metadata_netStatus;
	bool m_bMetaData_received;

	//Crew Info (from the cloud source)	
// 	bool m_bCloudData_received;
// 	CloudRequestID m_cloudReqID;
// 	datGrowBuffer m_cloudDataGB;
	CrewMetaDataCloudFile m_cloudGameMetadata;
};

#endif // !INC_NETCREWMETADATA_H_
