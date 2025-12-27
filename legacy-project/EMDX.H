#include <stdlib.h>
#include <alloc.h>
#include <mem.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>

#ifdef __DLL__
#	define FARDATA
#elif __COMPACT__
#	define FARDATA
#elif __LARGE__
#	define FARDATA
#endif


#ifdef FARDATA

#	define _PTR				far *
#	define _REF				far &
#	define _PTRREF			far *&
#	define _REFPTR			far &*
#	define GETMEM(X)		farmalloc(X)
#	define FREEMEM(X) 		farfree(X)
#	define MEMCOPY(X,Y,Z)	_fmemcpy(X,Y,Z)
#	define MEMSET(X,Y,Z)	_fmemset(ABlock,AC,AN)

#else

#	define _PTR				*
#	define _REF				&
#	define _PTRREF			*&
#	define _REFPTR			&*
#	define GETMEM(X)		malloc(X)
#	define FREEMEM(X) 		free(X)
#	define MEMCOPY(X,Y,Z)	memcpy(X,Y,Z)
#	define MEMSET(X,Y,Z)	memset(ABlock,AC,AN)

#endif

#define _TRUE				1
#define _FALSE				0

#define errOK 				0
#define errERROR      		1
#define errREAD				2
#define errWRITE			3
#define errSEEK				4
#define errBADDATA			5
#define errMEMERROR			6
#define errPOINTER			7
#define errBADFILEDATA		8
#define errBADFILEHANDLE	9
#define errCREATE			10
#define errGETFILESIZE		11
#define errOPEN				12
#define errCLOSE			13
#define errGETFILEPOS		14
#define errINIT 			15

#define errtINPUT			0x0001
#define errtOUTPUT			0x0002
#define errtIO            	0x0003
#define errtMEMORY        	0x0004
#define errtLOGICAL       	0x0008
#define errtBADDATA			0x0010
#define errtALL				0xFFFF

#define ftVOID 				0
#define ftBLOCK 			1
#define ftNUMBLOCK        	2
#define ftINTEGER			3
#define ftLONGINT		   	4
#define ftSTRING			5
#define ftLOGICAL			6
#define ftCHARACTER			7

#define attUNQUEE			1
#define attDELETE			2

#define	stEOF				0x0001
#define stBOF				0x0002

#define KEYSPERNODE			5
#define KEYSPERBLOCK		20


	/**********************************
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	**********************************/

unsigned char CalcBlockChecksum(void _PTR ABlock,unsigned int ABlockSize)
{
	register unsigned int I;
	register unsigned char Checksum = 0;
	register unsigned char _PTR Ptr;

	Ptr = (unsigned char _PTR)ABlock;
	for (I = 0; I < ABlockSize; I ++){
		Checksum ^= * Ptr;
		Ptr ++;
		}
	return (Checksum);
}

	/**********************************
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	**********************************/

class TError
{
public:
	TError(void);
	virtual ~TError(void);
	virtual void SetError(int AErrorCode);
	virtual void SetAdditionalInfo(void _PTR AErrorInfo);
	virtual int GetError(void);
	virtual void GetAdditionalInfo(void _PTR AErrorInfo);
	virtual int AnyError(void);
    virtual void ClearError(void);
};

TError::TError(void)
{
}

void TError::~TError(void)
{
}

#pragma argsused

void TError::SetError(int AErrorCode)
{
}

#pragma argsused

void TError::SetAdditionalInfo(void _PTR AErrorInfo)
{
}

int TError::GetError(void)
{
	return 0;
}

#pragma argsused

void TError::GetAdditionalInfo(void _PTR AErrorInfo)
{
}

int TError::AnyError(void)
{
	return _FALSE;
}

void TError::ClearError(void)
{
}

	/**********************************
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	**********************************/

class TObject:public TError
{
	int ErrorCode;
public:
	TObject(void);
	virtual ~TObject(void);
	virtual void _PTR MAllocBlock(unsigned int ABlockSize);
	virtual void FreeBlock(void _PTRREF ABlock);
	virtual void MoveBlock(void _PTR ADest,void _PTR ASource,size_t AN);
	virtual void SetBlock(void _PTR ABlock,unsigned char AC,size_t AN);
	virtual void Free(void);

	virtual void SetError(int AErrorCode);
	virtual int GetError(void);
	virtual int AnyError(void);
};

TObject::TObject(void)
{
	SetError(errOK);
}

void TObject::~TObject(void)
{
}

void _PTR TObject::MAllocBlock(unsigned int ABlockSize)
{
	register void _PTR Result = GETMEM(ABlockSize);
	if (Result == NULL)
		SetError(errMEMERROR);
	return Result;
}

void TObject::FreeBlock(void _PTRREF ABlock)
{
	if (ABlock != NULL){
		FREEMEM(ABlock);
		ABlock = NULL;
	}
	else {
		SetError(errPOINTER);
	}
}

void TObject::MoveBlock(void _PTR ADest,void _PTR ASource,size_t AN)
{
	if ((ADest != NULL) && (ASource != NULL)){
		if (MEMCOPY(ADest,ASource,AN) != ADest){
			SetError(errMEMERROR);
		}
	}
	else {
		SetError(errPOINTER);
	}
}

void TObject::SetBlock(void _PTR ABlock,unsigned char AC,size_t AN)
{
	if (ABlock != NULL) {
		if (MEMSET(ABlock,AC,AN) != ABlock){
			SetError(errMEMERROR);
		}
	}
	else {
		SetError(errPOINTER);
	}
}

void TObject::Free(void)
{
}

void TObject::SetError(int AErrorCode)
{
	ErrorCode = AErrorCode;
}

int TObject::GetError(void)
{
	return ErrorCode;
}

int TObject::AnyError(void)
{
	return (ErrorCode != errOK);
}

	/**********************************
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	**********************************/


class TFile:public TObject
{
private:
	int Handle;
public:
	TFile(const char _PTR Name,const int ACreate,const unsigned int Flags = O_RDWR | O_BINARY | O_DENYALL);
	virtual ~TFile(void);
	virtual long Pos(void);
	virtual long Size(void);
	virtual long Seek(long Pos,int FromWhere = SEEK_SET);
	virtual void Write(void _PTR Buffer,unsigned int Size,long Pos = -1);
	virtual void Read(void _PTR Buffer,unsigned int Size,long Pos = -1);
};

TFile::TFile(const char _PTR Name,const int ACreate,const unsigned int Flags)
{
	if (ACreate){
		Handle = _creat(Name,0);
		_close(Handle);
		};
	Handle = _open(Name,Flags);
}

TFile::~TFile(void)
{
	_close(Handle);
}

long TFile::Pos(void)
{
	return tell(Handle);
}

long TFile::Size(void)
{
	return filelength(Handle);
}

long TFile::Seek(long Pos,int FromWhere)
{
	return lseek(Handle,Pos,FromWhere);
}

void TFile::Write(void _PTR Buffer,unsigned int Size,long Pos)
{
	if (Pos != -1) Seek(Pos);
	_write(Handle,Buffer,Size);
}
void TFile::Read(void _PTR Buffer,unsigned int Size,long Pos)
{
	if (Pos != -1) Seek(Pos);
	_read(Handle,Buffer,Size);
}

	/**********************************
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	**********************************/


typedef struct tagMDXHEADER{
	unsigned char Checksum;
	unsigned int NumIndexes;
	}MDXHEADER;

typedef struct tagINDEXINFO{
	unsigned char Checksum;
	unsigned int Attrib;
	unsigned int KeyCode;
	unsigned int KeySize;
	unsigned int MaxItems;
	long FreeCreateNodes;
	long FreeCreateLeaves;

	long FreeNode;
	long FreeLeave;
	unsigned int NumLevels;
	long RootNode;
	long FirstLeave;
	long LastLeave;
	}INDEXINFO;

typedef struct tagNODEHEADER{
	unsigned char Checksum;
	unsigned int NumUsed;
	long NextNode;
	long PrevNode;
	// keys and pointers are here ...
	}NODEHEADER;

typedef struct tagLEAVEHEADER{
	unsigned char Checksum;
	long NextLeave;
	long PrevLeave;
	long DataPos;
	// Key Data Here ...
	}LEAVEHEADER;

typedef struct tagPOSITION{
	unsigned int State;
	long CurrentLeave;
	long NextLeave;
	long PrevLeave;
	long CurrentDataPos;
	}POSITION;

typedef struct tagSTACKITEM{
	long NodePos;
	unsigned int ChildNo;
	tagSTACKITEM _PTR Next;
	}STACKITEM;

	/**********************************
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	**********************************/


class TIndexStack:private TObject
{
private:
	STACKITEM _PTR Top;
public:
	TIndexStack(void);
	virtual ~TIndexStack(void);
	virtual void Free(void);
	virtual int Push(const long NodePos,const unsigned int ChildNo);
	virtual int Pop(long _REF NodePos,unsigned int _REF ChildNo);
	virtual int GetTop(long _REF NodePos,unsigned int _REF ChildNo);
	virtual int Empty(void);
	virtual void Clear(void);
};

TIndexStack::TIndexStack(void)
{
	Top = NULL;
}

TIndexStack::~TIndexStack(void)
{
	Free();
}

void TIndexStack::Clear(void)
{
	while (Top != NULL){
		STACKITEM _PTR Temp;
		Temp = Top;
		Top = Temp->Next;
		FreeBlock((void _PTR)Temp);
		}
	Top = NULL;
}

void TIndexStack::Free(void)
{
	Clear();
}

int TIndexStack::Push(const long NodePos,const unsigned int ChildNo)
{
	STACKITEM _PTR Temp;
	Temp = (STACKITEM _PTR)MAllocBlock(sizeof(STACKITEM));
	Temp->Next = Top;
	Temp->NodePos = NodePos;
	Temp->ChildNo = ChildNo;
	Top = Temp;
	return 1;
}

int TIndexStack::Pop(long _REF NodePos,unsigned int _REF ChildNo)
{
	int Result = 0;
	if (Top != NULL){
		STACKITEM _PTR Temp;
		Temp = Top;
		Top = Temp->Next;
		NodePos = Temp->NodePos;
		ChildNo = Temp->ChildNo;
		FreeBlock((void _PTR)Temp);
		Result = 1;
		}
	return Result;
}

