#define WIN31
#include <string.h>
#include <alloc.h>
#include <windows.h>
#include "emdx.h"

#define NUMINDEXES	256

typedef struct tagSTRUCT1 {
	TMIndex *MDX;
	int	Next;
    } STRUCT1;


STRUCT1 Index[NUMINDEXES];
int FirstFree = -1;

inline int TestHandle(int MDXHandle){
	if ((MDXHandle > -1) && (MDXHandle < NUMINDEXES)) {
		if (Index[MDXHandle].MDX != NULL)
			return 1;
        return 0;
	}
	else  {
    	return 0;
    }
}

void InitLib(void)
{
	int i;
	for ( i = 0; i < (NUMINDEXES - 1); i++){
		Index[i].MDX = NULL;
		Index[i].Next = i + 1;
        };
	Index[NUMINDEXES - 1].Next = -1;
    FirstFree = 0;
}

void CleanUp(void)
{
    int Temp;
	for (Temp = 0; Temp < NUMINDEXES; Temp ++) {
		if ((Index[Temp].MDX != NULL) && (Index[Temp].Next == -1)){ 
			delete Index[Temp].MDX;
			Index[Temp].MDX = NULL;
			Index[Temp].Next = FirstFree;
			FirstFree = Temp;
        }
    }
}

#pragma argsused

int FAR PASCAL LibMain( HANDLE hInstance,
						WORD wDataSeg,
						WORD wHeapSize,
						LPSTR lpCmdLine)
{
    InitLib();
	if ( wHeapSize != 0 )
		UnlockData( 0 );
	return 1;  
}

#pragma argsused

int FAR PASCAL WEP(int nParameter)
{
    CleanUp();
	return 1;
}


void FAR PASCAL _export DrawCompanyName(HDC PaintDC,int X,int Y,int H,int ExtX,int ExtY)
{
	int OldMapMode = SetMapMode(PaintDC,MM_ANISOTROPIC);
	int OldBKMode = SetBkMode(PaintDC,TRANSPARENT);
	int OldTextColor = GetTextColor(PaintDC);
	SIZE OldExt;
	const char *Temp = "Ultra";
	const int Len = strlen(Temp);
	DWORD TempExtent;
    int TempX = X;//TempY = Y;
	HPEN Pen,OldPen;
    HBRUSH Brush1,Brush2,OldBrush;
    HFONT Font,OldFont;

	
	SetViewportExtEx(PaintDC,ExtX,ExtY,&OldExt);
	Font = CreateFont(H,0,0,0,FW_BOLD,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_TT_ALWAYS,PROOF_QUALITY,FF_MODERN || VARIABLE_PITCH,"Times");
	Pen = CreatePen(PS_SOLID,1,RGB(0,0,0));
	Brush1 = CreateSolidBrush(RGB(255,0,0));
	Brush2 = CreateSolidBrush(RGB(0,0,0));

    OldFont = SelectObject(PaintDC,Font);
	TempExtent = GetTextExtent(PaintDC,Temp,Len);
	OldPen = SelectObject(PaintDC,Pen);
	SetBkColor(PaintDC,RGB(255,0,0));
    OldBrush = SelectObject(PaintDC,Brush1);
	Rectangle(PaintDC,X,Y,X+LOWORD(TempExtent),Y+HIWORD(TempExtent));
	TempExtent = GetTextExtent(PaintDC,Temp,1);
	TempX += LOWORD(TempExtent);
	SetBkColor(PaintDC,RGB(0,0,0));
    SelectObject(PaintDC,Brush2);
	Rectangle(PaintDC,X,Y,X+LOWORD(TempExtent),Y+HIWORD(TempExtent));

	SetTextColor(PaintDC,RGB(255,0,0));
	TextOut(PaintDC,X,Y,Temp,1);
	
	SetTextColor(PaintDC,RGB(0,0,0));
	TextOut(PaintDC,TempX,Y,Temp+1,Len-1);

    SelectObject(PaintDC,OldBrush);
	SelectObject(PaintDC,OldPen);
    SelectObject(PaintDC,OldFont);
	DeleteObject(Pen);
	DeleteObject(Brush1);
    DeleteObject(Brush2);
    DeleteObject(Font);
	SetViewportExt(PaintDC,OldExt.cx,OldExt.cy);
    SetTextColor(PaintDC,OldTextColor);
	SetMapMode(PaintDC,OldMapMode);
    SetBkMode(PaintDC,OldBKMode);
}

int GetHandle(void)
{
	if (FirstFree != -1) {
		int ResultHandle = FirstFree;
		FirstFree = Index[ResultHandle].Next;
		Index[ResultHandle].Next = -1;
        return ResultHandle;
		}
	else {
		return -1;
    	}
}

void FreeHandle(int MDXHandle)
{
	if ((MDXHandle > -1) && (MDXHandle < NUMINDEXES)) {
		if (Index[MDXHandle].Next == -1) {
			Index[MDXHandle].Next = FirstFree;
			Index[MDXHandle].MDX = NULL;
			FirstFree = MDXHandle;
		};
	};
}


// Exports Multi Index File Managment Functions ....



void FAR PASCAL _export MDXClearError(int MDXHandle)
{
	if (TestHandle(MDXHandle))
		Index[MDXHandle].MDX -> ClearError();
}

int FAR PASCAL _export MDXGetError(int MDXHandle)
{
	if (TestHandle(MDXHandle)){
		return Index[MDXHandle].MDX -> GetError();
	}
	else {
		return -1;
	}
}

void FAR PASCAL _export MDXSetError(int MDXHandle,int AError)
{
	if (TestHandle(MDXHandle))
		Index[MDXHandle].MDX -> SetError(AError);
}

