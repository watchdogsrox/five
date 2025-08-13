//
// marketplace_asset.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef MARKETPLACE_ASSET_H
#define MARKETPLACE_ASSET_H

#if __XENON

namespace rage
{
	//PURPOSE
	// Stores info about assets.
	class AssetInfo
	{
		friend class rlMarketplace;

	private:
		u32  m_AssetID;
		u32  m_Quantity;

		AssetInfo*  m_Next;

	public:
		AssetInfo()
			: m_AssetID(0),
			  m_Quantity(0),
			  m_Next(NULL)
		{
		}
		~AssetInfo()
		{
		}

		void Copy(const AssetInfo& offer)
		{
			m_AssetID   = offer.m_AssetID;
			m_Quantity  = offer.m_Quantity;
		}

		AssetInfo* Next() { return m_Next ? m_Next : NULL; }
		const AssetInfo* Next() const { return m_Next ? m_Next : NULL; }

		u32  GetAssetId()  const { return m_AssetID; }
		u32  GetQuantity() const { return m_Quantity; }

		void  SetAssetId(const u32  assetID)    { m_AssetID = assetID; }
		void  SetQuantity(const u32  quantity)  { m_Quantity = quantity; }
	};

} // namespace rage

#endif // __XENON

#endif // MARKETPLACE_ASSET_H

// EOF
