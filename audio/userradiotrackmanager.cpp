#if __WIN32PC

#pragma warning(disable: 4668)
#include <windows.h>
#include <shobjidl.h> // IShellLink
#include <shlguid.h> // CLSID_ShellLink
#pragma warning(error: 4668)

#include "userradiotrackmanager.h"
#include "system/FileMgr.h"
#include "frontend/PauseMenu.h"
#include "text/messages.h"
#include "text/textfile.h"

#include "atl/map.h"
#include "atl/bitset.h"
#include "file/stream.h"
#include "system/endian.h"
#include "system/magicnumber.h"

//#include "frontend/Frontend.h"
#include "audiohardware/driver.h"
#include "audiohardware/driverutil.h"

#include "xmmintrin.h"
#include "emmintrin.h"

#if RSG_PC && !__NO_OUTPUT
namespace rage
{
	XPARAM(processinstance);
}
#endif


const sysIpcPriority UserRadioTrackScanThreadPriority = PRIO_NORMAL;
const int UserMusicScanThreadCpu = 0;

// needs to align to 16
#define SecondsToStereoBytes(s) ((s&~15) * 44100 * 2 * 2)
u32 g_TrackAnalysisBufferSize = SecondsToStereoBytes(64);
s16 *g_TrackAnalysisBuffer = NULL;

