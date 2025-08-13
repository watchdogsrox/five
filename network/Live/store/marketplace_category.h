//
// marketplace_category.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef MARKETPLACE_CATEGORY_H
#define MARKETPLACE_CATEGORY_H

#if __PPU

#include <string.h>
#include "string/string.h"

namespace rage
{
	//PURPOSE
	// Represents a category. Categories can have products for download.
	class CategoryInfo
	{
		friend class rlMarketplace;

	public:
		static const unsigned int MAX_TEXT_LENGTH = 30;

	private:
		char  m_Id[MAX_TEXT_LENGTH];
		char  m_ParentId[MAX_TEXT_LENGTH];
		unsigned  m_CountOfProducts;
		bool  m_Enumerate;
		CategoryInfo*  m_Next;

	public:

		CategoryInfo() : m_CountOfProducts(0),
						 m_Enumerate(true),
						 m_Next(NULL)
		{
			memset(m_Id, 0, MAX_TEXT_LENGTH);
			memset(m_ParentId, 0, MAX_TEXT_LENGTH);
		}
		~CategoryInfo()
		{
		}

		CategoryInfo* Next() { return m_Next ? m_Next : NULL; }
		const CategoryInfo* Next() const { return m_Next ? m_Next : NULL; }

		bool  GetEnumerate() const { return m_Enumerate; }
		void  SetEnumerate(bool bEnumerate)  { m_Enumerate = bEnumerate; }

		unsigned  GetCountOfProducts() const { return m_CountOfProducts; }
		void  SetCountOfProducts(unsigned CountOfProducts)  { m_CountOfProducts = CountOfProducts; }

		const char*  GetId()       const { return ('\0' != m_Id ? m_Id : "(null)"); }
		const char*  GetParentId() const { return ('\0' != m_ParentId ? m_ParentId : "(null)"); }

		void  SetId(const char* id)
		{
			if(AssertVerify(id) && AssertVerify(0 < strlen(id) && MAX_TEXT_LENGTH > strlen(id)))
			{
				formatf(m_Id, MAX_TEXT_LENGTH, id);
			}
		}

		void  SetParentId(const char* id)
		{
			if(AssertVerify(id) && (0 < strlen(id) && MAX_TEXT_LENGTH > strlen(id)))
			{
				formatf(m_ParentId, MAX_TEXT_LENGTH, id);
			}
		}

		void Copy(const CategoryInfo& cat)
		{
			SetId(cat.m_Id);
			SetParentId(cat.m_ParentId);
			m_CountOfProducts = cat.m_CountOfProducts;
			m_Enumerate = cat.m_Enumerate;
		}
	};

} // namespace rage

#endif // __PPU

#endif // MARKETPLACE_CATEGORY_H

// EOF