int TIndexStack::GetTop(long _REF NodePos,unsigned int _REF ChildNo)
{
	int Result = 0;
	if (Top != NULL){
		NodePos = Top->NodePos;
		ChildNo = Top->ChildNo;
		Result = 1;
		}
	return Result;
}

int TIndexStack::Empty(void)
{
	return (Top == NULL);
}

	/**********************************
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	*                                 *
	**********************************/

class TMIndex:public TFile
{
private:
	MDXHEADER HeaderInfo;
	INDEXINFO _PTR IndexInfo;
	POSITION _PTR Position;
	unsigned int CurrentIndex;

	// Calculation functions ...

	void SetNumIndexes(unsigned int ANumIndexes);
	long GetIndexInfoPos(void);
	unsigned int GetIndexesInfoSize(void);
	unsigned int GetPositionsInfoSize(void);
	unsigned int GetNodeHeaderSize(void);
	unsigned int GetMaxItems(void);
	long GetFreeCreateNodes(void);
	void SetFreeCreateNodes(long ANumNodes);
	long GetFreeCreateLeaves(void);
	void SetFreeCreateLeaves(long ANumNodes);
	unsigned int GetNodeSize(void);
	unsigned int GetItemSize(void);
	unsigned int GetLeaveSize(void);
	long GetFirstFreeNode(void);
	void SetFirstFreeNode(long ANodePos);
	long GetFirstFreeLeave(void);
	void SetFirstFreeLeave(long ALeavePos);
	long GetRootNode(void);
	void SetRootNode(long ANodePos);
	long GetFirstLeave(void);
	void SetFirstLeave(long ALeavePos);
	long GetLastLeave(void);
	void SetLastLeave(long ALeavePos);
	unsigned int GetNumLevels(void);
	void SetNumLevels(unsigned int ALevel);
	unsigned int IncNumLevels(void);
	unsigned int DecNumLevels(void);
	void ResetPosition(void);
	void GetPosition(long _REF ACurrentLeavePos,long _REF ANextLeavePos,long _REF APrevLeavePos,long _REF ACurrentDataPos);
	void SetPosition(long ACurrentLeavePos,long ANextLeavePos,long APrevLeavePos,long ACurrentDataPos);
	void SetPosition(long ALeavePos,void _PTR ALeave);
	long GetCurrentDataPosition(void);
	long GetCurrentPosition(void);
	long GetNextPosition(void);
	long GetPrevPosition(void);
	void SetCurrentDataPosition(long ACurrentDataPosition);
	void SetCurrentPosition(long ACurrentLeavePos,long ACurrentDataPosition);
	void SetEOF(void);
	void ResetEOF(void);
	void SetBOF(void);
	void ResetBOF(void);
	void SetNextPosition(long ANextLeavePos);
	void SetPrevPosition(long APrevLeavePos);

	void ResetNode(void _PTR ANode);
	void ResetLeave(void _PTR ALeave);
	unsigned int GetNumItems(void _PTR ANode);
	void SetNumItems(void _PTR ANode,unsigned int ANumKeys);
	long IncNumItems(void _PTR ANode);
	long DecNumItems(void _PTR ANode);
	void _PTR GetNodeKey(void _PTR ANode,unsigned int AItemNo);
	void SetNodeKey(void _PTR ANode,unsigned int AItemNo,void _PTR AKeyData);
	void _PTR GetLeaveKey(void _PTR ALeave);
	void SetLeaveKey(void _PTR ALeave,void _PTR AKeyData);
	void FillEOFKey(void _PTR AKeyBlock);
	long GetChildPos(void _PTR ANode,unsigned int AItemNo);
	void SetChildPos(void _PTR ANode,unsigned int AItemNo,long AChildPos);
	long GetNextNode(void _PTR ANode);
	void SetNextNode(void _PTR ANode,long ANextNodePos);
	long GetPrevNode(void _PTR ANode);
	void SetPrevNode(void _PTR ANode,long APrevNodePos);
	long GetNextLeave(void _PTR ANode);
	void SetNextLeave(void _PTR ANode,long ANextNodePos);
	long GetPrevLeave(void _PTR ANode);
	void SetPrevLeave(void _PTR ANode,long APrevNodePos);
	long GetDataPos(void _PTR ALeave);
	void SetDataPos(void _PTR ALeave,long ADataPos);
	void InsertItem(void _PTR ANode,unsigned int AItemNo,void _PTR AKeyData,long AChildPos);
	void DeleteItem(void _PTR ANode,unsigned int AItemNo);

	// Memory functions ...
	void Allocate(void);
	void Free(void);
	void _PTR AllocateNodeBlock(void);
	void FreeNodeBlock(void _PTRREF ANode);
	void _PTR AllocateKeyBlock(void);
	void FreeKeyBlock(void _PTRREF AKey);
	void _PTR AllocateLeaveBlock(void);
	void FreeLeaveBlock(void _PTRREF ALeave);

    // Error detection functions ...

	void SetHeaderChecksum(void);
	int TestHeaderChecksum(void);
	void SetInfoChecksum(unsigned int AIndexNo);
	int TestInfoChecksum(unsigned int AIndexNo);
	void SetNodeChecksum(void _PTR ANode);
	int TestNodeChecksum(void _PTR ANode);
	void SetLeaveChecksum(void _PTR ANode);
	int TestLeaveChecksum(void _PTR ANode);

	// File change functions ...

	void WriteHeader(void);
	void ReadHeader(void);
	void WriteInfo(void);
	void ReadInfo(void);
	void WriteAllInfo(void);
	void ReadAllInfo(void);
	void CreateNodes(const long ANumNodes);
	void CreateLeaves(const long ANumLeaves);
	long AllocateNode(void);
	long AllocateLeave(void);
	void FreeNode(long ANodePos);
	void FreeLeave(long ALeavePos);
	void ReadNode(void _PTR ANode,long ANodePos = -1);
	void WriteNode(void _PTR ANode,long ANodePos = -1);
	long WriteNewNode(void _PTR ANode);
	void ReadLeave(void _PTR ALeave,long ALeavePos = -1);
	void WriteLeave(void _PTR ALeave,long ALeavePos = -1);
	long WriteNewLeave(void _PTR ALeave);
	// virtual void DisplayNodeData(void _PTR ANode);

	// Process functions ...
	int FindPath(void _PTR AKey,TIndexStack _PTR AStack,long _REF ALastLevelChild);
	void CreateFirstNode(void);
	int InsertKey(long ANodePos,void _PTR ANewKey,long ANewChildPos,unsigned int AChangedKeyNo,void *AChangedKeyVal,void _PTRREF AParentKey,void _PTRREF AAditionalKey,long _REF AAditionalChildPos);
	int RemoveKey(long ANodePos,unsigned int ARemoveKeyNo,void _PTRREF AParentKey);
	long GetFirstBottomNode(void);
	long GetFirstNodeFromLevel(unsigned int ALevel);
	long BringLeave(long ALeavePos,void _PTR AKey);
	int FindLeave(void _PTR AKey,long _REF ALeavePos);
	long ModifyLeave(void _PTR AKey,long ANewLeavePos);
	long DeleteKeyFromNodes(void _PTR ADeleteKey);
public:

	// User functions ...
	TMIndex( const char _PTR AName,
			 unsigned int NumIndexes);
	TMIndex( const char _PTR AName);
	~TMIndex(void);
	void InitIndex(const unsigned int AKeyCode,
				   const unsigned int AKeySize,
				   unsigned int AAttrib,
				   const unsigned int ANumItems,
				   const long AFreeCreateNode,
				   const long AFreeCreateLeave);
	void FlushIndex(void);
	void FlushFile(void);
	int CanDelete(void);
	int Unque(void);
	unsigned int GetNumIndexes(void);
	unsigned int GetKeyType(void);
	unsigned int GetKeySize(void);
	void SetActiveIndex(unsigned int AIndexNo);
	unsigned int GetActiveIndex(void);
	virtual int Compare(void _PTR AKey1,
					    void _PTR AKey2);
	int GetEOF(void);
	int GetBOF(void);
	long GetFirst(void _PTR AKey);
	long GetNext(void _PTR AKey);
	long GetPrev(void _PTR AKey);
	long GetCurrent(void _PTR AKey);
	int Delete(void _PTR ADeleteKey);
	long DeleteCurrent(void);
	long Find(void _PTR AKey);
	int Append(void _PTR ANewKey,
			   long ANewDataPos);
};

TMIndex::TMIndex(const char _PTR AName,unsigned int ANumIndexes):TFile(AName,1)
{
	unsigned int I;
	CurrentIndex = 0;
	HeaderInfo.NumIndexes = ANumIndexes;
	Allocate();
	for(I=0;I<ANumIndexes;I++){
		IndexInfo[I].Attrib = 0;
		IndexInfo[I].KeyCode = 0;
		IndexInfo[I].KeySize = 0;
		IndexInfo[I].MaxItems = 0;
		IndexInfo[I].FreeCreateNodes = 0;
		IndexInfo[I].FreeCreateLeaves = 0;
		IndexInfo[I].FreeNode = -1;
		IndexInfo[I].NumLevels = 0;
		IndexInfo[I].RootNode = -1;

		Position[I].CurrentLeave = -1;
		Position[I].NextLeave = -1;
		Position[I].PrevLeave = -1;
		Position[I].CurrentDataPos = -1;
		Position[I].State = 0;
		}
	WriteHeader();
	WriteAllInfo();
}

TMIndex::TMIndex(const char _PTR AName):TFile(AName,0)
{
	ReadHeader();
	CurrentIndex = 0;
	Allocate();
	ReadHeader();
	ReadAllInfo();
}

TMIndex::~TMIndex(void)
{
	WriteHeader();
	WriteAllInfo();
	Free();
}

