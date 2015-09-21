/*	CSV reading, MYSQL type environment (as these files are MYSQL database backups)
	General Notice: Always close .csv files if you dont need them anymore.
*/

#ifndef _CSV_
#define _CSV_

#define CSVERR_FILEOPEN 0

#define CSV_SEPERATION 44

typedef unsigned char **CSV_ROW;

class CCSV
{
    friend CCSV*    CreateCSV( const char *filename );
	friend CCSV*	LoadCSV(const char *filename);

public:
					CCSV( CFile *ioptr, bool closeIOOnDelete = true );
					~CCSV();

	unsigned int	GetCurrentLine();

    void            WriteNextRow( const std::vector <std::string>& items );
	bool			ReadNextRow();
	unsigned int	GetItemCount();
	const char*		GetRowItem(unsigned int id);
	const char**	GetRow();
	void			FreeRow();

    void            ParsingError( const char *msg );
    
    // Helper functions.
    bool            ExpectTokenCount( unsigned int numTokens );

private:
	CFile*		    m_file;
    bool            m_closeIOOnDelete;
	unsigned int	m_currentLine;
	unsigned int	m_numItems;
	char**			m_row;
};

// Global defines
CCSV*   CreateCSV( const char *filename );
CCSV*	LoadCSV(const char *filename);

#endif