bool audUserRadioTrackDatabase::Save(const WCHAR *filename) const
{
	WCHAR indexFile[RAGE_MAX_PATH];
	WCHAR stringsFile[RAGE_MAX_PATH];

	wcscpy_s(indexFile, filename);
	wcscat_s(indexFile, L".db");

	wcscpy_s(stringsFile, filename);
	wcscat_s(stringsFile, L".dbs");
	
	HANDLE hIndexFile = CreateFileW(indexFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
	HANDLE hStringsFile = CreateFileW(stringsFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);

#if __BANK
	if((hIndexFile == INVALID_HANDLE_VALUE || hStringsFile == INVALID_HANDLE_VALUE) && PARAM_processinstance.Get())
	{
		Warningf("Multiple copies of game running. Unable to save usermusic database on second instance.");
		return false;
	}
#endif

	if(audVerifyf(hIndexFile != INVALID_HANDLE_VALUE && hStringsFile != INVALID_HANDLE_VALUE, "Failed to create user track database"))
	{
		for(s32 i = 0; i < m_Data.GetCount(); i++)
		{
			DWORD bytesWritten = 0;
			WriteFile(hIndexFile, &m_Data[i], sizeof(TrackData), &bytesWritten, NULL);
			WriteFile(hStringsFile, m_Data[i].FileName, m_Data[i].FileNameLength * sizeof(WCHAR), &bytesWritten, NULL);
		}

		CloseHandle(hIndexFile);
		CloseHandle(hStringsFile);

		return true;
	}
	return false;
}

bool audUserRadioTrackDatabase::Load(const WCHAR *filename)
{
	WCHAR indexFile[RAGE_MAX_PATH];
	WCHAR stringsFile[RAGE_MAX_PATH];

	wcscpy_s(indexFile, filename);
	wcscat_s(indexFile, L".db");

	wcscpy_s(stringsFile, filename);
	wcscat_s(stringsFile, L".dbs");
	
	HANDLE hIndexFile = CreateFileW(indexFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_HIDDEN, NULL);
	HANDLE hStringsFile = CreateFileW(stringsFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_HIDDEN, NULL);

	if(hIndexFile != INVALID_HANDLE_VALUE && hStringsFile != INVALID_HANDLE_VALUE)
	{
		const u32 indexSize = static_cast<u32>(GetFileSize(hIndexFile, NULL));
		const u32 numEntries = indexSize / sizeof(TrackData);

		if(numEntries > 0 && audVerifyf(numEntries * sizeof(TrackData) == indexSize, "Invalid user track database size; expected %u found %u", numEntries * sizeof(TrackData), indexSize))
		{
			m_Data.Resize(numEntries);
			for(u32 i = 0; i < numEntries; i++)
			{
				DWORD bytesRead = 0;
				ReadFile(hIndexFile, &m_Data[i], sizeof(TrackData), &bytesRead, NULL);
				WCHAR *inputFileName = rage_new WCHAR[m_Data[i].FileNameLength + 1];
				ReadFile(hStringsFile, inputFileName, m_Data[i].FileNameLength * sizeof(WCHAR), &bytesRead, NULL);
				inputFileName[m_Data[i].FileNameLength] = L'\0';

				m_Data[i].FileName = inputFileName;
			}
		}
		CloseHandle(hIndexFile);
		CloseHandle(hStringsFile);

		return true;
	}
	return false;
}

s32 audUserRadioTrackDatabase::AddTrack(const TrackData &track)
{
	 m_Data.Grow() = track;
	 const s32 index = static_cast<s32>(m_Data.size()) - 1;
	 
	 // Ensure fileNameLength is computed to simplify serialisation
	 if(track.FileName)
	 {
		 m_Data[index].FileNameLength = static_cast<u32>(wcslen(track.FileName));
	 }
	 else
	 {
		 m_Data[index].FileNameLength = 0;
	 }

	 return index;
}

s32 audUserRadioTrackDatabase::FindTrackIndex(const WCHAR *fileName)
{

#if 1
	for(s32 i = 0; i < m_Data.GetCount(); i++)
	{
		if(_wcsicmp(m_Data[i].FileName, fileName) == 0)
		{
			return i;
		}
	}
	return -1;
#else
	if(m_SearchCache.GetNumUsed() == 0 && m_Data.GetCount() > 0)
	{
		for(s32 i = 0; i < m_Data.GetCount(); i++)
		{
			m_SearchCache.Insert(m_Data[i].FileName, i);
		}
	}
	const s32 *res = m_SearchCache.Access(fileName);
	if(res)
	{
		return *res;
	}
	return -1;
#endif
}

audUserRadioTrackManager::audUserRadioTrackManager() :
m_bInitialised(false),
m_CurrentTrack(0),
m_ForceNextTrack(-1),
m_CompleteScan(false),
m_ScanStatus(SCAN_STATE_IDLE),
m_ScanCount(0),
m_IsCompleteScanning(false),
m_nTracksToScan(0)
{
	m_CurrentDatabase = NULL;
	m_NewDatabase = NULL;
}

audUserRadioTrackManager::~audUserRadioTrackManager()
{
	
}

void audUserRadioTrackManager::Shutdown()
{
	if(m_CurrentDatabase)
	{
		delete m_CurrentDatabase;
		m_CurrentDatabase = NULL;
	}
}

bool audUserRadioTrackManager::Initialise()
{
	audMediaReader::InitClass();

	CFileMgr::SetUserDataDir("User Music\\", true);

	m_bInitialised = LoadFileLookup();

	for(s32 i = 0; i < TRACK_FORMAT_COUNT; i++)
	{
		m_bIsDecoderAvailable[i] = audMediaReader::IsFormatAvailable((USER_TRACK_FORMAT)i);
	}

	m_ScanStatus = SCAN_STATE_UNSCANNED;
	return m_bInitialised;
}

void audUserRadioTrackManager::PostLoad()
{
	if(GetRadioMode() == USERRADIO_PLAY_MODE_SEQUENTIAL && GetNumTracks() > 0)
	{
		const s32 numTracks = GetNumTracks();
		// The track stored in history is always ahead of the audible track when the save is made, so compensate
		m_CurrentTrack = (static_cast<s32>(m_History[0]) + numTracks - 1) % numTracks;
	}
}

const WCHAR *audUserRadioTrackManager::GetDataFileLocation() const
{
	const char *appDataDirectory = CFileMgr::GetAppDataRootDirectory();
	
	const int nChars = MultiByteToWideChar(CP_UTF8, 0, appDataDirectory, -1, NULL, 0);
	size_t newLength = nChars + 2 + wcslen(USER_TRACK_LOOKUP_FILENAME);
	
	WCHAR *pwcsUserDir = rage_new WCHAR[newLength];
	MultiByteToWideChar(CP_UTF8, 0, appDataDirectory, -1, (LPWSTR)pwcsUserDir, nChars);
	
	if(appDataDirectory[strlen(appDataDirectory) - 1] != '\\')
	{
		wcscat_s(pwcsUserDir, newLength, L"\\");
	}
	wcscat_s(pwcsUserDir, newLength, USER_TRACK_LOOKUP_FILENAME);

	return pwcsUserDir;
}

void audUserRadioTrackManager::UpdateScanningUI(const bool showMessageOnCompletion /* = true */)
{
	if(m_NewDatabase)
	{
		if(m_CurrentDatabase && m_CurrentDatabase != m_NewDatabase)
		{
			delete m_CurrentDatabase;
		}
		m_CurrentDatabase = m_NewDatabase;
		m_NewDatabase = NULL;
		m_ScanStatus = SCAN_STATE_IDLE;
		
		if(showMessageOnCompletion && !CPauseMenu::IsActive())
		{
			char *string = TheText.Get("MO_UR_COMPLETEDSCAN");
			CNumberWithinMessage numberArray[1];
			numberArray[0].Set(GetNumTracks());
			CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string, numberArray, 1);
			audDisplayf("Submitting scan complete message");
		}
	}
	// If we succeeded without building a database then there were no tracks to find, so go back to idle.
	if(m_ScanStatus == SCAN_STATE_SUCCEEDED)
	{
		m_ScanStatus = SCAN_STATE_IDLE;
	}
}