void TMIndex::InitIndex(const unsigned int AKeyCode,const unsigned int AKeySize,unsigned int AAttrib,const unsigned int ANumItems,const long AFreeCreateNodes,const long AFreeCreateLeaves)
{
	if (AnyError()) return;

	IndexInfo[CurrentIndex].Attrib = AAttrib;
	IndexInfo[CurrentIndex].KeyCode = AKeyCode;
	IndexInfo[CurrentIndex].KeySize = AKeySize;
	IndexInfo[CurrentIndex].MaxItems = ANumItems;
	IndexInfo[CurrentIndex].FreeCreateNodes = AFreeCreateNodes;
	IndexInfo[CurrentIndex].FreeCreateLeaves = AFreeCreateLeaves;
	IndexInfo[CurrentIndex].FreeNode = -1;
	IndexInfo[CurrentIndex].FreeLeave = -1;
	IndexInfo[CurrentIndex].NumLevels = 0;
	IndexInfo[CurrentIndex].RootNode = -1;
	WriteInfo();
	CreateNodes(AFreeCreateNodes);
	CreateLeaves(AFreeCreateLeaves);
	CreateFirstNode();
	SetNumLevels(1);
}

void TMIndex::FlushIndex(void)
{
	if (AnyError()) return;
	WriteHeader();
}

void TMIndex::FlushFile(void)
{
	if (AnyError()) return;
	WriteAllInfo();
}

int TMIndex::CanDelete(void)
{
	if (AnyError()) return 0;
	return (IndexInfo[CurrentIndex].Attrib & attDELETE);
}

int TMIndex::Unque(void)
{
	if (AnyError()) return 0;
	return (IndexInfo[CurrentIndex].Attrib & attUNQUEE);
}

unsigned int TMIndex::GetNumIndexes(void)
{
	if (AnyError()) return 0;
	return HeaderInfo.NumIndexes;
}

void TMIndex::SetNumIndexes(unsigned int ANumIndexes)
{
	HeaderInfo.NumIndexes = ANumIndexes;
}

unsigned int TMIndex::GetIndexesInfoSize(void)
{
	return (sizeof(INDEXINFO)*GetNumIndexes());
}

unsigned int TMIndex::GetPositionsInfoSize(void)
{
	return (sizeof(POSITION)*GetNumIndexes());
}

long TMIndex::GetIndexInfoPos(void)
{
	return (sizeof(MDXHEADER)+CurrentIndex*sizeof(INDEXINFO));
}

unsigned int TMIndex::GetKeyType(void)
{
	if (AnyError()) return 0;
	return IndexInfo[CurrentIndex].KeyCode;
}

unsigned int TMIndex::GetNodeHeaderSize(void)
{
	return sizeof(NODEHEADER);
}

unsigned int TMIndex::GetKeySize(void)
{
	if (AnyError()) return 0;
	return IndexInfo[CurrentIndex].KeySize;
}

unsigned int TMIndex::GetMaxItems(void)
{
	return IndexInfo[CurrentIndex].MaxItems;
}

unsigned int TMIndex::GetItemSize(void)
{
	// (key data size + child pointer size)
	return (IndexInfo[CurrentIndex].KeySize+sizeof(long));
}

unsigned int TMIndex::GetLeaveSize(void)
{
	return (IndexInfo[CurrentIndex].KeySize+sizeof(LEAVEHEADER));
}

long TMIndex::GetFreeCreateNodes(void)
{
	return IndexInfo[CurrentIndex].FreeCreateNodes;
}

void TMIndex::SetFreeCreateNodes(long ANumNodes)
{
	IndexInfo[CurrentIndex].FreeCreateNodes = ANumNodes;
}

long TMIndex::GetFreeCreateLeaves(void)
{
	return IndexInfo[CurrentIndex].FreeCreateLeaves;
}

void TMIndex::SetFreeCreateLeaves(long ANumLeaves)
{
	IndexInfo[CurrentIndex].FreeCreateLeaves = ANumLeaves;
}

long TMIndex::GetFirstFreeNode(void)
{
	return IndexInfo[CurrentIndex].FreeNode;
}

void TMIndex::SetFirstFreeNode(long ANodePos)
{
	IndexInfo[CurrentIndex].FreeNode = ANodePos;
}

long TMIndex::GetFirstFreeLeave(void)
{
	return IndexInfo[CurrentIndex].FreeLeave;
}

void TMIndex::SetFirstFreeLeave(long ALeavePos)
{
	IndexInfo[CurrentIndex].FreeLeave = ALeavePos;
}

long TMIndex::GetRootNode(void)
{
	return IndexInfo[CurrentIndex].RootNode;
}

void TMIndex::SetRootNode(long ANodePos)
{
	IndexInfo[CurrentIndex].RootNode = ANodePos;
}

long TMIndex::GetFirstLeave(void)
{
	return IndexInfo[CurrentIndex].FirstLeave;
}

void TMIndex::SetFirstLeave(long ALeavePos)
{
	IndexInfo[CurrentIndex].FirstLeave = ALeavePos;
}

long TMIndex::GetLastLeave(void)
{
	return IndexInfo[CurrentIndex].LastLeave;
}

void TMIndex::SetLastLeave(long ALeavePos)
{
	IndexInfo[CurrentIndex].LastLeave = ALeavePos;
}

unsigned int TMIndex::GetNumLevels(void)
{
	return IndexInfo[CurrentIndex].NumLevels;
}

void TMIndex::SetNumLevels(unsigned int ALevel)
{
	IndexInfo[CurrentIndex].NumLevels = ALevel;
}

unsigned int TMIndex::IncNumLevels(void)
{
	IndexInfo[CurrentIndex].NumLevels++;
	return IndexInfo[CurrentIndex].NumLevels;
}

unsigned int TMIndex::DecNumLevels(void)
{
	IndexInfo[CurrentIndex].NumLevels--;
	return IndexInfo[CurrentIndex].NumLevels;
}

void TMIndex::ResetPosition(void)
{
	Position[CurrentIndex].CurrentLeave = -1;
	Position[CurrentIndex].NextLeave = -1;
	Position[CurrentIndex].PrevLeave = -1;
	Position[CurrentIndex].CurrentDataPos = -1;
	Position[CurrentIndex].State = 0;
}

void TMIndex::GetPosition(long _REF ACurrentLeavePos,long _REF ANextLeavePos,long _REF APrevLeavePos,long _REF ACurrentDataPosition)
{
	ACurrentLeavePos = Position[CurrentIndex].CurrentLeave;
	ANextLeavePos = Position[CurrentIndex].NextLeave;
	APrevLeavePos = Position[CurrentIndex].PrevLeave;
	ACurrentDataPosition = Position[CurrentIndex].CurrentDataPos;
}

void TMIndex::SetPosition(long ACurrentLeavePos,long ANextLeavePos,long APrevLeavePos,long ACurrentDataPosition)
{
	Position[CurrentIndex].CurrentLeave = ACurrentLeavePos;
	Position[CurrentIndex].NextLeave = ANextLeavePos;
	Position[CurrentIndex].PrevLeave = APrevLeavePos;
	Position[CurrentIndex].CurrentDataPos = ACurrentDataPosition;

	if ((Position[CurrentIndex].PrevLeave == -1) || (Position[CurrentIndex].CurrentLeave == GetFirstLeave())){
		SetBOF();
		}
	else {
		ResetBOF();
		};

	if ((Position[CurrentIndex].NextLeave == -1) || (Position[CurrentIndex].NextLeave == GetLastLeave())){
		SetEOF();
		}
	else {
		ResetEOF();
		};
}

void TMIndex::SetPosition(long ALeavePos,void _PTR ALeave)
{
	if (ALeave != NULL)
		SetPosition(ALeavePos,GetNextLeave(ALeave),GetPrevLeave(ALeave),GetDataPos(ALeave));
	else ResetPosition();
}

long TMIndex::GetCurrentDataPosition(void)
{
	return Position[CurrentIndex].CurrentDataPos;
}

long TMIndex::GetCurrentPosition(void)
{
	return	Position[CurrentIndex].CurrentLeave;
}

long TMIndex::GetNextPosition(void)
{
	return Position[CurrentIndex].NextLeave;
}

long TMIndex::GetPrevPosition(void)
{
	return Position[CurrentIndex].PrevLeave;
}

int TMIndex::GetEOF(void)
{
	if (AnyError()) return _TRUE;
	return ((Position[CurrentIndex].State & stEOF) == stEOF);
}

int TMIndex::GetBOF(void)
{
	if (AnyError()) return _TRUE;
	return ((Position[CurrentIndex].State & stBOF) == stBOF);
}

void TMIndex::SetEOF(void)
{
	Position[CurrentIndex].State |= stEOF;
}

void TMIndex::ResetEOF(void)
{
	Position[CurrentIndex].State &= (stEOF ^ 0xFFFF);
}

void TMIndex::SetBOF(void)
{
	Position[CurrentIndex].State |= stBOF;
}

void TMIndex::ResetBOF(void)
{
	Position[CurrentIndex].State &= (stBOF ^ 0xFFFF);
}

void TMIndex::SetCurrentDataPosition(long ACurrentDataPosition)
{
	Position[CurrentIndex].CurrentDataPos = ACurrentDataPosition;
}

void TMIndex::SetCurrentPosition(long ACurrentLeavePos,long ACurrentDataPosition)
{
	Position[CurrentIndex].CurrentLeave = ACurrentLeavePos;
	Position[CurrentIndex].CurrentDataPos = ACurrentDataPosition;
}

void TMIndex::SetNextPosition(long ANextLeavePos)
{
	Position[CurrentIndex].NextLeave = ANextLeavePos;
}

void TMIndex::SetPrevPosition(long APrevLeavePos)
{
	Position[CurrentIndex].PrevLeave = APrevLeavePos;
}

unsigned int TMIndex::GetNodeSize(void)
{
	// (key data size + child pointer size) * (max items in node) + (node header size)
	return (GetItemSize()*GetMaxItems()+GetNodeHeaderSize());
}

