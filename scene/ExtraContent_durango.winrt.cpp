#if RSG_DURANGO
#include "file/asset.h"
#include "ExtraContent.h"
#include "scene/dlc_channel.h"
//WinRT Headers
#include "system/new_winrt.h"
#include <xdk.h>
#include <collection.h>
#include <string.h>
#if _XDK_VER >= 9586
namespace rage
{
	namespace dns
	{
		using namespace Windows;
		using namespace Windows::Foundation::Collections;
		using namespace Microsoft;
		using namespace Windows::Xbox::System;
		using namespace Windows::Xbox::ApplicationModel;
		using namespace Microsoft::Xbox::Services;
		using namespace Platform::Collections;
		using namespace Windows::Xbox::Management;
		static Deployment::IDownloadableContentPackageManager^ m_dlcPackageManager;
		static IVectorView<Deployment::IDownloadableContentPackage^> ^ InstalledPackages;
		static IVector<Deployment::IDownloadableContentPackage^> ^ MountedPackages;
		static Windows::Foundation::EventRegistrationToken m_eventToken_PackageInstalled;
	}
}
#endif

using namespace rage::dns;



void CExtraContentManager::OnDurangoContentDownloadCompleted()
{
	EXTRACONTENT.m_enumerateOnUpdate = true;
}

void CExtraContentManager::InitDurangoContent()
{
	MountedPackages = ref new Vector< Deployment::IDownloadableContentPackage^ >();
	m_dlcPackageManager = ref new Deployment::DownloadableContentPackageManager();
	m_eventToken_PackageInstalled = m_dlcPackageManager->DownloadableContentPackageInstallCompleted
		+=	ref new Deployment::DownloadableContentPackageInstallCompletedEventHandler(OnDurangoContentDownloadCompleted);
}

void CExtraContentManager::ShutdownDurangoContent()
{
	for( auto itrMountedPackages = MountedPackages->First(); itrMountedPackages->HasCurrent; itrMountedPackages->MoveNext() )
	{
		auto currentPackage = itrMountedPackages->Current;
		dlcDisplayf("Unmounting pack : %ls",currentPackage->DisplayName->Data());
		currentPackage->Unmount();
	}
	MountedPackages->Clear();
}

void CExtraContentManager::EnumerateDurangoContent()
{
	IVector< Deployment::IDownloadableContentPackage^ >^ vMountedPackages = ref new Vector< Deployment::IDownloadableContentPackage^ >();
	try
	{
		for( auto itrPackageToAdd = MountedPackages->First(); itrPackageToAdd->HasCurrent; itrPackageToAdd->MoveNext() )
		{
			if( itrPackageToAdd->Current->IsMounted )
			{
				vMountedPackages->Append( itrPackageToAdd->Current );
			}
		}
		MountedPackages->Clear();
		InstalledPackages = m_dlcPackageManager->FindPackages(Deployment::InstalledPackagesFilter::AllDownloadableContentOnly);
		dlcDisplayf("Found %d packages!", InstalledPackages->Size);
		for( auto itrMountedPackage = vMountedPackages->First(); itrMountedPackage->HasCurrent; itrMountedPackage->MoveNext() )
		{
			MountedPackages->Append( itrMountedPackage->Current );
		}
		for( auto itrInstalledPackage = InstalledPackages->First(); itrInstalledPackage->HasCurrent; itrInstalledPackage->MoveNext() )
		{
			bool bAlreadyAdded = false;
			for( auto itrMountedPackage = vMountedPackages->First(); itrMountedPackage->HasCurrent; itrMountedPackage->MoveNext() )
			{
				if( itrInstalledPackage->Current->ContentId->Equals( itrMountedPackage->Current->ContentId ) )
				{
					bAlreadyAdded = true;
					break;
				}
			}

			if( !bAlreadyAdded )
			{
				MountedPackages->Append( itrInstalledPackage->Current );
			}
		}
	}
	catch (Platform::Exception^ e)
	{
		MountedPackages = vMountedPackages;
		dlcErrorf("DLC Enumeration failed! resultcode: 0x%x", e->HResult);

	}
}

void CExtraContentManager::UpdateContentArrayDurango()
{
	try
	{
		for( auto itrMountedPackage = MountedPackages->First(); itrMountedPackage->HasCurrent; itrMountedPackage->MoveNext() )
		{
			auto currentPackage = itrMountedPackage->Current;
			dlcDisplayf("Checking DLC pack : %ls",currentPackage->DisplayName->Data());
			if(!currentPackage->IsMounted)
			{
				dlcDisplayf("Mounting pack : %ls",currentPackage->DisplayName->Data());
				currentPackage->Mount();
				Platform::String^ mountPoint1 = currentPackage->MountPath;
				char mountPointC[RAGE_MAX_PATH];
				size_t sizeInBytes = ((mountPoint1->Length()+1)*2);
				wcstombs(mountPointC,mountPoint1->Data(),sizeInBytes);
				if(currentPackage->IsMounted)
				{
					if(!AddDurangoContent(mountPointC))
					{
						currentPackage->Unmount();
					}
				}	
			}
			else
			{
				dlcDisplayf("DLC pack is already mounted: %ls",currentPackage->DisplayName->Data());
			}	
		}
	}
	catch (Platform::Exception^ e)
	{
		dlcErrorf("DLC Enumeration failed! resultcode: 0x%x", e->HResult);
	}
}

bool CExtraContentManager::AddDurangoContent(const char *path)
{
	char filePathPreFixed[RAGE_MAX_PATH] = { 0 };
	formatf(filePathPreFixed,"%s\\dlc.rpf",path);
	if(Verifyf(ASSET.Exists(filePathPreFixed,""),"DLC pack not built correctly, DLC.rpf must be inside the package"))
	{
		CMountableContent mount;
		mount.SetFilename(filePathPreFixed);
		mount.SetUsesPackFile(false);
		mount.SetPrimaryDeviceType(CMountableContent::DT_PACKFILE);
		mount.SetNameHash(mount.GetFilename());
		AddContent(mount);
		return true;
	}
	return false;
}
#endif