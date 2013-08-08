#include <stdlib.h>
#include <alloc.h>
#include <mem.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
#include "EMDX.H"

	/**********************************
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	**********************************/

typedef struct tagHFILEHEADER{
	unsigned char Checksum;
	long FirstHolesTablePos;
	unsigned int HolesTableSize;
	}HFILEHEADER;

typedef struct tagHOLERECORD{
	long HolePos;
	long HoleSize;
	}HOLERECORD;

typedef struct tagHOLESTABLEHEADER{
	unsigned char Checksum;
	unsigned int NumUsed;
	long NextTablePos;
	HOLERECORD Record[1]; // Actual size is ~HolesTableSize~, defined in header ...
	}HOLESTABLEHEADER;

	/**********************************
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	**********************************/

class THFile:public TFile
{
private:
	HFILEHEADER HeaderInfo;

	// Error detection functions ...

	void SetHeaderChecksum();
	int TestHeaderChecksum();
	void SetHolesTableChecksum(void _PTR ATable);
	int TestHolesTableChecksum(void _PTR ATable);

	// Calculation functions ...

	unsigned int GetHeaderBlockSize(void);
	unsigned int GetHolesTableHeaderBlockSize(void);
	unsigned int GetHolesTableItemBlockSize(void);
	void SetHolesTableSize(unsigned int AHolesTableSize);
	unsigned int GetHolesTableSize(void);
	unsigned int GetHolesTableBlockSize(void);
	void SetFirstHolesTablePos(long APos);
	long int GetFirstHolesTablePos(void);

	// Memory functions ...

	void _PTR AllocateHolesTableBlock(void);
	void FreeHolesTableBlock(void _PTRREF ATable);

	// File access functions ...

	void WriteHeader(void);
	void ReadHeader(void);
	void WriteHolesTable(void _PTR ATable,long APos);
	void ReadHolesTable(void _PTR ATable,long APos);
	long WriteNewHolesTable(void _PTR ATable);

	// Process functions ...

	void ResetHolesTable(void _PTR ATable);
	void SetHolesTableNumUsed(void _PTR ATable,unsigned int ANum);
	unsigned int GetHolesTableNumUsed(void _PTR ATable);
	void SetHolesTableNextPos(void _PTR ATable,long APos);
	long GetHolesTableNextPos(void _PTR ATable);
	void SetHolesTableItem(void _PTR ATable,unsigned int AItemNo,long AHolePos,long AHoleSize);
	long GetHolePos(void _PTR ATable,unsigned int AItemNo);
	long GetHoleSize(void _PTR ATable,unsigned int AItemNo);
public:

	// User functions ...

	THFile( const char _PTR AName,
			unsigned int AHolesTableSize);
	THFile( const char _PTR AName);
	~THFile(void);

};

THFile::THFile( const char _PTR AName,
				unsigned int AHolesTableSize):TFile(AName,1)
{
	SetFirstHolesTablePos(-1L);
	SetHolesTableSize(AHolesTableSize);
	WriteHeader();
}

THFile::THFile( const char _PTR AName):TFile(AName,0)
{
	ReadHeader();
}

THFile::~THFile(void)
{
	WriteHeader();
	Free();
}

void THFile::SetHeaderChecksum()
{
	HeaderInfo.Checksum = 0;
	HeaderInfo.Checksum = CalcBlockChecksum((void _PTR) &HeaderInfo,sizeof(HeaderInfo));
}

int THFile::TestHeaderChecksum()
{
	return (CalcBlockChecksum((void _PTR) &HeaderInfo,sizeof(HeaderInfo)) == 0);
}

void THFile::SetHolesTableChecksum(void _PTR ATable)
{
	((HOLESTABLEHEADER _PTR)ATable)->Checksum = 0;
	((HOLESTABLEHEADER _PTR)ATable)->Checksum = CalcBlockChecksum(ATable,GetHolesTableBlockSize());
}

int THFile::TestHolesTableChecksum(void _PTR ATable)
{
	return (CalcBlockChecksum(ATable,GetHolesTableBlockSize()) == 0);
}

void THFile::WriteHeader(void)
{
	SetHeaderChecksum();
	Write((void _PTR) &HeaderInfo,sizeof(HeaderInfo),0);
}