int TMIndex::Compare(void _PTR AKey1,void _PTR AKey2)
{
	if (AnyError()) return 0;
	int Result = 0;
	switch (GetKeyType()) {
		case ftBLOCK:{
			unsigned char _PTR Temp1;
			unsigned char _PTR Temp2;
			Temp1 = (unsigned char _PTR)AKey1;
			Temp2 = (unsigned char _PTR)AKey2;
				{
				register unsigned int I;
				register unsigned int Max;
				Max = GetKeySize();
				for (I=0;I<Max;I++){
					if (Temp1[I] != Temp2[I]){
						if (Temp1[I]>Temp2[I]) {
							Result = +1;
							}
							else {
							Result = -1;
							}
						break;
						}
					}
				}
			};break;
		case ftNUMBLOCK:{
			unsigned char _PTR Temp1;
			unsigned char _PTR Temp2;
			Temp1 = (unsigned char _PTR)AKey1;
			Temp2 = (unsigned char _PTR)AKey2;
				{
				register unsigned int I;
				for (I = GetKeySize(); I > 0;){
					I --;
					if (Temp1[I] != Temp2[I]){
						if (Temp1[I]>Temp2[I]) {
							Result = +1;
							}
							else {
							Result = -1;
							}
						break;
						}
					}
				}
			};break;
		case ftINTEGER:{
			if (*(int *)(AKey1)>*(int *)(AKey2)) Result = +1;
			else if (*(int *)(AKey1)<*(int *)(AKey2)) Result = -1;
			};break;
		case ftLONGINT:{
			if (*(long *)(AKey1)>*(long *)(AKey2)) Result = +1;
			else if (*(long *)(AKey1)<*(long *)(AKey2)) Result = -1;
			};break;
		case ftSTRING:{
			Result = strcmp((char _PTR)AKey1,(char _PTR)AKey2);
			if (Result>1) Result = 1;
			else if (Result<-1) Result = -1;
			};break;
		case ftLOGICAL:{
			if ((*(char *)(AKey1) != 0) && (*(char *)(AKey2) == 0)) Result = 1;
			else if ((*(char *)(AKey1) == 0) && (*(char *)(AKey2) != 0)) Result = -1;
			};break;
		case ftCHARACTER:{
			if (*(char *)(AKey1)>*(char *)(AKey2)) Result = +1;
			else if (*(char *)(AKey1)<*(char *)(AKey2)) Result = -1;
			};break;
		default:;
		}
	return Result;
}

void TMIndex::ResetNode(void _PTR ANode)
{
	SetBlock(ANode,0,GetNodeSize());
	SetNumItems(ANode,0);
	SetNextNode(ANode,-1);
	SetPrevNode(ANode,-1);
}

void TMIndex::ResetLeave(void _PTR ALeave)
{
	SetBlock(ALeave,0,GetLeaveSize());
	SetNextLeave(ALeave,-1);
	SetPrevLeave(ALeave,-1);
}

unsigned int TMIndex::GetNumItems(void _PTR ANode)
{
	return (((NODEHEADER _PTR)ANode)->NumUsed);
}

void TMIndex::SetNumItems(void _PTR ANode,unsigned int ANumItems)
{
	((NODEHEADER _PTR)ANode)->NumUsed = ANumItems;
}

long TMIndex::IncNumItems(void _PTR ANode)
{
	if (((NODEHEADER _PTR)ANode)->NumUsed < GetMaxItems()) ((NODEHEADER _PTR)ANode)->NumUsed++;
	return ((NODEHEADER _PTR)ANode)->NumUsed;
}

long TMIndex::DecNumItems(void _PTR ANode)
{
	if (((NODEHEADER _PTR)ANode)->NumUsed > 0) ((NODEHEADER _PTR)ANode)->NumUsed--;
	return ((NODEHEADER _PTR)ANode)->NumUsed;
}

void _PTR TMIndex::GetNodeKey(void _PTR ANode,unsigned int AItemNo)
{
	// may be error calculations ...
	return (char _PTR)((NODEHEADER _PTR)ANode+1)+(GetItemSize()*(AItemNo-1));
}

void TMIndex::SetNodeKey(void _PTR ANode,unsigned int AItemNo,void _PTR AKeyData)
{
	MoveBlock(GetNodeKey(ANode,AItemNo),AKeyData,GetKeySize());
}

void _PTR TMIndex::GetLeaveKey(void *ALeave)
{
	return ((LEAVEHEADER _PTR)ALeave+1);
}

void TMIndex::SetLeaveKey(void _PTR ALeave,void _PTR AKeyData)
{
	MoveBlock(GetLeaveKey(ALeave),AKeyData,GetKeySize());
}

void TMIndex::FillEOFKey(void _PTR AKeyBlock)
{
	switch (GetKeyType()){
	case ftBLOCK:
		SetBlock(AKeyBlock,255,GetKeySize());
		break;
	case ftNUMBLOCK:
		SetBlock(AKeyBlock,255,GetKeySize());
		*((unsigned char _PTR)AKeyBlock) &= 127;
		break;
	case ftINTEGER:
		SetBlock(AKeyBlock,255,GetKeySize());
		*((unsigned char _PTR)AKeyBlock) &= 127;
		break;
	case ftLONGINT:
		SetBlock(AKeyBlock,255,GetKeySize());
		*((unsigned char _PTR)AKeyBlock+(GetKeySize()-1)) &= 127;
		break;
	case ftSTRING:
		SetBlock(AKeyBlock,255,GetKeySize()-1);
		*((char _PTR)AKeyBlock + (GetKeySize()-1)) = 0;
		break;
	case ftLOGICAL:
		SetBlock(AKeyBlock,255,GetKeySize());
		break;
	case ftCHARACTER:
		SetBlock(AKeyBlock,255,GetKeySize());
		break;
	default:;
	};

	//
}

long TMIndex::GetChildPos(void _PTR ANode,unsigned int AItemNo)
{
	return *(long _PTR)((char _PTR)((NODEHEADER _PTR)ANode+1)+ (GetItemSize()*(AItemNo-1)+GetKeySize()));
}

void TMIndex::SetChildPos(void _PTR ANode,unsigned int AItemNo,long AChildPos)
{
	*(long _PTR)((char _PTR)((NODEHEADER _PTR)ANode+1)+ (GetItemSize()*(AItemNo-1)+GetKeySize())) = AChildPos;
}

long TMIndex::GetNextNode(void _PTR ANode)
{
	return (((NODEHEADER _PTR )ANode)->NextNode);
}

void TMIndex::SetNextNode(void _PTR ANode,long ANextNodePos)
{
	((NODEHEADER _PTR)ANode)->NextNode = ANextNodePos;
}

long TMIndex::GetPrevNode(void _PTR ANode)
{
	return (((NODEHEADER _PTR)ANode)->PrevNode);
}

void TMIndex::SetPrevNode(void _PTR ANode,long APrevNodePos)
{
	((NODEHEADER _PTR)ANode)->PrevNode = APrevNodePos;
}

long TMIndex::GetNextLeave(void _PTR ALeave)
{
	return (((LEAVEHEADER _PTR)ALeave)->NextLeave);
}

void TMIndex::SetNextLeave(void _PTR ALeave,long ANextLeavePos)
{
	((LEAVEHEADER _PTR)ALeave)->NextLeave = ANextLeavePos;
}

long TMIndex::GetPrevLeave(void _PTR ALeave)
{
	return (((LEAVEHEADER _PTR)ALeave)->PrevLeave);
}

void TMIndex::SetPrevLeave(void _PTR ALeave,long APrevLeavePos)
{
	((LEAVEHEADER _PTR)ALeave)->PrevLeave = APrevLeavePos;
}

long TMIndex::GetDataPos(void _PTR ALeave)
{
	return (((LEAVEHEADER _PTR)ALeave)->DataPos);
}

void TMIndex::SetDataPos(void _PTR ALeave,long ADataPos)
{
	(((LEAVEHEADER _PTR)ALeave)->DataPos) = ADataPos;
}

void TMIndex::InsertItem(void _PTR ANode,unsigned int AItemNo,void _PTR AKeyData,long AChildPos)
{
	unsigned int ItemNo;
	if (AItemNo>0){
		IncNumItems(ANode);
		if (AItemNo >= GetNumItems(ANode)){
			ItemNo = GetNumItems(ANode);
			}
		else {
			unsigned int I;
			ItemNo = AItemNo;
			for (I = GetNumItems(ANode);I>ItemNo;I--){
				SetNodeKey(ANode,I,GetNodeKey(ANode,I-1));
				SetChildPos(ANode,I,GetChildPos(ANode,I-1));
				}
			}
		SetNodeKey(ANode,ItemNo,AKeyData);
		SetChildPos(ANode,ItemNo,AChildPos);
		}
}

void TMIndex::DeleteItem(void _PTR ANode,unsigned int AItemNo)
{
	if ((AItemNo<GetNumItems(ANode)) && (AItemNo>0)){
		unsigned int I;
		for (I = AItemNo;I<GetNumItems(ANode);I++){
			SetNodeKey(ANode,I,GetNodeKey(ANode,I+1));
			SetChildPos(ANode,I,GetChildPos(ANode,I+1));
			}
		}
	DecNumItems(ANode);
}

void TMIndex::Allocate(void)
{
	IndexInfo = (INDEXINFO _PTR)MAllocBlock(GetIndexesInfoSize());
	Position = (POSITION _PTR)MAllocBlock(GetPositionsInfoSize());
}

void TMIndex::Free(void)
{
	FreeBlock((void _PTR)IndexInfo);
	FreeBlock((void _PTR)Position);
}

void _PTR TMIndex::AllocateNodeBlock(void)
{
	return MAllocBlock(GetNodeSize());
}

void TMIndex::FreeNodeBlock(void _PTRREF ANode)
{
	FreeBlock(ANode);
}

void _PTR TMIndex::AllocateKeyBlock(void)
{
	return MAllocBlock(GetKeySize());
}

void TMIndex::FreeKeyBlock(void _PTRREF AKey)
{
	FreeBlock(AKey);
}

void _PTR TMIndex::AllocateLeaveBlock(void)
{
	return MAllocBlock(GetLeaveSize());
}

void TMIndex::FreeLeaveBlock(void _PTRREF ALeave)
{
	FreeBlock(ALeave);
}

void TMIndex::SetHeaderChecksum(void)
{
	HeaderInfo.Checksum = 0;
	HeaderInfo.Checksum = CalcBlockChecksum((void _PTR) &HeaderInfo,sizeof(HeaderInfo));
}

int TMIndex::TestHeaderChecksum(void)
{
	return (CalcBlockChecksum((void _PTR) &HeaderInfo,sizeof(HeaderInfo)) == 0);
}

void TMIndex::SetInfoChecksum(unsigned int AIndexNo)
{
	IndexInfo[AIndexNo].Checksum = 0;
	IndexInfo[AIndexNo].Checksum = CalcBlockChecksum((void _PTR) &IndexInfo[AIndexNo],sizeof(IndexInfo[AIndexNo]));
}

