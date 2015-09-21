#include "StdInc.h"

CSyntax::CSyntax(const char *buffer, size_t size)
{
	m_buffer = buffer;

	m_offset = 0;
	m_size = size;
}

CSyntax::~CSyntax()
{
}

inline bool IsName(char c)
{
	switch (c)
	{
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
	case '_':
		return true;
	}

	return false;
}

inline bool IsNumber(char c)
{
	switch (c)
	{
    case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': case '0': case '.': case '-':
		return true;
	}

	return false;
}

inline bool IsSpace(char c)
{
	switch (c)
	{
	case ' ': case '\t':
		return true;
	}

	return false;
}

inline bool IsNewline(char c)
{
	switch (c)
	{
	case '\n':
	case '\r':
		return true;
	}

	return false;
}

bool CSyntax::Finished()
{
	return m_offset == m_size;
}

char CSyntax::ReadNext()
{
	if ( Finished() )
		return 0;

	return *(m_buffer + m_offset++);
}

const char*	CSyntax::ParseToken(size_t *len)
{
	char read;

	do
	{
		if ( Finished() )
			return NULL;

	} while ( ( read = ReadNext() ) < 0x11 );

	if ( IsName(read) )
	{
		Seek( -1 );

		return ParseName(len);
	}

	if ( IsNumber(read) )
	{
		Seek( -1 );

		return ParseNumber(len);
	}

	*len = 1;
	return m_buffer + m_offset - 1;
}

const char* CSyntax::ParseName(size_t *len)
{
	size_t offset = GetOffset();
	const char *name = m_buffer + offset;
	char read = ReadNext();

	while ( IsName( read ) || IsNumber( read ) )
    {
		read = ReadNext();
    }

	if ( !Finished() )
    {
		Seek( -1 );
    }

	*len = GetOffset() - offset;
	return name;
}

const char* CSyntax::ParseNumber(size_t *len)
{
	size_t offset = GetOffset();
	const char *name = m_buffer + offset;

	while ( IsNumber( ReadNext() ) );

	if ( !Finished() )
		Seek( -1 );

	*len = GetOffset() - offset;
	return name;
}

bool CSyntax::GetToken(char *buffer, size_t len)
{
	size_t size;
	const char *token = ParseToken( &size );

	if ( !token )
		return false;

	size = std::min(len, size);

	memcpy( buffer, token, size );

	buffer[size] = 0;
	return true;
}

const char* CSyntax::ReadUntilNewLine(size_t *len)
{
    const char *curOff = m_buffer + this->GetOffset();
    size_t theLength = 0;

    while ( !this->Finished() )
    {
        char outchar = this->ReadNext();

        if ( outchar == '\r' || outchar == '\n' )
            break;

        theLength++;
    }

    if ( len )
    {
        *len = theLength;
    }
    return curOff;
}

bool CSyntax::GotoNewLine()
{
    bool skipped = SkipWhitespace();

    if ( !skipped )
        return false;

	char read = this->ReadNext();

	if ( read == '\r' )
    {
		read = ReadNext();
    }

	return read == '\n';
}

bool CSyntax::SkipWhitespace()
{
    if ( this->Finished() )
    {
        return false;
    }

	while ( IsSpace( ReadNext() ) );

    this->Seek( -1 );

    return true;
}

bool CSyntax::ScanCharacter(char c)
{
	while ( !Finished() )
	{
		if ( ReadNext() == c )
			return true;
	}

	return false;
}

bool CSyntax::ScanCharacterEx(char c, bool ignoreName, bool ignoreNumber, bool ignoreNewline)
{
	while ( !Finished() )
	{
		char read = ReadNext();

		if ( read == c )
			return true;

		if ( !ignoreName && IsName( read ) )
			return false;

		if ( !ignoreNumber && IsNumber( read ) )
			return false;

		if ( !ignoreNewline && IsNewline( read ) )
			return false;
	}

	return false;
}

size_t CSyntax::GetOffset()
{
	return m_offset;
}

void CSyntax::SetOffset(unsigned long offset)
{
	m_offset = offset;
}

void CSyntax::Seek(long seek)
{
	m_offset = std::max(std::min((size_t)( m_offset + seek ), m_size), (size_t)0);
}