bool audUserRadioTrackManager::LoadFileLookup()
{	
	if(m_CurrentDatabase)
	{
		delete m_CurrentDatabase;		
	}
	m_CurrentDatabase = rage_new audUserRadioTrackDatabase();
	const WCHAR *dataFileLocation = GetDataFileLocation();
	const bool ret = m_CurrentDatabase->Load(dataFileLocation);

	delete[] dataFileLocation;

	CFileMgr::SetDir("");

	return ret;
}

u32 audUserRadioTrackManager::GetTrackDuration(const s32 trackId) const
{
	if(!m_CurrentDatabase || trackId >= m_CurrentDatabase->GetNumTracks())
		return ~0U;

	return m_CurrentDatabase->GetTrack(trackId).DurationMs;	
}

const char *audUserRadioTrackManager::GetTrackTitle(const s32 trackId) const
{
	if(!m_CurrentDatabase || trackId >= m_CurrentDatabase->GetNumTracks())
		return NULL;

	return m_CurrentDatabase->GetTrack(trackId).Title;
}

const char *audUserRadioTrackManager::GetTrackArtist(const s32 trackId) const
{
	if(!m_CurrentDatabase || trackId >= m_CurrentDatabase->GetNumTracks())
		return NULL;

	return m_CurrentDatabase->GetTrack(trackId).Artist;
}

u32 audUserRadioTrackManager::GetStartOffsetMs(const s32 trackId) const
{
	if(!m_CurrentDatabase || trackId >= m_CurrentDatabase->GetNumTracks())
		return ~0U;

	u32 startOffset = m_CurrentDatabase->GetTrack(trackId).SkipStartMs;
	if(startOffset < 100 || GetRadioMode() != USERRADIO_PLAY_MODE_RADIO)
	{
		return 0;
	}
	return startOffset - 100;
}

u32 audUserRadioTrackManager::GetTrackPostRoll(const s32 trackId) const
{
	if(!m_CurrentDatabase || trackId >= m_CurrentDatabase->GetNumTracks())
		return 0;

	u32 skipEnd = m_CurrentDatabase->GetTrack(trackId).SkipEndMs;
	if(skipEnd < 100 || GetRadioMode() != USERRADIO_PLAY_MODE_RADIO)
	{
		return 0;
	}
	return skipEnd - 100;
}

f32 audUserRadioTrackManager::ComputeTrackMakeUpGain(const s32 trackId) const
{
	if(!m_CurrentDatabase || trackId >= m_CurrentDatabase->GetNumTracks())
	{
		return 0.f;
	}

	const u32 loudness = m_CurrentDatabase->GetTrack(trackId).Loudness;
	if(loudness  == 0 || GetRadioMode() != USERRADIO_PLAY_MODE_RADIO)
	{
		return 0.f;
	}

	// we have valid peak/rms info
	const f32 peakVolumeDb = audDriverUtil::ComputeDbVolumeFromLinear((loudness &0xffff)/65536.f);
	const f32 rmsVolumeDb = audDriverUtil::ComputeDbVolumeFromLinear((loudness >>16)/65536.f);

	// aim for -9dB RMS but don't go allow peak volume to go above 4dB and don't boost more than +8dB
	return Clamp(Min(4.f - peakVolumeDb, -9.f - rmsVolumeDb), -3.f, 8.f);
}

const WCHAR *audUserRadioTrackManager::GetFileName(const s32 trackId) const
{
	if(!m_CurrentDatabase || trackId >= m_CurrentDatabase->GetNumTracks())
		return NULL;	
	
	return m_CurrentDatabase->GetTrack(trackId).FileName;
}

const WCHAR *audUserRadioTrackManager::GetLnkTarget(const WCHAR *lnk)
{
	HRESULT hr;
	IShellLinkW *pLink;
	IPersistFile *ppf;
	WIN32_FIND_DATAW wfd;
	
	WCHAR *pPath;

	if(FAILED(hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&pLink)))
	{
		return NULL;
	}
	else
	{
		// now need to get the IPersistFile interface
        if (FAILED(pLink->QueryInterface(IID_IPersistFile, (void**)&ppf)))
        {
			pLink->Release();
			return NULL;
		}
		else
		{
			if(FAILED(ppf->Load(lnk, STGM_READ)))
			{
				ppf->Release();
				pLink->Release();
				return NULL;
			}
			else
			{
				pPath = rage_new WCHAR[MAX_PATH];

				HRESULT hr = pLink->GetPath(pPath, MAX_PATH, (WIN32_FIND_DATAW*)&wfd, SLGP_SHORTPATH);

				ppf->Release();
				pLink->Release();
					
				if(SUCCEEDED(hr))
					return pPath;
				else
					return NULL;
			}
		}

	}	
}

