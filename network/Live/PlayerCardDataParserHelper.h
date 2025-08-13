//
// PlayerCardDataParserHelper.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef PLAYER_CARD_PARSER_HELPER_H
#define PLAYER_CARD_PARSER_HELPER_H

//rage
#include "atl/array.h"
#include "rline/rlgamerinfo.h"

//game includes
#include "net/message.h"

//Forward Declarations
namespace rage { class rlProfileStatsReadResults; };

//PURPOSE
//  Helper class to create a buffer with all player card stat ids and values.
class PlayerCardDataParserHelper
{
private:
	//4 byte header for LZ4 compression/decompression.
	static const u32 COMPRESSION_TYPE_HEADER_SIZE = 1;

	//4 byte header for LZ4 compression/decompression.
	static const u32 SOURCESIZE_HEADER_SIZE = 2;

	//4 byte header for LZ4 compression/decompression.
	static const u32 HEADER_SIZE = COMPRESSION_TYPE_HEADER_SIZE+SOURCESIZE_HEADER_SIZE;

	//Max uncompressed buffer size.
	static const u32 MAX_UMCOMPRESSED_SIZE = 1024; 
	static const u32 MIN_UMCOMPRESSED_SIZE =  512; 

	//Max buffer size to fit into 1 packet.
	static const u32 MAX_NETWORK_PACKET_SIZE = netMessage::MAX_BYTE_SIZEOF_PAYLOAD - sizeof(u32) - sizeof(rlGamerId);

	//Defines for the compression types
	static const u8 COMPRESSION_ZLIB = 2;
	static const u8 COMPRESSION_LZ4 = 1;


public:
	 PlayerCardDataParserHelper(const bool lz4Compression) 
		 : m_lz4Compression(lz4Compression)
		 , m_buffer(0)
		 , m_bufferSize(0)
	 {;}
	~PlayerCardDataParserHelper( ) { TryDeAllocateMemory(m_buffer);}

	//PURPOSE
	//  Creates a compressed buffer with the values of statIds of the local player.
	bool CreateDataBuffer(const int* statIds, const unsigned numStatIds);
	u8*         Getbuffer() const {return m_buffer;}
	u32     GetBufferSize() const {return m_bufferSize;}

	//PURPOSE
	//  Decompresses a buffer with the stats values of a certain player into outResults.
	bool GetBufferResults(u8* inBuffer, const u32 inbufferSize, rlProfileStatsReadResults* outResults, const u32 row, u32& numStatsReceived);

private:
	bool          ReceiveStat(rlProfileStatsReadResults* outResults, const u32 row, const int index, const rlProfileStatsValue& statValue);
	bool         CompressData( );
	bool       DecompressData(u8* source, const u32 sourcesize, u8* dest, const u32 destinationSize, const u32 compressionType);
	void      BuildDataBuffer(const int* statIds, const int numStatIds);
	u8*     TryAllocateMemory(const u32 size, const u32 alignment = 16);
	void  TryDeAllocateMemory(void* address);

private:
	bool m_lz4Compression;
	u8*  m_buffer;
	u16  m_bufferSize;
};

#endif // PLAYER_CARD_PARSER_HELPER_H

// eof