int TMIndex::TestInfoChecksum(unsigned int AIndexNo)
{
	return (CalcBlockChecksum((void _PTR) &IndexInfo[AIndexNo],sizeof(IndexInfo[AIndexNo])) == 0);
}


void TMIndex::SetNodeChecksum(void _PTR ANode)
{
	((NODEHEADER _PTR)ANode)->Checksum = 0;
	((NODEHEADER _PTR)ANode)->Checksum = CalcBlockChecksum(ANode,GetNodeSize());
}

int TMIndex::TestNodeChecksum(void _PTR ANode)
{
	return (CalcBlockChecksum(ANode,GetNodeSize()) == 0);
}

void TMIndex::SetLeaveChecksum(void _PTR ALeave)
{
	((LEAVEHEADER _PTR)ALeave)->Checksum = 0;
	((LEAVEHEADER _PTR)ALeave)->Checksum = CalcBlockChecksum(ALeave,GetLeaveSize());
}

int TMIndex::TestLeaveChecksum(void _PTR ALeave)
{
	return (CalcBlockChecksum(ALeave,GetLeaveSize()) == 0);
}

void TMIndex::WriteHeader(void)
{
	SetHeaderChecksum();
	Write((void _PTR) &HeaderInfo,sizeof(HeaderInfo),0);
}

void TMIndex::ReadHeader(void)
{
	Read((void _PTR) &HeaderInfo,sizeof(HeaderInfo),0);
	if (!TestHeaderChecksum())
		SetError(errBADDATA);
}

void TMIndex::WriteInfo(void)
{
	SetInfoChecksum(CurrentIndex);
	Write((void _PTR) &IndexInfo[CurrentIndex],sizeof(IndexInfo[CurrentIndex]),GetIndexInfoPos());
}

void TMIndex::ReadInfo(void)
{
	Read((void _PTR) &IndexInfo[CurrentIndex],sizeof(IndexInfo[CurrentIndex]),GetIndexInfoPos());
	if (!TestInfoChecksum(CurrentIndex))
		SetError(errBADDATA);
}

void TMIndex::WriteAllInfo(void)
{
// sizeof(HeaderInfo) = Position in the file of indexes information ...
	unsigned int I,Num = GetNumIndexes();
	for(I = 0;I < Num;I ++)
		SetInfoChecksum(I);
	Write(IndexInfo,GetIndexesInfoSize(),sizeof(HeaderInfo));
}

void TMIndex::ReadAllInfo(void)
{
	unsigned int I,Num = GetNumIndexes();
	Read(IndexInfo,GetIndexesInfoSize(),sizeof(HeaderInfo));
	for(I = 0;I < Num;I ++)
		if (!TestInfoChecksum(I))
        	SetError(errBADDATA);
}

void TMIndex::CreateNodes(const long ANumNodes)
{
	long FSize;
	void _PTR TempNode;
	long Counter;
	long FirstCreatedNodePos;

	FSize = Size();
	FirstCreatedNodePos = FSize;
	TempNode = AllocateNodeBlock();
	ResetNode(TempNode);
	Seek(FSize);
	for (Counter = 1;Counter < ANumNodes;Counter++){
		FSize += GetNodeSize();
		SetNextNode(TempNode,FSize);
		WriteNode(TempNode);
		}
	SetNextNode(TempNode,GetFirstFreeNode());
	WriteNode(TempNode);
	FreeNodeBlock(TempNode);
	SetFirstFreeNode(FirstCreatedNodePos);
}

void TMIndex::CreateLeaves(const long ANumLeaves)
{
	long FSize;
	void _PTR TempLeave;
	long Counter;
	long FirstCreatedLeavePos;

	FSize = Size();
	FirstCreatedLeavePos = FSize;
	TempLeave = AllocateLeaveBlock();
	ResetLeave(TempLeave);
	Seek(FSize);
	for (Counter = 1;Counter < ANumLeaves;Counter++){
		FSize += GetLeaveSize();
		SetNextLeave(TempLeave,FSize);
		WriteLeave(TempLeave);
		}
	SetNextLeave(TempLeave,GetFirstFreeLeave());
	WriteLeave(TempLeave);
	FreeLeaveBlock(TempLeave);
	SetFirstFreeLeave(FirstCreatedLeavePos);
}


long TMIndex::AllocateNode(void)
{
	void _PTR TempNode;
	long NodePos = -1;

	if ((NodePos = GetFirstFreeNode()) == -1) {
		CreateNodes(GetFreeCreateNodes());
		NodePos = GetFirstFreeNode();
		};
	TempNode = AllocateNodeBlock();
	ReadNode(TempNode,NodePos);
	SetFirstFreeNode(GetNextNode(TempNode));
	FreeNodeBlock(TempNode);
	return NodePos;
}

long TMIndex::AllocateLeave(void)
{
	void _PTR TempLeave;
	long LeavePos = -1;

	if ((LeavePos = GetFirstFreeLeave()) == -1) {
		CreateLeaves(GetFreeCreateLeaves());
		LeavePos = GetFirstFreeLeave();
		};
	TempLeave = AllocateLeaveBlock();
	ReadLeave(TempLeave,LeavePos);
	SetFirstFreeLeave(GetNextLeave(TempLeave));
	FreeLeaveBlock(TempLeave);
	return LeavePos;
}


void TMIndex::FreeNode(long ANodePos)
{
	void _PTR TempNode;

	TempNode = AllocateNodeBlock();
	ResetNode(TempNode);
	SetNextNode(TempNode,GetFirstFreeNode());
	WriteNode(TempNode,ANodePos);
	FreeNodeBlock(TempNode);
	SetFirstFreeNode(ANodePos);
}

void TMIndex::FreeLeave(long ALeavePos)
{
	void _PTR TempLeave;

	TempLeave = AllocateLeaveBlock();
	ResetLeave(TempLeave);
	SetNextLeave(TempLeave,GetFirstFreeLeave());
	WriteLeave(TempLeave,ALeavePos);
	FreeLeaveBlock(TempLeave);
	SetFirstFreeLeave(ALeavePos);
}


void TMIndex::ReadNode(void _PTR ANode,long ANodePos)
{
	Read(ANode,GetNodeSize(),ANodePos);
	if (!TestNodeChecksum(ANode))
		SetError(errBADDATA);
}

void TMIndex::WriteNode(void _PTR ANode,long ANodePos)
{
	SetNodeChecksum(ANode);
	Write(ANode,GetNodeSize(),ANodePos);
}

long TMIndex::WriteNewNode(void _PTR ANode)
{
	long NodePos;

	NodePos = AllocateNode();
	WriteNode(ANode,NodePos);
	return NodePos;
}

void TMIndex::ReadLeave(void _PTR ALeave,long ALeavePos)
{
	Read(ALeave,GetLeaveSize(),ALeavePos);
	if (!TestLeaveChecksum(ALeave))
		SetError(errBADDATA);
}

void TMIndex::WriteLeave(void _PTR ALeave,long ALeavePos)
{
	SetLeaveChecksum(ALeave);
	Write(ALeave,GetLeaveSize(),ALeavePos);
}

long TMIndex::WriteNewLeave(void _PTR ALeave)
{
	long LeavePos;

	LeavePos = AllocateLeave();
	WriteLeave(ALeave,LeavePos);
	return LeavePos;
}

/*
void TMIndex::DisplayNodeData(void _PTR ANode)
{
	unsigned int I,NumItems;
	NumItems = GetNumItems(ANode);
	if (NumItems>0){
		for (I = 1;I<=NumItems;I++){
			char _PTR CP;
			unsigned int I2;
			long Child;
			Child = GetChildPos(ANode,I);
			if (Child != -1) cout <<"[" << (char _PTR)GetNodeKey(ANode,I) << "] ";
			else cout << "[EOF]";
			}
		}
}
*/

void TMIndex::SetActiveIndex(unsigned int AIndexNo)
{
	if (AnyError()) return;
	if ((AIndexNo<=GetNumIndexes()) && (AIndexNo>0)) CurrentIndex = AIndexNo-1;
	else CurrentIndex = 0;
}

int TMIndex::FindPath(void _PTR AKey,TIndexStack _PTR AStack,long _REF ALastLevelChild)
{
	long NodePos;
	int Result = 0;

	AStack->Clear();
	if ((NodePos = GetRootNode()) != -1){
		unsigned int RemainLevels;
		void _PTR Node;
		int NotFirstLevel = 0;
		RemainLevels = GetNumLevels();
		Node = AllocateNodeBlock();
		while (NodePos != -1){
			unsigned int NumItems,KeyNo;
			ReadNode(Node,NodePos);
			NumItems = GetNumItems(Node);
			if ((NotFirstLevel) || (NumItems>1)){
				int CompResult;
				NotFirstLevel = 1;
				KeyNo = 1;
				while ((KeyNo<=NumItems) && ((CompResult = Compare(AKey,GetNodeKey(Node,KeyNo))) == 1)) KeyNo++;
				if (KeyNo<=NumItems){
					RemainLevels--;
					if (RemainLevels == 0){
						if (CompResult == 0) {
							AStack->Push(NodePos,KeyNo);
							}
						else {
							AStack->Push(NodePos,0);
							}
						ALastLevelChild = GetChildPos(Node,KeyNo);
						NodePos = -1;
						}
					else{
						AStack->Push(NodePos,KeyNo);
						NodePos = GetChildPos(Node,KeyNo);
						}
					}
				else {
					// A logical error here because EOF value,
					// must be after any key in the index ...
					NodePos = -1;
					ALastLevelChild = -1;
					AStack->Clear();
                    }
				}
			else
				if (GetNumLevels()>1){
					FreeNode(NodePos);
					NodePos = GetChildPos(Node,1);
					RemainLevels--;
					SetRootNode(NodePos);
					DecNumLevels();
					}
				else {
					RemainLevels--;
					AStack->Push(NodePos,0);
					NodePos = -1;
					ALastLevelChild = GetChildPos(Node,1);
					}
			}
		FreeNodeBlock(Node);
		if ((RemainLevels == 0) && !AStack->Empty()){
			Result = 1;
			}
		}
	return Result;
}

// internal method:
// used when we create an index ...