USER_TRACK_FORMAT audUserRadioTrackManager::GetFileFormat(const WCHAR *fileName)
{
	return audMediaReader::GetFileFormat(fileName);
}

s32 audUserRadioTrackManager::GetNextTrack()
{
	s32 track = -1;

	if(!GetNumTracks())
		return -1;

	switch(GetRadioMode())
	{
	case USERRADIO_PLAY_MODE_RANDOM:
	case USERRADIO_PLAY_MODE_RADIO:
		{
			if(m_ForceNextTrack != -1) // this means the user selected to play the previous track
			{
				track = m_ForceNextTrack;
				m_ForceNextTrack = -1;
				return track;
			}
			
			for(s32 i = 0; i < 50; i++) // try 50 times, then just go sequential to avoid risk of inf loop if something goes wrong
			{
				track = audEngineUtil::GetRandomNumberInRange(0, GetNumTracks()-1);
				if(!IsTrackInHistory(track))
				{
					m_CurrentTrack = track;				
					return track;
				}				
			}

			// just do sequential
			track = m_CurrentTrack = (m_CurrentTrack + 1) % GetNumTracks();			
			return track;
		}
	case USERRADIO_PLAY_MODE_SEQUENTIAL: 
		{
			if(m_ForceNextTrack != -1) // this means the user selected to play the previous track
			{
				m_CurrentTrack = track = m_ForceNextTrack;
				m_ForceNextTrack = -1;
				return track;
			}
			
			m_CurrentTrack = (m_CurrentTrack + 1) % GetNumTracks();

			// Preserve current track in history for save/load
			m_History[0] = static_cast<u32>(m_CurrentTrack);
			return m_CurrentTrack;
		}
	}
	return -1;
}

#pragma warning(disable: 4748)
DECLARE_THREAD_FUNC(audUserRadioTrackManager::RebuildFileDatabaseCB)
{
	audUserRadioTrackManager *_this = (audUserRadioTrackManager*)ptr;
	_this->RebuildFileDatabase();
}

void audUserRadioTrackManager::RebuildFileDatabase()
{
	char *userDir;
	u32 count;

	sysPerformanceTimer timer("MediaScan Timer");
	timer.Start();

	if(m_CompleteScan)
	{
		m_IsCompleteScanning = true;
	}
	
	// catalogue all user files from the specified source directory
	Displayf("audUserRadioTrackManager: rebuilding user music database ... (source dir: %s)", USER_TRACK_PATH);
	
	m_AbortRequested = false;

	// initialise COM for this thread (required to resolve shortcuts)
	CoInitialize(NULL);

	audUserRadioTrackDatabase *database = rage_new audUserRadioTrackDatabase();
	
	CFileMgr::SetAppDataDir("User Music\\", true);
	CFileMgr::SetUserDataDir("User Music\\", true);

	// grab the my documents directory
	char *curDir = CFileMgr::GetCurrDirectory();
	userDir = rage_new char[lstrlen(curDir)+1];
	lstrcpy(userDir, curDir);	
	
	const int nChars = MultiByteToWideChar(CP_UTF8, 0, userDir, -1, NULL, 0);
	const WCHAR *pwcsUserDir = rage_new WCHAR[nChars];
	MultiByteToWideChar(CP_UTF8, 0, userDir, -1, (LPWSTR)pwcsUserDir, nChars);
	delete[] userDir;

	// pass in full path to CatalogueDirectory() to ensure we can correctly open files
	// regardless of current directory
	count = CatalogueDirectory(pwcsUserDir, database, 0);
	
	delete [] pwcsUserDir;
	
	Displayf("Finished - found %d files\n", count);

	CFileMgr::SetAppDataDir("User Music\\", true);
	
	// we need access to the file name db
	CFileMgr::SetAppDataDir("User Music\\", true);
	
	m_ScanCount = 0;
	m_nTracksToScan = database->GetNumTracks();
	
	for(s32 i=0; i < database->GetNumTracks(); i++)
	{
		audUserRadioTrackDatabase::TrackData &trackData = database->GetTrack(i);

		// If we already have a populated database, check to see if we can skip the complete scan and re-use the data for this track.
		if(m_CurrentDatabase != NULL && m_CurrentDatabase != database)
		{
			const s32 oldIndex = m_CurrentDatabase->FindTrackIndex(trackData.FileName);
			if(oldIndex != -1)
			{
				const audUserRadioTrackDatabase::TrackData &oldTrackData = m_CurrentDatabase->GetTrack(oldIndex);

				trackData.Loudness = oldTrackData.Loudness;
				trackData.SkipStartMs = oldTrackData.SkipStartMs;
				trackData.SkipEndMs = oldTrackData.SkipEndMs;
				safecpy(trackData.Artist, oldTrackData.Artist);
				safecpy(trackData.Title, oldTrackData.Title);

				if(trackData.Loudness != 0 || trackData.SkipEndMs != 0 || trackData.SkipStartMs != 0)
				{
					naDisplayf("Reusing data for %S", trackData.FileName);
				}
			}
		}
		if(!m_AbortRequested && trackData.Loudness == 0 && trackData.SkipStartMs == 0 && trackData.SkipEndMs == 0)
		{
			audMediaReader* pAudioReader = rage_new audMediaReader();
			Assert(pAudioReader != NULL);
			if(pAudioReader)
			{				
				if(pAudioReader->Open(trackData.FileName, 0, true))
				{
					if(pAudioReader->IsFileOpen())
					{
						if(m_CompleteScan)
						{
							AnalyzeTrack(trackData.FileName, pAudioReader, trackData.DurationMs, trackData.SkipStartMs, trackData.SkipEndMs, trackData.Loudness);
						}

						safecpy(trackData.Artist, pAudioReader->GetArtist());
						safecpy(trackData.Title, pAudioReader->GetTitle());

						pAudioReader->Close();
					}
					else
					{
						naErrorf("Failed to analyze: %S", trackData.FileName);
						trackData.DurationMs = ~0U;
					}
				}
				else
				{
					naErrorf("Failed to analyze: %S", trackData.FileName);
					trackData.DurationMs = ~0U;
				}
			}
			delete pAudioReader;
		}	
		m_ScanCount++;
	}

	m_IsCompleteScanning = false;
		
	if(g_TrackAnalysisBuffer)
	{
		audDriver::FreeVirtual(g_TrackAnalysisBuffer);
		g_TrackAnalysisBuffer = NULL;
	}

	CoUninitialize();

	if(m_AbortRequested)
	{
		Displayf("Aborted media scan");
	}
	else
	{
		const WCHAR *dataFileLocation = GetDataFileLocation();
		database->Save(dataFileLocation);
		delete [] dataFileLocation;	
	}

	timer.Stop();

#if !__NO_OUTPUT
	timer.Print();
#endif

	// A scan that finishes immediately leads to what looks like a UI bug, as we get a frame or two of 'Scanning'.  Since the scan time is totally unpredictable (until we've followed
	// the shortcuts we don't know how many files we'll be looking through) the most robust fix is to apply a minimum time that we'll remain in a 'Scanning' state.
	u32 elapsedTimeMs = (u32)timer.GetElapsedTimeMS();
	enum { minScanTime = 1500 };
	if(elapsedTimeMs < minScanTime)
	{
		sysIpcSleep(minScanTime - elapsedTimeMs);
	}

	if(!m_AbortRequested)
	{
		m_NewDatabase = database;
	}

	SetStatus(SCAN_STATE_SUCCEEDED);
	return;
}

