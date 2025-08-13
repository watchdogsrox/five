//
// PlayerCardDataParserHelper.cpp
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

//rage headers
#include "rline/profilestats/rlprofilestatscommon.h"
#include "system/memory.h"
#include "zlib/lz4.h"
#include "zlib/zlib.h"

// FrameWork Headers
#include "fwnet/netchannel.h"

//Game headers
#include "PlayerCardDataParserHelper.h"
#include "Stats/StatsDataMgr.h"
#include "Stats/StatsMgr.h"

NETWORK_OPTIMISATIONS();

RAGE_DEFINE_SUBCHANNEL(net, playercardparser, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_playercardparser

//PURPOSE
//  Creates a compressed buffer with the values of statIds of the local player.
bool PlayerCardDataParserHelper::CreateDataBuffer(const int* statIds, const unsigned numStatIds)
{
	//Create parsable data.
	if (gnetVerify(!m_buffer) && gnetVerify(!m_bufferSize) && gnetVerify(statIds) && gnetVerify(numStatIds > 0))
	{
		//Build buffer.
		BuildDataBuffer(statIds, numStatIds);

		if (gnetVerify(m_buffer))
		{
			//Add compression.
			if (gnetVerifyf(CompressData(), "PlayerCardDataNetParserHelper::CreateDataBuffer( ) - CompressData() - FAILED"))
			{
				gnetDebug1("PlayerCardDataNetParserHelper:: Buffer Generated: m_bufferSize='%u', MAX_NETWORK_PACKET_SIZE='%u'.", m_bufferSize, MAX_NETWORK_PACKET_SIZE);

				gnetAssertf(m_bufferSize < MAX_NETWORK_PACKET_SIZE, "PlayerCardDataNetParserHelper:: Buffer generated: MAX_NETWORK_PACKET_SIZE='%u', compressedSize='%u', ", MAX_NETWORK_PACKET_SIZE, m_bufferSize);

				return true;
			}
		}
	}

	return false;
}

//PURPOSE
//  Decompresses a buffer with the stats values of a certain player into outResults.
bool PlayerCardDataParserHelper::GetBufferResults(u8* inBuffer, const u32 inbufferSize, rlProfileStatsReadResults* outResults, const u32 row, u32& numStatsReceived)

{
	bool result = false;

	gnetAssertf(outResults, "inBuffer is NULL");
	if (!outResults)
		return result;

	gnetAssertf(inBuffer, "inBuffer is NULL");
	if (!inBuffer)
		return result;

	//Get the compression type used.
	u8 compressionType = 0;
	memcpy(&compressionType, inBuffer, COMPRESSION_TYPE_HEADER_SIZE);

	//Get the size of the uncompressed buffer.
	u16 uncompressedBufferSize = 0;
	memcpy(&uncompressedBufferSize, inBuffer+COMPRESSION_TYPE_HEADER_SIZE, SOURCESIZE_HEADER_SIZE);

	gnetAssertf(uncompressedBufferSize < MAX_UMCOMPRESSED_SIZE, "Uncompressed size is too big '%u', max size is '%u'", uncompressedBufferSize, MAX_UMCOMPRESSED_SIZE);

	u16 bufferSize = uncompressedBufferSize;

	u8* bufferdata = 0;
	bufferdata = TryAllocateMemory(bufferSize);
	gnetAssertf(bufferdata, "Failed to allocate dest - size is too big '%u', max size is '%u'", bufferSize, MAX_UMCOMPRESSED_SIZE);

	if (bufferdata)
	{
		//Clear the buffer
		sysMemSet(bufferdata, 0, bufferSize);

		//Our compressed buffer data
		u8* compressedData = inBuffer + PlayerCardDataParserHelper::HEADER_SIZE;

		//Decompress the full buffer.
		if (gnetVerifyf(DecompressData(compressedData, inbufferSize-PlayerCardDataParserHelper::HEADER_SIZE, bufferdata, bufferSize, compressionType), "PlayerCardDataNetParserHelper::GetBufferResults() - DecompressData - FAILED."))
		{
			numStatsReceived = 0;
			u32 parsedbufferSize = 0;

			const CStatsDataMgr& statMgr = CStatsMgr::GetStatsDataMgr();

			result = true;

			while(parsedbufferSize < uncompressedBufferSize && result)
			{
				int keyhash = 0;
				sysMemCpy(&keyhash, bufferdata+parsedbufferSize, sizeof(int)); 
				parsedbufferSize += sizeof(int);

				const sStatDataPtr* ppStat = statMgr.GetStatPtr(keyhash);
				gnetAssertf(ppStat, "Cant find stat with key='%d'", keyhash);

				if (!ppStat)
					result = false;

				if (ppStat)
				{
					const sStatData* pStat = ppStat && ppStat->KeyIsvalid() ? *ppStat : 0;
					gnetAssertf(pStat, "ppStat->KeyIsvalid() with key='%s:%d'", ppStat->GetKeyName(), keyhash);

					if (!pStat)
						result = false;

					if (pStat)
					{
						rlProfileStatsValue localValue;
						result = pStat->ConstWriteOutProfileStat(&localValue);

						if (gnetVerify(result))
						{
							switch (localValue.GetType())
							{
							case RL_PROFILESTATS_TYPE_FLOAT:
								{
									float value = 0.0f;
									sysMemCpy(&value, bufferdata+parsedbufferSize, sizeof(float));
									parsedbufferSize += sizeof(float);

									rlProfileStatsValue statValue;
									statValue.SetFloat(value);

									const int index = outResults->GetStatColumn( keyhash );
									if (gnetVerify(-1 < index))
									{
										if(ReceiveStat(outResults, row, index, statValue))
										{
											numStatsReceived++;
										}
									}
								}
								break;

							case RL_PROFILESTATS_TYPE_INT32:
								{
									int value = 0;
									sysMemCpy(&value, bufferdata+parsedbufferSize, sizeof(int));
									parsedbufferSize += sizeof(int);

									rlProfileStatsValue statValue;
									statValue.SetInt32(value);

									const int index = outResults->GetStatColumn( keyhash );
									if (gnetVerify(-1 < index))
									{
										if(ReceiveStat(outResults, row, index, statValue))
										{
											numStatsReceived++;
										}
									}
								}
								break;

							case RL_PROFILESTATS_TYPE_INT64:
								{
									s64 value = 0;
									sysMemCpy(&value, bufferdata+parsedbufferSize, sizeof(s64));
									parsedbufferSize += sizeof(s64);

									rlProfileStatsValue statValue;
									statValue.SetInt64(value);

									const int index = outResults->GetStatColumn( keyhash );
									if (gnetVerify(-1 < index))
									{
										if(ReceiveStat(outResults, row, index, statValue))
										{
											numStatsReceived++;
										}
									}
								}
								break;

							default:
								result = false;
								gnetAssertf(0, "localValue.GetType()='%d'", localValue.GetType());
								break;
							}
						}
					}
				}
			}

			gnetDebug1("PlayerCardDataNetParserHelper::GetBufferResults - NumStatIds='%u', numStatsReceived='%u'.", outResults->GetNumStatIds(), numStatsReceived);
		}

		TryDeAllocateMemory(bufferdata);
	}

	return result;
}

bool
PlayerCardDataParserHelper::ReceiveStat(rlProfileStatsReadResults* outResults, const u32 row, const int index, const rlProfileStatsValue& statValue)

{
	rlProfileStatsValue* resultVal = const_cast<rlProfileStatsValue*>(outResults->GetValue(row, index));

	if (resultVal)
	{
		gnetDebug1("ReceiveStat() - StatId='%d', index='%d'.", outResults->GetStatId(index), index);

		*resultVal = statValue;
		return true;
	}
	else
	{
		gnetError("PlayerCardDataNetParserHelper::ReceiveStat() - FAILED - index='%d'.", index);
		gnetAssertf(false, "PlayerCardDataNetParserHelper::ReceiveStat() - FAILED - index='%d'.", index);
	}

	return false;
}

bool PlayerCardDataParserHelper::CompressData( )
{
	if (gnetVerify(m_buffer) && gnetVerify(m_bufferSize))
	{
		int compSize = 0;
		u32 destSize = 0;

		if (m_lz4Compression)
			destSize = LZ4_compressBound(m_bufferSize)+PlayerCardDataParserHelper::HEADER_SIZE;
		else
			destSize = m_bufferSize;

		u8* dest = TryAllocateMemory(destSize);
		if(dest)
		{
			sysMemSet(dest, 0, destSize);

			if (m_lz4Compression)
			{
				compSize = LZ4_compress(reinterpret_cast<const char*>(m_buffer), reinterpret_cast<char*>(dest), m_bufferSize);
			}
			else
			{
				z_stream c_stream;
				memset(&c_stream,0,sizeof(c_stream));

				if (deflateInit2(&c_stream,Z_BEST_COMPRESSION,Z_DEFLATED,-MAX_WBITS,MAX_MEM_LEVEL,Z_DEFAULT_STRATEGY) < 0)
				{
					gnetError("PlayerCardDataNetParserHelper::CompressData() - FAILED deflateInit2()");
					TryDeAllocateMemory(dest);
					return 0;
				}

				c_stream.next_in   = m_buffer;
				c_stream.avail_in  = m_bufferSize;
				c_stream.next_out  = dest;
				c_stream.avail_out = destSize;

				if (deflate(&c_stream, Z_FINISH) != Z_STREAM_END)
				{
					gnetError("PlayerCardDataNetParserHelper::CompressData() - FAILED deflate()");
					TryDeAllocateMemory(dest);
					return 0;
				}

				deflateEnd(&c_stream);
				compSize = (int) (destSize - c_stream.avail_out);
			}



			if (gnetVerifyf(0 < compSize, "Compression failed"))
			{
				u8 compressionType = m_lz4Compression ? COMPRESSION_LZ4 : COMPRESSION_ZLIB;

				if((compSize+PlayerCardDataParserHelper::HEADER_SIZE) > m_bufferSize)
				{
					Warningf("The compressed buffer + header is bigger than the original buffer : m_bufferSize=%d   compSize=%d    HEADER_SIZE=%d", m_bufferSize, compSize, PlayerCardDataParserHelper::HEADER_SIZE);
					// We'll try to reallocate the buffer if the compressed size fits into a packet
					if(gnetVerifyf( compSize+PlayerCardDataParserHelper::HEADER_SIZE < MAX_NETWORK_PACKET_SIZE, "Compressed buffer '%d' wont fit in a packet '%d'.", compSize+PlayerCardDataParserHelper::HEADER_SIZE, MAX_NETWORK_PACKET_SIZE))
					{
						gnetDebug1("The compressed buffer will fit into a packet, reallocating from %d to %d", m_bufferSize, compSize+PlayerCardDataParserHelper::HEADER_SIZE);
						TryDeAllocateMemory(m_buffer);
						m_buffer=TryAllocateMemory(compSize+PlayerCardDataParserHelper::HEADER_SIZE);
					}
				}
				else
				{
					//Clear the buffer
					sysMemSet(m_buffer, 0, m_bufferSize);
				}

				// Always make sure it fits into a packet
				if(gnetVerify(m_buffer) && gnetVerifyf( compSize+PlayerCardDataParserHelper::HEADER_SIZE < MAX_NETWORK_PACKET_SIZE, "Compressed buffer wont '%d' fit in a packet '%d'.", compSize+PlayerCardDataParserHelper::HEADER_SIZE, MAX_NETWORK_PACKET_SIZE))
				{
					memcpy(m_buffer, &compressionType, COMPRESSION_TYPE_HEADER_SIZE);
					memcpy(m_buffer+COMPRESSION_TYPE_HEADER_SIZE, &m_bufferSize, SOURCESIZE_HEADER_SIZE);
					memcpy(m_buffer+PlayerCardDataParserHelper::HEADER_SIZE, dest, compSize);

					m_bufferSize = static_cast<u16>(compSize+PlayerCardDataParserHelper::HEADER_SIZE);

					gnetDebug1("CompressData() - m_bufferSize='%u', compSize='%d'.", m_bufferSize, compSize);

					gnetAssertf(m_bufferSize > 0, "Zero size m_bufferSize '%u'", m_bufferSize);
					gnetAssertf(m_bufferSize < MAX_UMCOMPRESSED_SIZE, "Max size reached '%u'", m_bufferSize);

					TryDeAllocateMemory(dest);

					return true;
				}
			}

			TryDeAllocateMemory(dest);
		}
	}

	gnetError("PlayerCardDataNetParserHelper::CompressData() FAILED");

	return false;
}

bool PlayerCardDataParserHelper::DecompressData(u8* source, const u32 sourcesize, u8* dest, const u32 destinationSize, const u32 compressionType)
{
	const bool  lz4Compression = (compressionType == COMPRESSION_LZ4);
	const bool zlibCompression = (compressionType == COMPRESSION_ZLIB);

	int returnValue = 0;

	gnetDebug1("DecompressData() - sourcesize='%u', destinationSize='%u'.", sourcesize, destinationSize);

	if (lz4Compression)
	{
		returnValue = LZ4_decompress_fast(reinterpret_cast<const char*>(source), reinterpret_cast<char*>(dest), destinationSize);
	}
	else if(gnetVerify(zlibCompression))
	{
		z_stream c_stream;
		memset(&c_stream,0,sizeof(c_stream));

		if (inflateInit2(&c_stream,-MAX_WBITS) < 0)
		{
			gnetError("PlayerCardDataNetParserHelper::DecompressData() - Savegame InflateInit failed - '%s'.", c_stream.msg);
			inflateEnd(&c_stream);
			return false;
		}

		c_stream.next_in   = source;
		c_stream.avail_in  = sourcesize;
		c_stream.next_out  = dest;
		c_stream.avail_out = destinationSize;

		if (inflate(&c_stream, Z_FINISH) < 0)
		{
			gnetError("PlayerCardDataNetParserHelper::DecompressData() - Failed to inflate savegame data - '%s'.", c_stream.msg);
			inflateEnd(&c_stream);
			return false;
		}
		inflateEnd(&c_stream);

		returnValue = (int) (destinationSize - c_stream.avail_out);
	}

	if(returnValue <= 0)
	{
		gnetError("PlayerCardDataNetParserHelper::DecompressData() - FAILED - LZ4_decompress_fast FAILED - returnValue='%d', sourcesize='%u'", returnValue, sourcesize);
		return false;
	}

	return true;
}

void PlayerCardDataParserHelper::BuildDataBuffer(const int* statIds, const int numStatIds)
{
	u16 totalSize = 0;

	const CStatsDataMgr& statMgr = CStatsMgr::GetStatsDataMgr();

	//Calculate destination Size
	for (int i=0; i<numStatIds; i++)
	{
		const int keyhash = statIds[i];

		const sStatDataPtr* ppStat = statMgr.GetStatPtr(keyhash);
		gnetAssertf(ppStat, "Cant find stat with key='%d'", keyhash);

		if (ppStat)
		{
			const sStatData* pStat = ppStat && ppStat->KeyIsvalid() ? *ppStat : 0;
			gnetAssertf(pStat, "ppStat->KeyIsvalid() with key='%s:%d'", ppStat->GetKeyName(), keyhash);

			if (pStat)
			{
				rlProfileStatsValue localValue;
				if (gnetVerify(pStat->ConstWriteOutProfileStat(&localValue)))
				{
					switch (localValue.GetType())
					{
					case RL_PROFILESTATS_TYPE_FLOAT:
						{
							totalSize += sizeof(int);
							totalSize += sizeof(float);
						}
						break;

					case RL_PROFILESTATS_TYPE_INT32:
						{
							totalSize += sizeof(int);
							totalSize += sizeof(int);
						}
						break;

					case RL_PROFILESTATS_TYPE_INT64:
						{
							totalSize += sizeof(int);
							totalSize += sizeof(s64);
						}
						break;

					default:
						gnetAssertf(0, "localValue.GetType()='%d'", localValue.GetType());
						break;
					}
				}
			}
		}
	}

	gnetDebug1("Trying to allocate Buffer size='%u', numStatIds='%d'", totalSize, numStatIds);

	gnetAssert(!m_buffer);

	totalSize = (totalSize < MIN_UMCOMPRESSED_SIZE ? MIN_UMCOMPRESSED_SIZE : totalSize);

	m_buffer = TryAllocateMemory(totalSize);
	gnetAssertf(m_buffer, "Failed to allocate '%u'.", totalSize);

	if (m_buffer)
	{
		//clear buffer
		sysMemSet(m_buffer, 0, totalSize);

		m_bufferSize = 0;

		//Populate m_buffer
		for (int i=0; i<numStatIds && gnetVerify(m_bufferSize<=totalSize); i++)
		{
			const int keyhash = statIds[i];

			const sStatDataPtr* ppStat = statMgr.GetStatPtr(keyhash);
			gnetAssertf(ppStat, "Cant find stat with key='%d'", keyhash);

			if (ppStat)
			{
				const sStatData* pStat = ppStat && ppStat->KeyIsvalid() ? *ppStat : 0;
				gnetAssertf(pStat, "ppStat->KeyIsvalid() with key='%s:%d'", ppStat->GetKeyName(), keyhash);

				if (pStat)
				{
					rlProfileStatsValue localValue;
					if (gnetVerify(pStat->ConstWriteOutProfileStat(&localValue)))
					{
						switch (localValue.GetType())
						{
						case RL_PROFILESTATS_TYPE_FLOAT:
							{
								float value = localValue.GetFloat();
								sysMemCpy(m_buffer+m_bufferSize, &keyhash, sizeof(int)); m_bufferSize += sizeof(int);
								sysMemCpy(m_buffer+m_bufferSize, &value, sizeof(value)); m_bufferSize += sizeof(value);
							}
							break;

						case RL_PROFILESTATS_TYPE_INT32:
							{
								int value = localValue.GetInt32();
								sysMemCpy(m_buffer+m_bufferSize, &keyhash, sizeof(int)); m_bufferSize += sizeof(int);
								sysMemCpy(m_buffer+m_bufferSize, &value, sizeof(value)); m_bufferSize += sizeof(value);
							}
							break;

						case RL_PROFILESTATS_TYPE_INT64:
							{
								s64 value = localValue.GetInt64();
								sysMemCpy(m_buffer+m_bufferSize, &keyhash, sizeof(int)); m_bufferSize += sizeof(int);
								sysMemCpy(m_buffer+m_bufferSize, &value, sizeof(value)); m_bufferSize += sizeof(value);
							}
							break;

						default:
							gnetAssertf(0, "localValue.GetType()='%d'", localValue.GetType());
							break;
						}
					}
				}
			}
		}
	}
};

// --- Memory ------------------------------------------------------------------------------------

u8* PlayerCardDataParserHelper::TryAllocateMemory(const u32 size, const u32 alignment)
{
	sysMemAllocator& oldAllocator = sysMemAllocator::GetCurrent();
	sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());
	u8* data = reinterpret_cast<u8*>(sysMemAllocator::GetCurrent().TryAllocate(size, alignment));
	sysMemAllocator::SetCurrent(oldAllocator);

	return data; 
}

void PlayerCardDataParserHelper::TryDeAllocateMemory(void* address)
{
	if(!address)
		return;

	sysMemAllocator& oldAllocator = sysMemAllocator::GetCurrent();
	sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());
	sysMemAllocator::GetCurrent().Free(address);
	sysMemAllocator::SetCurrent(oldAllocator);
}

// eof

