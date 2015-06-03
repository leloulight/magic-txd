/* You can write ONE header per C BLOCk using these macros */
#define SKIP_HEADER()\
	uint32 bytesWritten = 0;\
	uint32 headerPos = rw.tellp();\
    rw.seekp(0x0C, std::ios::cur);

#define WRITE_HEADER(chunkType)\
	uint32 oldPos = rw.tellp();\
    rw.seekp(headerPos, std::ios::beg);\
	header.setType( (chunkType) );\
	header.setLength( bytesWritten );\
	bytesWritten += header.write(rw);\
	writtenBytesReturn = bytesWritten;\
    rw.seekp(oldPos, std::ios::beg);

inline void writeStringIntoBufferSafe( rw::Interface *engineInterface, const std::string& theString, char *buf, size_t bufSize, const std::string& texName, const std::string& dbgName )
{
    size_t nameLen = theString.size();

    if (nameLen >= bufSize)
    {
        engineInterface->PushWarning( "texture " + texName + " has been written using truncated " + dbgName );

        nameLen = bufSize - 1;
    }

    memcpy( buf, theString.c_str(), nameLen );

    // Pad with zeroes (which will also zero-terminate).
    memset( buf + nameLen, 0, bufSize - nameLen );
}

inline rw::uint32 writePartialBlockSafe( rw::BlockProvider& outputProvider, const void *srcData, rw::uint32 srcDataSize, rw::uint32 streamSize )
{
    rw::uint32 streamWriteCount = std::min(srcDataSize, streamSize);

    outputProvider.write( srcData, streamWriteCount );

    rw::uint32 writeCount = streamWriteCount;

    // Write the remainder, if required.
    if (srcDataSize < streamSize)
    {
        rw::uint32 leftDataSize = ( streamSize - srcDataSize );

        for ( rw::uint32 n = 0; n < leftDataSize; n++ )
        {
            outputProvider.writeUInt8( 0 );
        }

        writeCount += leftDataSize;
    }

    return writeCount;
}