bool audUserRadioTrackManager::IsFileLink(const WCHAR *pDir)
{
	const WCHAR *ext = wcsrchr(pDir, L'.');

	if(!ext || _wcsicmp(ext, L".lnk"))
	{
		return false;
	}

	return true;
}

bool audUserRadioTrackManager::PossiblyAddTrack(const WCHAR *target, audUserRadioTrackDatabase *database)
{
	USER_TRACK_FORMAT format = GetFileFormat(target);
	s32 duration = 0;
	if(format != TRACK_FORMAT_UNKNOWN && audMediaReader::IsValidFile(target, duration))
	{	
		audUserRadioTrackDatabase::TrackData track;
			
		track.DurationMs = static_cast<u32>(duration);	
		track.format = format;
		track.FileName = rage_new WCHAR[wcslen(target) + 1];
		lstrcpyW((LPWSTR)track.FileName, target);

		database->AddTrack(track);
		
		return true;
	}
	return false;
}

void audUserRadioTrackManager::AnalyzeBuffer(const s32 samplesRead, const u32 rmsWindowSize, s32 &firstNonZeroSample, s32 &lastNonZeroSample, f32 &maxPeak, f32 &maxRms)
{
	const s32 silenceThresholdStart = 413; // around -38dB
	const s32 silenceThresholdEnd = 1038; // around -30dB
	bool foundContent = false;

	lastNonZeroSample = 0;
	firstNonZeroSample = 0;
	for(s32 sampleIndex = 0; sampleIndex < samplesRead; sampleIndex += rmsWindowSize)
	{
		// compute a sample accurate start offset - detect the first sample louder than our threshold
		for(u32 sampleSubIndex = 0; sampleSubIndex < rmsWindowSize && sampleIndex + sampleSubIndex < samplesRead; sampleSubIndex += 2)
		{
			s32 maxSample = Max(Abs(g_TrackAnalysisBuffer[sampleIndex+sampleSubIndex]),Abs(g_TrackAnalysisBuffer[sampleIndex+sampleSubIndex+1]));

			if(maxSample > silenceThresholdEnd)
			{
				// divide by two since we're dealing with stereo data
				lastNonZeroSample = (sampleIndex+sampleSubIndex);
			}

			if(!foundContent && maxSample > silenceThresholdStart)
			{
				foundContent = true;
			}
			else if(!foundContent)
			{
				// increment only once per two samples since we're dealing with stereo data
				firstNonZeroSample++;
			}
		}
		
		if(sampleIndex + rmsWindowSize < samplesRead)
		{
			// compute RMS and Peak volume - window size defined by rmsWindowSize above
			__m128 runningTotal = _mm_setzero_ps();
			const __m128 oneOverDivisor = _mm_set_ps1(1.f / 32767.f);
			const __m128 oneOverMeanDivisor = _mm_set_ps1(1.f / (f32)rmsWindowSize);
			__m128 maxPeakV = _mm_setzero_ps();
			for(u32 sampleSubIndex = 0; sampleSubIndex < rmsWindowSize; sampleSubIndex += 8)
			{
				const __m128i inputInts = _mm_load_si128((__m128i*)&g_TrackAnalysisBuffer[sampleSubIndex + sampleIndex]);

				// unpack 8 16 bit ints into 32bit ints (splitting into two vectors)
				const __m128i inputInts1 = _mm_srai_epi32(_mm_unpacklo_epi16(inputInts,inputInts), 16);
				const __m128i inputInts2 = _mm_srai_epi32(_mm_unpackhi_epi16(inputInts,inputInts), 16);

				// convert to float, square and sum
				__m128 samples1 = _mm_mul_ps(oneOverDivisor, _mm_cvtepi32_ps(inputInts1));
				__m128 samples2 = _mm_mul_ps(oneOverDivisor, _mm_cvtepi32_ps(inputInts2));
				samples1 = _mm_mul_ps(samples1,samples1);
				samples2 = _mm_mul_ps(samples2,samples2);

				const __m128 maxMask1 = _mm_cmpgt_ps(samples1, samples2);
				const __m128 maxMask2 = _mm_cmpge_ps(samples2,samples1);
				const __m128 maxSamples12 = _mm_or_ps(_mm_and_ps(samples1, maxMask1),_mm_and_ps(samples2,maxMask2));
			
				const __m128 maxMask3 = _mm_cmpgt_ps(maxSamples12, maxPeakV);
				const __m128 maxMask4 = _mm_cmpge_ps(maxPeakV,maxSamples12);
				maxPeakV = _mm_or_ps(_mm_and_ps(maxSamples12, maxMask3),_mm_and_ps(maxPeakV,maxMask4));

				runningTotal = _mm_add_ps(runningTotal, 
					_mm_add_ps(_mm_mul_ps(oneOverMeanDivisor,samples1),_mm_mul_ps(oneOverMeanDivisor,samples2)));
			}

			// perform horizontal add
			__m128 meanSquared = _mm_add_ss(
				_mm_add_ss(_mm_shuffle_ps(runningTotal, runningTotal, _MM_SHUFFLE(0,0,0,0)), _mm_shuffle_ps(runningTotal, runningTotal, _MM_SHUFFLE(1,1,1,1))),
				_mm_add_ss(_mm_shuffle_ps(runningTotal, runningTotal, _MM_SHUFFLE(2,2,2,2)), _mm_shuffle_ps(runningTotal, runningTotal, _MM_SHUFFLE(3,3,3,3)))
			);

			// and horizontal max
			const __m128 x = _mm_shuffle_ps(maxPeakV, maxPeakV, _MM_SHUFFLE(0,0,0,0));
			const __m128 y = _mm_shuffle_ps(maxPeakV, maxPeakV, _MM_SHUFFLE(1,1,1,1));
			const __m128 z = _mm_shuffle_ps(maxPeakV, maxPeakV, _MM_SHUFFLE(2,2,2,2));
			const __m128 w = _mm_shuffle_ps(maxPeakV, maxPeakV, _MM_SHUFFLE(3,3,3,3));
			const __m128 xmask = _mm_cmpgt_ss(x,y);
			const __m128 ymask = _mm_cmpge_ss(y,x);
			const __m128 zmask = _mm_cmpgt_ss(z,w);
			const __m128 wmask = _mm_cmpge_ss(w,z);
			const __m128 xyMax = _mm_or_ps(_mm_and_ps(xmask,x),_mm_and_ps(ymask,y));
			const __m128 zwMax = _mm_or_ps(_mm_and_ps(zmask,z),_mm_and_ps(wmask,w));
			const __m128 xyMask = _mm_cmpgt_ss(xyMax,zwMax);
			const __m128 zwMask = _mm_cmpge_ss(zwMax,xyMax);
			const __m128 maxVal = _mm_or_ps(_mm_and_ps(xyMask,xyMax),_mm_and_ps(zwMask,zwMax));
		
			f32 maxSampleValue;
			f32 rms;
			_mm_store_ss(&maxSampleValue,maxVal);
			_mm_store_ss(&rms,meanSquared);
			maxPeak = Max(maxPeak,maxSampleValue);
			maxRms = Max(maxRms,rms);
		}
	}
}