void THFile::ReadHeader(void)
{
	Read((void _PTR) &HeaderInfo,sizeof(HeaderInfo),0);
	if (!TestHeaderChecksum())
		SetError(errBADDATA);
}

void THFile::WriteHolesTable(void _PTR ATable,long APos)
{
	SetHolesTableChecksum(ATable);
	Write(ATable,GetHolesTableBlockSize(),APos);
}

void THFile::ReadHolesTable(void _PTR ATable,long APos)
{
	Read(ATable,GetHolesTableBlockSize(),APos);
	if (!TestHolesTableChecksum(ATable))
		SetError(errBADDATA);
}

long THFile::WriteNewHolesTable(void _PTR ATable)
{
	long Pos = Size();
	SetHolesTableNextPos(ATable,-1L);
	WriteHolesTable(ATable,Pos);
	return Pos;
}

unsigned int THFile::GetHeaderBlockSize(void)
{
	return (sizeof(HFILEHEADER));
}

unsigned int THFile::GetHolesTableHeaderBlockSize(void)
{
	return (sizeof(HOLESTABLEHEADER) - sizeof(HOLERECORD));
}

unsigned int THFile::GetHolesTableItemBlockSize(void)
{
	return (sizeof(HOLERECORD));
}

void THFile::SetHolesTableSize(unsigned int AHolesTableSize)
{
	HeaderInfo.HolesTableSize = AHolesTableSize;
}

unsigned int THFile::GetHolesTableSize(void)
{
	return (HeaderInfo.HolesTableSize);
}

unsigned int THFile::GetHolesTableBlockSize(void)
{
	return (GetHolesTableHeaderBlockSize() + GetHolesTableItemBlockSize() * GetHolesTableSize());
}

void THFile::SetFirstHolesTablePos(long APos)
{
	HeaderInfo.FirstHolesTablePos = APos;
}

long THFile::GetFirstHolesTablePos(void)
{
	return (HeaderInfo.FirstHolesTablePos);
}

void _PTR THFile::AllocateHolesTableBlock(void)
{
	return MAllocBlock(GetHolesTableBlockSize());
}

void THFile::FreeHolesTableBlock(void _PTRREF ATable)
{
	FreeBlock(ATable);
}

void THFile::ResetHolesTable(void _PTR ATable)
{
	SetBlock(ATable,0,GetHolesTableBlockSize());
	SetHolesTableNumUsed(ATable,0);
	SetHolesTableNextPos(ATable,-1L);
}

void THFile::SetHolesTableNumUsed(void _PTR ATable,unsigned int ANum)
{
	((HOLESTABLEHEADER _PTR)ATable)->NumUsed = ANum;
}

unsigned int THFile::GetHolesTableNumUsed(void _PTR ATable)
{
	return (((HOLESTABLEHEADER _PTR)ATable)->NumUsed);
}

void THFile::SetHolesTableNextPos(void _PTR ATable,long APos)
{
	((HOLESTABLEHEADER _PTR)ATable)->NextTablePos = APos;
}

long THFile::GetHolesTableNextPos(void _PTR ATable)
{
	return (((HOLESTABLEHEADER _PTR)ATable)->NextTablePos);
}

void THFile::SetHolesTableItem(void _PTR ATable,unsigned int AItemNo,long AHolePos,long AHoleSize)
{
	((HOLERECORD _PTR)((HOLESTABLEHEADER _PTR)ATable + 1) + (AItemNo - 1))->HolePos = AHolePos;
	((HOLERECORD _PTR)((HOLESTABLEHEADER _PTR)ATable + 1) + (AItemNo - 1))->HoleSize = AHoleSize;
}

long THFile::GetHolePos(void _PTR ATable,unsigned int AItemNo)
{
	return (((HOLERECORD _PTR)((HOLESTABLEHEADER _PTR)ATable + 1) + (AItemNo - 1))->HolePos);
}

long THFile::GetHoleSize(void _PTR ATable,unsigned int AItemNo)
{
	return (((HOLERECORD _PTR)((HOLESTABLEHEADER _PTR)ATable + 1) + (AItemNo - 1))->HoleSize);
}