void TMIndex::CreateFirstNode(void)
{
	void _PTR Node;
	void _PTR Leave;
	long NodePos;
	long LeavePos;

	Node = AllocateNodeBlock();
	Leave = AllocateLeaveBlock();
	FillEOFKey(GetLeaveKey(Leave));
	SetNextLeave(Leave,-1);
	SetPrevLeave(Leave,-1);
	SetDataPos(Leave,-1);
	LeavePos = WriteNewLeave(Leave);
	SetFirstLeave(LeavePos);
		SetLastLeave(LeavePos);//&&
	ResetNode(Node);
	SetNumItems(Node,1);
	FillEOFKey(GetNodeKey(Node,1));
	SetChildPos(Node,1,LeavePos);
	NodePos = WriteNewNode(Node);
	SetRootNode(NodePos);
	FreeLeaveBlock(Leave);
	FreeNodeBlock(Node);
}

// internal method:
// used in algorithm that removes key from tree.

int TMIndex::RemoveKey(long ANodePos,unsigned int ARemoveKeyNo,void _PTRREF AParentKey)
{
	int ResultState = 1;
	void _PTR Node;
	unsigned int KeyNo;
	unsigned int NumItems;
	int LastChanged = 0;
	int RemoveCurrent = 0;
	int SaveCurrent = 0;
	long NodePos;

	AParentKey = NULL;
	Node = AllocateNodeBlock();
	NodePos = ANodePos;
	ReadNode(Node,NodePos);
	NumItems = GetNumItems(Node);
	KeyNo = ARemoveKeyNo;
	if ((KeyNo > NumItems) || (KeyNo < 1)) KeyNo = 0;
	if (KeyNo != 0){
		DeleteItem(Node,KeyNo);
		SaveCurrent = _TRUE;
		if (KeyNo == NumItems) LastChanged = _TRUE;
		NumItems --;
		if (NumItems != 0){
			long NextNodePos;
			if ((NextNodePos = GetNextNode(Node)) != -1){
				void _PTR NextNode;
				unsigned int NextNumItems;
				NextNode = AllocateNodeBlock();
				ReadNode(NextNode,NextNodePos);
				NextNumItems = GetNumItems(NextNode);
				if ((NextNumItems+NumItems)<=(GetMaxItems()/2)){
					long PrevNodePos;
					KeyNo = 1;
					while (KeyNo<=NumItems) {
						InsertItem(NextNode,KeyNo,GetNodeKey(Node,KeyNo),GetChildPos(Node,KeyNo));
						KeyNo ++;
						}
					if ((PrevNodePos = GetPrevNode(Node)) != -1){
						void _PTR PrevNode;
						PrevNode = AllocateNodeBlock();
						ReadNode(PrevNode,PrevNodePos);
						SetNextNode(PrevNode,NextNodePos);
						WriteNode(PrevNode,PrevNodePos);
						FreeNodeBlock(PrevNode);
						}
					SetPrevNode(NextNode,PrevNodePos);
					WriteNode(NextNode,NextNodePos);
					FreeNode(NodePos);
					RemoveCurrent = 1;
					}
				else {
					if (NextNumItems>(NumItems+1)){
						unsigned int I;
						I = ((NextNumItems - NumItems)/2);
						while (I>0) {
							NumItems ++;
							InsertItem(Node,NumItems,GetNodeKey(NextNode,1),GetChildPos(NextNode,1));
							DeleteItem(NextNode,1);
							NextNumItems --;
							I --;
							}
						LastChanged = _TRUE;
						WriteNode(NextNode,NextNodePos);
						}
					else
						if (NumItems>(NextNumItems+1)){
							unsigned int I;
							I = ((NumItems - NextNumItems)/2);
							while (I>0) {
								InsertItem(NextNode,1,GetNodeKey(Node,NumItems),GetChildPos(Node,NumItems));
								DeleteItem(Node,NumItems);
								NextNumItems ++;
								NumItems --;
								I --;
								}
							LastChanged = _TRUE;
							WriteNode(NextNode,NextNodePos);
							}
					}
				FreeNodeBlock(NextNode);
				}
			}
		else {
			long NextNodePos;
			long PrevNodePos;
			NextNodePos = GetNextNode(Node);
			PrevNodePos = GetPrevNode(Node);
			if (NextNodePos != -1){
				void _PTR NextNode;
				NextNode = AllocateNodeBlock();
				ReadNode(NextNode,NextNodePos);
				SetPrevNode(NextNode,PrevNodePos);
				WriteNode(NextNode,NextNodePos);
				FreeNodeBlock(NextNode);
				}
			if (PrevNodePos != -1){
				void _PTR PrevNode;
				PrevNode = AllocateNodeBlock();
				ReadNode(PrevNode,PrevNodePos);
				SetNextNode(PrevNode,NextNodePos);
				WriteNode(PrevNode,PrevNodePos);
				FreeNodeBlock(PrevNode);
				}
			FreeNode(NodePos);
			RemoveCurrent = _TRUE;
			}
		}
	if (RemoveCurrent) ResultState = 3;
	else {
		if (SaveCurrent){
			WriteNode(Node,NodePos);
			if (LastChanged){
				ResultState = 2;
				AParentKey = AllocateKeyBlock();
				MoveBlock(AParentKey,GetNodeKey(Node,GetNumItems(Node)),GetKeySize());
				}
			}
		}
	FreeNodeBlock(Node);
	return ResultState;
}

// internal method:
// used in algorith that append key to tree.

int TMIndex::InsertKey(long ANodePos,void _PTR ANewKey,long ANewChildPos,unsigned int AChangedKeyNo,void *AChangedKeyVal,void _PTRREF AParentKey,void _PTRREF AAditionalKey,long _REF AAditionalChildPos)
{
	int ResultState = -1;
	void _PTR Node;
	unsigned int KeyNo;
	unsigned int NumItems;
	int Comp;

	AParentKey = NULL;
	AAditionalKey = NULL;
	AAditionalChildPos = -1;
	Node = AllocateNodeBlock();
	ReadNode(Node,ANodePos);
	if (AChangedKeyNo != 0){
		SetNodeKey(Node,AChangedKeyNo,AChangedKeyVal);
		}
	KeyNo = 1;
	NumItems = GetNumItems(Node);
	while ((KeyNo<=NumItems) && ((Comp = Compare(ANewKey,GetNodeKey(Node,KeyNo))) == 1)) KeyNo++;
	if (Comp != 0){
		if (NumItems<GetMaxItems()){
			// Simple add new item ...
			InsertItem(Node,KeyNo,ANewKey,ANewChildPos);
			WriteNode(Node,ANodePos);
			if (KeyNo<=NumItems) ResultState = 1;
			else {
				AParentKey = AllocateKeyBlock();
				MoveBlock(AParentKey,ANewKey,GetKeySize());
				ResultState = 2;// Appended in the end of node (and end of file)...
				}
			}
		else {
			long NextNodePos;

			if ((NextNodePos = GetNextNode(Node)) != -1){
				void _PTR NextNode;
				long NextNumItems;
				NextNode = AllocateNodeBlock();
				ReadNode(NextNode,NextNodePos);
				if ((NextNumItems = GetNumItems(NextNode))<GetMaxItems()){
					// Divide items to two groups ...
					if (KeyNo<=NumItems){
						InsertItem(NextNode,1,GetNodeKey(Node,NumItems),GetChildPos(Node,NumItems));
						DeleteItem(Node,NumItems);
						InsertItem(Node,KeyNo,ANewKey,ANewChildPos);
						}
					else {
						InsertItem(NextNode,1,ANewKey,ANewChildPos);
						}
					NextNumItems ++;
					if (CanDelete()){
						unsigned int I;
						I = (NumItems - GetNumItems(NextNode))/ 2;
						while (I>0) {
							InsertItem(NextNode,1,GetNodeKey(Node,NumItems),GetChildPos(Node,NumItems));
							DeleteItem(Node,NumItems);
							NextNumItems ++;
							NumItems --;
							I --;
							}
						}
					AParentKey = AllocateKeyBlock();
					MoveBlock(AParentKey,GetNodeKey(Node,NumItems),GetKeySize());
					WriteNode(Node,ANodePos);
					WriteNode(NextNode,NextNodePos);
					ResultState = 2;
					}
				else {
					// Create new node and divide items to three groups ...
					void _PTR NewNode;
					long NewNodePos;

					NewNode = AllocateNodeBlock();
					ResetNode(NewNode);
					if (KeyNo<=NumItems){
						InsertItem(NewNode,1,GetNodeKey(Node,NumItems),GetChildPos(Node,NumItems));
						DeleteItem(Node,NumItems);
						InsertItem(Node,KeyNo,ANewKey,ANewChildPos);
						}
					else {
						InsertItem(NewNode,1,ANewKey,ANewChildPos);
						}
					if (CanDelete()){
						unsigned int I,NI;
						NI = ((NumItems - 1)/4);
						for (I = 1;I<=NI;I++){
							InsertItem(NewNode,1,GetNodeKey(Node,NumItems),GetChildPos(Node,NumItems));
							DeleteItem(Node,NumItems);
							NumItems --;
							}
						for (I = 1;I<=NI;I++){
							InsertItem(NewNode,GetNumItems(NewNode)+1,GetNodeKey(NextNode,1),GetChildPos(NextNode,1));
							DeleteItem(NextNode,1);
							NextNumItems --;
							}
						}
					SetNextNode(NewNode,NextNodePos);
					SetPrevNode(NewNode,ANodePos);
					NewNodePos = WriteNewNode(NewNode);
					SetNextNode(Node,NewNodePos);
					SetPrevNode(NextNode,NewNodePos);
					WriteNode(Node,ANodePos);
					WriteNode(NextNode,NextNodePos);
					AParentKey = AllocateKeyBlock();
					AAditionalKey = AllocateKeyBlock();
					MoveBlock(AParentKey,GetNodeKey(Node,GetNumItems(Node)),GetKeySize());
					MoveBlock(AAditionalKey,GetNodeKey(NewNode,GetNumItems(NewNode)),GetKeySize());
					AAditionalChildPos = NewNodePos;
					FreeNodeBlock(NewNode);
					ResultState = 3;
					}
				FreeNodeBlock(NextNode);
				}
			else {
				// Create new node and divide items to two groups ...
				void _PTR NewNode;
				long NewNodePos;
				NewNode = AllocateNodeBlock();
				ResetNode(NewNode);
				if (KeyNo<=NumItems){
					InsertItem(NewNode,1,GetNodeKey(Node,NumItems),GetChildPos(Node,NumItems));
					DeleteItem(Node,NumItems);
					InsertItem(Node,KeyNo,ANewKey,ANewChildPos);
					}
				else {
					InsertItem(NewNode,1,ANewKey,ANewChildPos);
					}
				if (CanDelete()){
					unsigned int I;
					I = (NumItems - 1)/ 2;
					while (I>0) {
						InsertItem(NewNode,1,GetNodeKey(Node,NumItems),GetChildPos(Node,NumItems));
						DeleteItem(Node,NumItems);
						NumItems --;
						I --;
						}
					}
				SetNextNode(NewNode,-1);
				SetPrevNode(NewNode,ANodePos);
				NewNodePos = WriteNewNode(NewNode);
				SetNextNode(Node,NewNodePos);
				WriteNode(Node,ANodePos);
				AParentKey = AllocateKeyBlock();
				MoveBlock(AParentKey,GetNodeKey(Node,GetNumItems(Node)),GetKeySize());
				AAditionalKey = AllocateKeyBlock();
				MoveBlock(AAditionalKey,GetNodeKey(NewNode,GetNumItems(NewNode)),GetKeySize());
				AAditionalChildPos = NewNodePos;
				FreeNodeBlock(NewNode);
				ResultState = 3;
				}
			}
		}
	else {
		SetChildPos(Node,KeyNo,ANewChildPos);
		WriteNode(Node,ANodePos);
		ResultState = 1;
		}
	FreeNodeBlock(Node);
	return ResultState;
}