void audUserRadioTrackManager::AnalyzeTrack(const WCHAR *trackPath, audMediaReader *pTrack, u32 &duration, u32 &skipStart, u32 &skipEnd, u32 &gain)
{
	Assert(pTrack);
	sysPerformanceTimer analysisTimer("Analysis");
	analysisTimer.Start();
	duration = pTrack->GetStreamLengthMs();

	if(!g_TrackAnalysisBuffer)
	{
		g_TrackAnalysisBuffer = (s16*)audDriver::AllocateVirtual(g_TrackAnalysisBufferSize);
		if(!g_TrackAnalysisBuffer)
		{
			return;
		}
	}

	// decode the start and look for silence
	s32 bytesRead = pTrack->FillBuffer(g_TrackAnalysisBuffer,g_TrackAnalysisBufferSize);
	const u32 rmsWindowSize = 65536;
	s32 samplesRead = (bytesRead>>1) & (~(rmsWindowSize-1));
	s32 startSamplesToSkip = 0, endSamplesToSkip = 0;

	f32 maxPeak = 0.f;
	f32 maxRms = 0.f;

	AnalyzeBuffer(samplesRead,rmsWindowSize,startSamplesToSkip,endSamplesToSkip,maxPeak,maxRms);

	const f32 invSamplesPerMs = 1.f / (f32)(pTrack->GetSampleRate()/1000.f);

	if(duration > 22000)
	{
		audMediaReader *pTrackForEnd;

		// windows media doesnt seem to like seeking an already open file, open a new reader
		pTrackForEnd = rage_new audMediaReader();
		if(pTrackForEnd->Open(trackPath, duration - 20000, true))
		{
			pTrackForEnd->SetMsStartOffset(duration - 20000);

			s32 dummyStartSamplesToSkip=0;
		
			// decode the last 20 seconds
			bytesRead = pTrackForEnd->FillBuffer(g_TrackAnalysisBuffer, g_TrackAnalysisBufferSize);
			s32 samplesRead = (bytesRead>>1);// & (~(rmsWindowSize-1));
			s32 silenceStartPoint = 0;
			AnalyzeBuffer(samplesRead, rmsWindowSize, dummyStartSamplesToSkip, silenceStartPoint, maxPeak, maxRms);
			// silence start point is already samples/2, divide samplesRead by 2 to convert to stereo samples
			endSamplesToSkip = (samplesRead>>1) - (silenceStartPoint>>1);

			pTrackForEnd->Close();
		}
		delete pTrackForEnd;
	}
	else
	{
		endSamplesToSkip = 0;
	}


	skipStart = (u32)(startSamplesToSkip * invSamplesPerMs);
	skipEnd = (u32)(endSamplesToSkip * invSamplesPerMs);

	// 

	maxPeak = Min(1.f,sqrtf(maxPeak));
	maxRms = Min(1.f,sqrtf(maxRms));

	const u32 packedPeak = static_cast<u32>(65535U * maxPeak);
	const u32 packedRms = static_cast<u32>(65535U * maxRms);
	gain = (packedPeak | (packedRms<<16));
	analysisTimer.Stop();
	Displayf("Analyzed %s - %s in %f: %.2fs pre-roll, %.2fs post-roll, Maximum RMS: %.2fdB Peak: %.2fdB", pTrack->GetArtist(), pTrack->GetTitle(), analysisTimer.GetTimeMS()/1000.f, skipStart / 1000.f, skipEnd / 1000.f, audDriverUtil::ComputeDbVolumeFromLinear(maxRms),audDriverUtil::ComputeDbVolumeFromLinear(maxPeak));
}