int FAR PASCAL _export MDXCreateFile(char far *IndexFileName,int NumIndexes)
{
	int NewHandle = GetHandle();
	if (NewHandle > -1){
		Index[NewHandle].MDX = new TMIndex(IndexFileName,NumIndexes);
		}
	return NewHandle;
}

int FAR PASCAL _export MDXOpenFile(char far *IndexFileName)
{
	int NewHandle = GetHandle();
	if (NewHandle > -1){
		Index[NewHandle].MDX = new TMIndex(IndexFileName);
		}
	return NewHandle;
}

void FAR PASCAL _export MDXCloseFile(int MDXHandle)
{
	if (TestHandle(MDXHandle)){
		delete Index[MDXHandle].MDX;
		FreeHandle(MDXHandle);
	}
}

void FAR PASCAL _export MDXCLoseAll(void)
{
	CleanUp();
}

void FAR PASCAL _export MDXFlushFile(int MDXHandle)
{
	if (TestHandle(MDXHandle))
		Index[MDXHandle].MDX -> FlushFile();
}

void FAR PASCAL _export MDXCreateIndex(int MDXHandle,
							const unsigned int AKeyCode,
							const unsigned int AKeySize,
							unsigned int AAttrib,
							const unsigned int ANumItems,
							const long AFreeCreateNode,
							const long AFreeCreateLeave)
{
	if (TestHandle(MDXHandle))
		Index[MDXHandle].MDX->InitIndex(AKeyCode,AKeySize,AAttrib,ANumItems,AFreeCreateNode,AFreeCreateLeave);
}

void FAR PASCAL _export MDXAppend(int MDXHandle,void far *AKey,long ADataPos)
{
	if (TestHandle(MDXHandle))
    	Index[MDXHandle].MDX -> Append(AKey,ADataPos);
}

long FAR PASCAL _export MDXFind(int MDXHandle,void far *AKey)
{
    long Pos = -1;
	if (TestHandle(MDXHandle))
    	Pos = Index[MDXHandle].MDX -> Find(AKey);
    return Pos;
}


int FAR PASCAL _export MDXUnque(int MDXHandle)
{
	int Result = 0;
	if (TestHandle(MDXHandle))
		Result = Index[MDXHandle].MDX -> Unque();
    return Result;
}

unsigned int FAR PASCAL _export MDXGetNumIndexes(int MDXHandle)
{
	unsigned int Result = 0;
	if (TestHandle(MDXHandle))
		Result = Index[MDXHandle].MDX -> GetNumIndexes();
    return Result;
}

unsigned int FAR PASCAL _export MDXGetKeyType(int MDXHandle)
{
	unsigned int Result = 0;
	if (TestHandle(MDXHandle))
		Result = Index[MDXHandle].MDX -> GetKeyType();
    return Result;
}

unsigned int FAR PASCAL _export MDXGetKeySize(int MDXHandle)
{
	unsigned int Result = 0;
	if (TestHandle(MDXHandle))
		Result = Index[MDXHandle].MDX -> GetKeySize();
    return Result;
}

void FAR PASCAL _export MDXSetActiveIndex(int MDXHandle, unsigned int AIndexNo)
{
	if (TestHandle(MDXHandle))
		Index[MDXHandle].MDX -> SetActiveIndex(AIndexNo);
}

int FAR PASCAL _export MDXCompare(int MDXHandle, void far *AKey1, void far *AKey2)
{
    int Result = 0;
	if (TestHandle(MDXHandle))
		Result = Index[MDXHandle].MDX -> Compare(AKey1, AKey2);
    return Result;
}

int FAR PASCAL _export MDXGetEOF(int MDXHandle)
{
    int Result = 0;
	if (TestHandle(MDXHandle))
		Result = Index[MDXHandle].MDX -> GetEOF();
    return Result;
}

int FAR PASCAL _export MDXGetBOF(int MDXHandle)
{
    int Result = 0;
	if (TestHandle(MDXHandle))
		Result = Index[MDXHandle].MDX -> GetBOF();
    return Result;
}

long FAR PASCAL _export MDXGetFirst(int MDXHandle, void far *AKey)
{
    long Result = 0;
	if (TestHandle(MDXHandle))
		Result = Index[MDXHandle].MDX -> GetFirst(AKey);
    return Result;
}

long FAR PASCAL _export MDXGetNext(int MDXHandle, void far *AKey)
{
    long Result = 0;
	if (TestHandle(MDXHandle))
		Result = Index[MDXHandle].MDX -> GetNext(AKey);
    return Result;
}

long FAR PASCAL _export MDXGetPrev(int MDXHandle, void far *AKey)
{
    long Result = 0;
	if (TestHandle(MDXHandle))
		Result = Index[MDXHandle].MDX -> GetPrev(AKey);
    return Result;
}

long FAR PASCAL _export MDXGetCurrent(int MDXHandle, void far *AKey)
{
    long Result = 0;
	if (TestHandle(MDXHandle))
		Result = Index[MDXHandle].MDX -> GetCurrent(AKey);
    return Result;
}

int FAR PASCAL _export MDXDelete(int MDXHandle, void far *AKey)
{
    int Result = 0;
	if (TestHandle(MDXHandle))
		Result = Index[MDXHandle].MDX -> Delete(AKey);
    return Result;
}

long FAR PASCAL _export MDXDeleteCurrent(int MDXHandle)
{
    long Result = 0;
	if (TestHandle(MDXHandle))
		Result = Index[MDXHandle].MDX -> DeleteCurrent();
    return Result;
}

int FAR PASCAL _export MDXCanDelete(int MDXHandle)
{
	int Result = 0;
	if (TestHandle(MDXHandle))
		Result = Index[MDXHandle].MDX -> CanDelete();
    return Result;
}