// internal not useful method:
// for gebugging only.

long TMIndex::GetFirstNodeFromLevel(unsigned int ALevel)
{
	void _PTR Node;
	long NodePos;
	unsigned int LevelNo;

	NodePos = GetRootNode();
	if ((LevelNo = ALevel) > 1){
		Node = AllocateNodeBlock();
		while ((LevelNo>1) && (NodePos != -1)){
			unsigned int NI,I;
			ReadNode(Node,NodePos);
			NodePos = GetChildPos(Node,1);
			LevelNo --;
			}
		FreeNodeBlock(Node);
		};
	return NodePos;
}

// internal method:
// debugging method.

long TMIndex::GetFirstBottomNode(void)
{
	void _PTR Node;
	long NodePos;
	unsigned int LevelNo;

	NodePos = GetRootNode();
	if ((LevelNo = GetNumLevels()) > 1){
		Node = AllocateNodeBlock();
		while ((LevelNo>1) && (NodePos != -1)){
			unsigned int NI,I;
			ReadNode(Node,NodePos);
			NodePos = GetChildPos(Node,1);
			LevelNo --;
			}
		FreeNodeBlock(Node);
		};
	return NodePos;
}

// internal method:
// get data position.

long TMIndex::BringLeave(long ALeavePos,void _PTR AKey)
{
	long DataPos;

	if (ALeavePos != -1){
		void _PTR Leave;
		Leave = AllocateLeaveBlock();
		ReadLeave(Leave,ALeavePos);
		SetPosition(ALeavePos,Leave);
		if (AKey != NULL){
			MoveBlock(AKey,GetLeaveKey(Leave),GetKeySize());
			}
		FreeLeaveBlock(Leave);
		DataPos = GetCurrentDataPosition();
		}
	else {
		DataPos = -1;
		}
	return DataPos;
}

// user method:

long TMIndex::GetFirst(void _PTR AKey)
{
	if (AnyError()) return -1L;
	if (GetFirstLeave() != GetLastLeave()) {
		return BringLeave(GetFirstLeave(),AKey);
		}
	else {
		return -1;
		}
}

// user method:

long TMIndex::GetNext(void _PTR AKey)
{
	if (AnyError()) return -1L;
	if ((!GetEOF()) && (GetNextPosition() != -1)) {
		return BringLeave(GetNextPosition(),AKey);
	} else {
		return -1;
		}
}

// user method:

long TMIndex::GetPrev(void _PTR AKey)
{
	if (AnyError()) return -1L;
	if ((!GetBOF()) && (GetPrevPosition() != -1)){
		return BringLeave(GetPrevPosition(),AKey);
	} else {
		return -1;
		}
}

// user method:

long TMIndex::GetCurrent(void _PTR AKey)
{
	if (AnyError()) return -1L;
	return BringLeave(GetCurrentPosition(),AKey);
}

// internal use method:
// find first equal or larger leave.

int TMIndex::FindLeave(void _PTR AKey,long _REF ALeavePos)
{
	void _PTR Node;
	long Result = 0;
	long NodePos;
	unsigned int LevelNo;

	// if not empty file.
	if ((LevelNo = GetNumLevels()) >= 1){
		Node = AllocateNodeBlock();
		NodePos = GetRootNode();
		while ((LevelNo>1) && (NodePos != -1)){
			unsigned int NI,I;
			ReadNode(Node,NodePos);
			NI = GetNumItems(Node);
			I = 1;
			while ((I<=NI) && (Compare(AKey,GetNodeKey(Node,I)) == 1)) I++;
			if (I<=NI) {
				// go one level down.
				NodePos = GetChildPos(Node,I);
				LevelNo --;
				}
			else {
				// this is an error state,
				// because no key value larger than EOF.
				NodePos = -1;
				}
			}

		// get the child of last level.
		if (NodePos != -1){
			unsigned int NI,I;
			ReadNode(Node,NodePos);
			NI = GetNumItems(Node);
			I = 1;
			while ((I <= NI) && (Compare(AKey,GetNodeKey(Node,I)) == 1)) I++;
			if (I <= NI) {
				ALeavePos = GetChildPos(Node,I);
				// Key found.
				if (Compare(AKey,GetNodeKey(Node,I)) == 0)
					Result = 1;
				}
			else {
				// Error state:
				// because no key value larger than EOF.
				ALeavePos = -1;
				}
			}
		FreeNodeBlock(Node);
		}

	return Result;
}

// internal method:
// change leave pointer of key.

long TMIndex::ModifyLeave(void _PTR AKey,long ANewLeavePos)
{
	void _PTR Node;
	long Result = -1;
	long NodePos;
	unsigned int LevelNo;

	// if not empty file.
	if ((LevelNo = GetNumLevels()) >= 1){
		Node = AllocateNodeBlock();
		NodePos = GetRootNode();
		while ((LevelNo>1) && (NodePos != -1)){
			unsigned int NI,I;
			ReadNode(Node,NodePos);
			NI = GetNumItems(Node);
			I = 1;
			while ((I<=NI) && (Compare(AKey,GetNodeKey(Node,I)) == 1)) I++;
			if (I<=NI) {
				// go one level down.
				NodePos = GetChildPos(Node,I);
				LevelNo --;
				}
			else {
				// this is an error state,
				// because no key value larger than EOF.
				NodePos = -1;
				}
			}

		// get the child of last level.
		if (NodePos != -1){
			unsigned int NI,I;
			ReadNode(Node,NodePos);
			NI = GetNumItems(Node);
			I = 1;
			while ((I <= NI) && (Compare(AKey,GetNodeKey(Node,I)) == 1)) I++;
			if (I <= NI)
				if (Compare(AKey,GetNodeKey(Node,I)) == 0){
					// Key found.
					Result = GetChildPos(Node,I);
					SetChildPos(Node,I,ANewLeavePos);
					WriteNode(Node,NodePos);
					}
				else {
					// Error state:
					// because no key value larger than EOF.
					}
			}
		FreeNodeBlock(Node);
		}

	return Result;
}


// user method:
// find first leave with equal key to AKey,
// and bring the data pos if found, or (-1) if no equal,
// current leave is the first equal or first levae after AKey.
long TMIndex::Find(void _PTR AKey)
{
	long LeavePos;
	long DataPos = -1;

	if (AnyError()) return -1L;

	if (FindLeave(AKey,LeavePos)){
		DataPos = BringLeave(LeavePos,NULL);
		}
	else if (LeavePos != -1) {
			BringLeave(LeavePos,NULL);
			}
	return DataPos;
}

long TMIndex::DeleteKeyFromNodes(void _PTR ADeleteKey)
{
	long NodePos;
	long LeavePos;
	int Ok = 0;
	TIndexStack _PTR Stack = new TIndexStack();
	if (FindPath(ADeleteKey,Stack,LeavePos)){
		long NodePos;
		unsigned int KeyNoToRemove;

		Stack->Pop(NodePos,KeyNoToRemove);
		if (KeyNoToRemove != 0){
			void _PTR ParentKey;
			int State;

			while (NodePos != -1){
				State = RemoveKey(NodePos,KeyNoToRemove,ParentKey);
				switch (State){
					case 1:
						// no parent modification,
						// finish process.
						NodePos = -1;
						Ok = 1;
						break;
					case 2:
						// modify last key in the nodes.
						if (!Stack->Empty()){
							void _PTR Node;
							unsigned int KeyNo;
							Node = AllocateNodeBlock();
							do {
								Stack->Pop(NodePos,KeyNo);
								ReadNode(Node,NodePos);
								SetNodeKey(Node,KeyNo,ParentKey);
								WriteNode(Node,NodePos);
								} while((KeyNo == GetNumItems(Node)) && (!Stack->Empty()));
							FreeNodeBlock(Node);
							}
						if (ParentKey != NULL) {
							FreeKeyBlock(ParentKey);
							ParentKey = NULL;
							}
						NodePos = -1;
						Ok = 1;
						break;
					case 3:
						if (!Stack->Empty()){
							Stack->Pop(NodePos,KeyNoToRemove);
							}
						else {
							// here must be an error ...
							NodePos = -1;
							Ok = 1;
							}
						break;
					default:;
					}
				}
			}
		}
	delete Stack;
	if (Ok){
		return LeavePos;
		}
	else {
		return -1L;
		}
}

// user method:
// used to all equal keys to ADeleteKey.