u32 audUserRadioTrackManager::CatalogueDirectory(const WCHAR *srcDir, audUserRadioTrackDatabase *database, s32 level)
{
	if(level >= MAX_SEARCH_LEVELS)
	{
		Errorf("audUserRadioTrackManager::CatalogueDirectory: skipping \"%S\" - recursion level too deep! (%d)", srcDir, level);
		return 0;
	}

	size_t bufLen = wcslen(srcDir) + 6;
	WCHAR *searchPath = rage_new WCHAR[bufLen];
	lstrcpyW(searchPath, srcDir);
	lstrcatW(searchPath, L"\\*.*");

	WIN32_FIND_DATAW findData;
	unsigned int count = 0;
	HANDLE hFind = FindFirstFileW(searchPath, &findData);

	delete[] searchPath;

	if(hFind != INVALID_HANDLE_VALUE)
	{	
		BOOL bActive = true;

		while(bActive && !m_AbortRequested)
		{
			if(findData.cFileName[0] == L'.')
			{
				;// skip '.' and '..'
			}
			else if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				WCHAR *newPath = rage_new WCHAR[wcslen(srcDir) + wcslen(findData.cFileName) + 2];
				lstrcpyW(newPath, srcDir);
				lstrcatW(newPath, L"\\");
				lstrcatW(newPath, findData.cFileName);
								
				count += CatalogueDirectory(newPath, database, level+1);
				
				delete[] newPath;				
			}
			else
			{
				if(!level && IsFileLink(findData.cFileName))
				{
					// it's a file link in the top level directory so we'll try to resolve it
					Displayf("%S appears to be a shortcut...", findData.cFileName);

					WCHAR *lnk = rage_new WCHAR[wcslen(findData.cFileName) + wcslen(srcDir) + 2];

					lstrcpyW(lnk, srcDir);
					lstrcatW(lnk, L"\\");
					lstrcatW(lnk, findData.cFileName);

					const WCHAR *target = GetLnkTarget(lnk);

					delete[] lnk;

					if(target)
					{
						Displayf("Resolved %S to %S", findData.cFileName, target);

						DWORD dwAttribs = GetFileAttributesW(target);
						if(dwAttribs == INVALID_FILE_ATTRIBUTES)
						{
							Warningf("invalid shortcut.");
						}
						else
						{
							if(dwAttribs & FILE_ATTRIBUTE_DIRECTORY)
							{
								count += CatalogueDirectory(target, database, level+1);
							}
							else
							{
								if(PossiblyAddTrack(target, database))
								{
									count++;
								}
							}

						}
						delete[] target;
					} // if(target)
 
				}
				else if(GetFileFormat(findData.cFileName) != TRACK_FORMAT_UNKNOWN)
				{
					WCHAR *fileName = rage_new WCHAR[wcslen(findData.cFileName) + wcslen(srcDir) + 2];
					lstrcpyW(fileName, srcDir);
					lstrcatW(fileName, L"\\");
					lstrcatW(fileName,findData.cFileName);

					if(PossiblyAddTrack(fileName, database))
					{
						count++;
					}
					delete[] fileName;
				}
			}

			bActive = FindNextFileW(hFind, &findData);
		} // while(bActive)

		FindClose(hFind);
	} // if(hFind != INVALID_HANDLE_VALUE)

	return count;
}

