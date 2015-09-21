#ifndef _SYNTAX_
#define _SYNTAX_

class CSyntax
{
public:
						CSyntax(const char *buffer, size_t size);
						~CSyntax();

	bool				Finished();
	char				ReadNext();

	const char*			ParseToken(size_t *len);
	const char*			ParseName(size_t *len);
	const char*			ParseNumber(size_t *len);

	bool				GetToken(char *buffer, size_t len);

    const char*         ReadUntilNewLine(size_t *len);

	bool				GotoNewLine();
    bool                SkipWhitespace();
	bool				ScanCharacter(char c);
	bool				ScanCharacterEx(char c, bool ignoreName, bool ignoreNumber, bool ignoreNewline);

	size_t		        GetOffset();
	void				SetOffset(unsigned long offset);

	void				Seek(long seek);

private:
	const char*			m_buffer;

	size_t		        m_offset;
	size_t				m_size;
};

#endif //_SYNTAX_