int TMIndex::Delete(void _PTR ADeleteKey)
{
	int Result = 0;
	long LeavePos = DeleteKeyFromNodes(ADeleteKey);

	if (AnyError()) return 0;

	if (LeavePos != -1){
		void _PTR FirstLeave,_PTR TempLeave;
		long FirstLeavePos,TempLeavePos;
		FirstLeave = AllocateLeaveBlock();
		TempLeave = AllocateLeaveBlock();
		TempLeavePos = LeavePos;
		ReadLeave(TempLeave,TempLeavePos);
		FirstLeavePos = GetPrevLeave(TempLeave);
		do {
			FreeLeave(TempLeavePos);
			TempLeavePos = GetNextLeave(TempLeave);
			// if (TempLeavePos = -1) it is an error,
			// because it must not much the last leave (EOF node).
			ReadLeave(TempLeave,TempLeavePos);
			} while ((Compare(GetLeaveKey(TempLeave),ADeleteKey) == 0) && (TempLeavePos != -1));
		// it will be best if we test the (TempLeavePos) value,
		// to be not a (-1).
		// and we can modify of extend compare function by
		// test next leave pos ... (if EOF it must be -1).
		SetPrevLeave(TempLeave,FirstLeavePos);
		WriteLeave(TempLeave,TempLeavePos);
		if (FirstLeavePos != -1){
			ReadLeave(FirstLeave,FirstLeavePos);
			SetNextLeave(FirstLeave,TempLeavePos);
			WriteLeave(FirstLeave,FirstLeavePos);
			}
		else {
			SetFirstLeave(TempLeavePos);
			}
		if (TempLeavePos != GetLastLeave()){
			SetPosition(TempLeavePos,TempLeave);
			}
		else if (FirstLeavePos != -1){
				SetPosition(FirstLeavePos,FirstLeave);
				}
			 else {
				ResetPosition();
				}
		FreeLeaveBlock(FirstLeave);
		FreeLeaveBlock(TempLeave);
		Result = 1;
		};

	return Result;
}

// user method:
// used to Delete current pointed key and return data position.

long TMIndex::DeleteCurrent(void)
{
	long int DataPos = -1;

	if (AnyError()) return -1L;

	if ((Position[CurrentIndex].CurrentLeave != -1) && (Position[CurrentIndex].CurrentLeave != GetLastLeave())){
		void _PTR DeletedLeave = AllocateLeaveBlock();
		void _PTR PrevLeave = AllocateLeaveBlock();
		void _PTR NextLeave = AllocateLeaveBlock();
		void _PTR NewCurrentLeave = NULL;
		long NewCurrentLeavePos = -1;
		int NextEqual = 0;
		int PrevEqual = 0;

		ReadLeave(DeletedLeave,Position[CurrentIndex].CurrentLeave);
		DataPos = GetDataPos(DeletedLeave);

		if (Position[CurrentIndex].PrevLeave != -1){
			ReadLeave(PrevLeave,Position[CurrentIndex].PrevLeave);
			SetNextLeave(PrevLeave,Position[CurrentIndex].NextLeave);
			WriteLeave(PrevLeave,Position[CurrentIndex].PrevLeave);
			PrevEqual = (Compare(GetLeaveKey(PrevLeave),GetLeaveKey(DeletedLeave)) == 0);
			NewCurrentLeave = PrevLeave;
			NewCurrentLeavePos = Position[CurrentIndex].PrevLeave;
			}
		else {
			SetFirstLeave(Position[CurrentIndex].NextLeave);
			}

		if (Position[CurrentIndex].NextLeave != -1){
			ReadLeave(NextLeave,Position[CurrentIndex].NextLeave);
			SetPrevLeave(NextLeave,Position[CurrentIndex].PrevLeave);
			WriteLeave(NextLeave,Position[CurrentIndex].NextLeave);
			NextEqual = (Compare(GetLeaveKey(NextLeave),GetLeaveKey(DeletedLeave)) == 0);
			if (Position[CurrentIndex].NextLeave != GetLastLeave()){
				NewCurrentLeave = NextLeave;
				NewCurrentLeavePos = Position[CurrentIndex].NextLeave;
				};
			};

		FreeLeave(Position[CurrentIndex].CurrentLeave);

		if (!PrevEqual)
			if (NextEqual) {
				// Modify key leave pointer in tree to next ...
				ModifyLeave(GetLeaveKey(DeletedLeave),Position[CurrentIndex].NextLeave);
				}
			else {
				// Remove key from tree ...
				DeleteKeyFromNodes(GetLeaveKey(DeletedLeave));
				}

		if (NewCurrentLeave != NULL) {
			SetPosition(NewCurrentLeavePos,NewCurrentLeave);
			}
		else {
			ResetPosition();
			}

		FreeLeaveBlock(DeletedLeave);
		FreeLeaveBlock(PrevLeave);
		FreeLeaveBlock(NextLeave);
		}
	return DataPos;
}

// user method:
// used to append key to index.

int TMIndex::Append(void _PTR ANewKey,long ANewDataPos)
{
	int Result = 0;
	long NextLeavePos;

	if (AnyError()) return 0;

	TIndexStack _PTR Stack = new TIndexStack();
	if (FindPath(ANewKey,Stack,NextLeavePos)){
		long NodePos;
		unsigned int KeyNo;
		void _PTR Node;
		void _PTR NewLeave,_PTR TempLeave;
		long PrevLeavePos,LeavePos;
		Node = AllocateNodeBlock();
		NewLeave = AllocateLeaveBlock();
		TempLeave = AllocateLeaveBlock();
		// the new leave in the file
		LeavePos = AllocateLeave();
		// put new leave in the front of old equal leaves
		SetNextLeave(NewLeave,NextLeavePos);
		// modify next leave
		ReadLeave(TempLeave,NextLeavePos);
		PrevLeavePos = GetPrevLeave(TempLeave);
		SetPrevLeave(NewLeave,PrevLeavePos);
		SetPrevLeave(TempLeave,LeavePos);
		WriteLeave(TempLeave,NextLeavePos);
		if (PrevLeavePos != -1){
			ReadLeave(TempLeave,PrevLeavePos);
			// Here, must be {GetNextLeave(TempLeave) = NextLeavePos}
			// else an error data ..
			SetNextLeave(TempLeave,LeavePos);
			WriteLeave(TempLeave,PrevLeavePos);
			}
		else {
			// no prev. leave => the new leave is the first leave.
			SetFirstLeave(LeavePos);
			}
		// cmplete new leave data and record it.
		SetLeaveKey(NewLeave,ANewKey);
		SetDataPos(NewLeave,ANewDataPos);
		WriteLeave(NewLeave,LeavePos);
		SetPosition(LeavePos,NewLeave);

		Stack->Pop(NodePos,KeyNo);
		if (KeyNo>0){
			// complete putting the new leave at the front of equals if it be.
			ReadNode(Node,NodePos);
			SetChildPos(Node,KeyNo,LeavePos);
			WriteNode(Node,NodePos);
			}
		else {
			// new key value appending to file.
			// => modify the index tree.
			if (NodePos != -1){
				void _PTR NewKey = NULL;
				void _PTR ChangedKeyVal = NULL;
				void _PTR ParentKey = NULL;
				void _PTR AditionalKey = NULL;
				unsigned int ChangedKeyNo = 0;
				long NewChildPos = -1;
				long AditionalChildPos = -1;

				NewKey = AllocateKeyBlock();
				// NewKey = ANewKey
				MoveBlock(NewKey,ANewKey,GetKeySize());
				NewChildPos = LeavePos;
				// while not finished
				while (NodePos != -1){
					unsigned int State;
					// append the key to node, if there is a place in the node fo new key,
					// or create new node and devide elements between two nodes.
					State = InsertKey(NodePos,NewKey,NewChildPos,
							ChangedKeyNo,ChangedKeyVal,
							ParentKey,AditionalKey,AditionalChildPos);
					if (NewKey != NULL) {
						FreeKeyBlock(NewKey);
						NewKey = NULL;
						NewChildPos = -1;
						}
					if (ChangedKeyVal != NULL) {
						FreeKeyBlock(ChangedKeyVal);
						ChangedKeyVal = NULL;
						ChangedKeyNo = 0;
						}
					switch (State){
						case 1:
							// finish, no parent changes after.
							NodePos = -1;
							Result = 1;
							break;
						case 2:
							// last key in the node modified =>
							// we must modify parent node (only parent key value).
							if (!Stack->Empty()){
								do {
									Stack->Pop(NodePos,ChangedKeyNo);
									ReadNode(Node,NodePos);
									SetNodeKey(Node,ChangedKeyNo,ParentKey);
									WriteNode(Node,NodePos);
									} while((ChangedKeyNo == GetNumItems(Node)) && (!Stack->Empty()));
								}
							if (ParentKey != NULL) {
								FreeKeyBlock(ParentKey);
								ParentKey = NULL;
								}
							// finish modification ...
							NodePos = -1;
							Result = 1;
							break;
						case 3:
							// in this state we must process the creating new node in the tree.
							if (!Stack->Empty()){
								// initialisation of appending new key to parent node,
								// and repeate this loop process.
								Stack->Pop(NodePos,ChangedKeyNo);
								ChangedKeyVal = ParentKey;
								NewKey = AditionalKey;
								NewChildPos = AditionalChildPos;
								}
							else {
								// Create new level ...
								// because of creating new node at rote level.
								// init new root node.
								ResetNode(Node);
								SetNumItems(Node,2);
								SetNodeKey(Node,1,ParentKey);
								SetChildPos(Node,1,NodePos);
								SetNodeKey(Node,2,AditionalKey);
								SetChildPos(Node,2,AditionalChildPos);
								if (ParentKey != NULL) {
									FreeKeyBlock(ParentKey);
									ParentKey = NULL;
									}
								if (AditionalKey != NULL) {
									FreeKeyBlock(AditionalKey);
									AditionalKey = NULL;
									}
								NodePos = WriteNewNode(Node);
								// set new root .
								SetRootNode(NodePos);
								// finsh loop.
								NodePos = -1;
								Result = 1;
								IncNumLevels();
								}
							break;
						// unknown state ...
						default:;
						}
					}
				}
			}
		FreeLeaveBlock(NewLeave);
		FreeLeaveBlock(TempLeave);
		FreeNodeBlock(Node);
		}
	delete Stack;
	return Result;
}

//****************************************************************************