// if idle then kicks off a new scan and returns false,
// otherwise returns false when busy and true when scan is finished.  use GetScanStatus()
// to check if the scan was successful or not.
bool audUserRadioTrackManager::StartMediaScan(bool completeScan)
{
	bool ret = false;

	switch(m_ScanStatus)
	{
	case SCAN_STATE_FAILED:
	case SCAN_STATE_UNSCANNED:
	case SCAN_STATE_IDLE:
		// kick off new scan
		m_ScanStatus = SCAN_STATE_SCANNING;
		m_CompleteScan = completeScan;
		m_Thread = sysIpcCreateThread(audUserRadioTrackManager::RebuildFileDatabaseCB, this, 65536, UserRadioTrackScanThreadPriority, "[GTA Audio] User Music Scan Thread", false, UserMusicScanThreadCpu);
		Assert(m_Thread != sysIpcThreadIdInvalid && "Couldn't create User Music Scan thread!");
		if(m_Thread == sysIpcThreadIdInvalid)
		{
			m_ScanStatus = SCAN_STATE_FAILED;
		}
		else
		{
			//sysIpcResumeThread(m_Thread);	
			ret = true;
		}
		break;
	case SCAN_STATE_SCANNING:
	case SCAN_STATE_SUCCEEDED:
		break;
	}

	return ret;
}

void audUserRadioTrackManager::AbortMediaScan()
{
	m_AbortRequested = true;
}

USERRADIO_PLAYMODE audUserRadioTrackManager::GetRadioMode() const
{
	if(!HasTracks() || audRadioStation::IsInRecoveryMode())
		return USERRADIO_PLAY_MODE_RADIO;
	return (USERRADIO_PLAYMODE)CPauseMenu::GetMenuPreference(PREF_UR_PLAYMODE);
}

void audUserRadioTrackManager::AddTrackToHistory(s32 track)
{
	m_History.AddToHistory(static_cast<u32>(track));
}

bool audUserRadioTrackManager::IsTrackInHistory(s32 track)
{
	if(GetNumTracks() < 3)
	{
		return false;
	}
	// Limit history length to keep two tracks available
	return m_History.IsInHistory(static_cast<u32>(track), GetNumTracks() - 2);
}

#endif
