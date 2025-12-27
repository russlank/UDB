#include "EMDX.H"
#include <conio.h>
#include <iostream.h>
// void DisplayLevels(TMIndex *AIndex);

/*
void DisplayLeaves(TMIndex *AIndex)
{
	long LeavePos;
	void *Leave;
	Leave = AIndex->AllocateLeaveBlock();
	LeavePos = AIndex->GetFirstLeave();
	cout << "\n[";
	while (LeavePos != -1){
		AIndex->ReadLeave(Leave,LeavePos);
		LeavePos = AIndex->GetNextLeave(Leave);
		cout << (char *)AIndex->GetLeaveKey(Leave);
		if (LeavePos != -1) cout << '-';
		else cout << ']';
		}
	AIndex->FreeLeaveBlock(Leave);
}

void DisplayIndexNodes(TMIndex *AIndex,long ANodePos)
{
	void *Node;
	long NodePos;
	long NodeNo;
	NodePos = ANodePos;
	NodeNo = 1;
	while (NodePos != -1){
		cout << "\n  " << NodeNo << ". ";
		Node = AIndex->AllocateNodeBlock();
		AIndex->ReadNode(Node,NodePos);
		AIndex->DisplayNodeData(Node);
		NodePos = AIndex->GetNextNode(Node);
		AIndex->FreeNodeBlock(Node);
		NodeNo ++;
		}
}

void DisplayLevels(TMIndex *AIndex)
{
	unsigned int I;
	for (I=1;I<=AIndex->GetNumLevels();I++){
		long FN;
		FN = AIndex->GetFirstNodeFromLevel(I);
		cout << "\nLevel : #"<< I<<" ............................................";
		DisplayIndexNodes(AIndex,FN);
		}
	cout << "\n.......................................................";
#ifdef __HUGE__
	cout << "\nFree memory :"<< farcoreleft();
#else
	cout << "\nFree memory :"<< coreleft();
#endif
}
*/
void Commander(void)
{
	TMIndex *Index;
	char Command[100];
	char Parameter[100];
	int ComCode = 0;
	long Counter = 1;
	long DPos;
	long Num;

	cout << "\nEnter C to create or O to open >";
	memset(Command,0,100);
	cin >> Command;
	if ((Command[0] == 'c') || (Command[0] == 'C')){
		Index = new TMIndex("TEST.NDX",5);
		Index->SetActiveIndex(1);
		Index->InitIndex(ftSTRING,20,attDELETE,7,50,150);
		Index->SetActiveIndex(2);
		Index->InitIndex(ftSTRING,20,attDELETE,5,50,150);
		Index->SetActiveIndex(3);
		Index->InitIndex(ftSTRING,20,attDELETE,3,50,150);
		Index->SetActiveIndex(4);
		Index->InitIndex(ftSTRING,20,attDELETE,11,50,150);
		Index->SetActiveIndex(5);
		Index->InitIndex(ftSTRING,20,attDELETE,25,50,150);
	}
	else {
		Index = new TMIndex("TEST.NDX");
	}
	/*
	do {
		cout << "\nEnter command (C=Create,O=Open) :>";
		memset(Command,0,100);
		cin >> Command;
		switch (Command[0]) {
			case 'c':;
			case 'C':
				unsigned int NumItems;
				cout << "\nEnter num keys in node :";
				cin >> NumItems;
				Index = new TIndex("E:\TEST.NDX",ftSTRING,20,attDELETE,NumItems,100);
				ComCode = 0;
				break;
			case 'o':;
			case 'O':
				Index = new TIndex("E:\TEST.NDX");
				ComCode = 0;
				break;
			default:
				ComCode = 1;
			}
		} while (ComCode != 0);
	*/
	if (1)
	do {

		cout << "\nError code :" << Index->GetError();
		cout << "\n('A'=Add,'D'=Delete,'S'=Show,'F'=Find,'X'=Exit,'N'=Fill,'C'=Change,'L'=Show leaves,'+'=Next,'-'=Prev,'.'=First,'T'=Current,'R'=Remove current,'M'=Mem) \nEnter command :>";
		memset(Command,0,100);
		cin >> Command;
		switch (Command[0]) {
			case 'm':;
			case 'M':
				cout << "\nNear mem. free :" << coreleft();
				cout << "\nFar mem. free :" << farcoreleft();
                cout << "\n\n";
				break;
			case 'l':;
			case 'L':
				memset(Parameter,0,100);
				DPos = Index->GetFirst(Parameter);
				cout << "\n";
				if (DPos != -1 && !Index->GetEOF()){
					char c;
					do {
						cout << Parameter << ".";
						Index->GetNext(Parameter);
						if (kbhit()) break;
						} while (!Index->GetEOF());
						cout << Parameter ;
					}
				else cout << Parameter ;
				break;
			case 'c':;
			case 'C':
				cout << "\nEnnter index code :";
				cin >> Num;
				Index->SetActiveIndex(Num);
				break;
			case 'r':;
			case 'R':
				if (Index->DeleteCurrent() != -1){
					cout << "\n Removed ...\n";
					}
				else {
					cout << "\n Not removed ...\n";
					}
				break;
			case 'n':;
			case 'N':
				memset(Parameter,0,100);
				cin >> Num;
				for (Counter=1;Counter<=Num;Counter ++){
					ltoa(Counter,Parameter,10);
					Index->Append(Parameter,Counter);
					cout << Counter << "     " << (char)13;
					}
				ComCode = 0;
				break;
			case 'a':;
			case 'A':
				memset(Parameter,0,100);
				cin >> Parameter;
				Index->Append(Parameter,Counter);
				Counter ++;
				ComCode = 0;
				break;
			case 'd':;
			case 'D':
				memset(Parameter,0,100);
				cin >> Parameter;
				Index->Delete(Parameter);
				ComCode = 0;
				break;
			case 'f':;
			case 'F':
				memset(Parameter,0,100);
				cin >> Parameter;
				DPos = Index->Find(Parameter);
				if (DPos != -1) cout <<"\nThe key :"<<Parameter<<"   Data pos ="<<DPos<<"\n";
				else cout <<"\nThe key :"<<Parameter<<"   Not found\n";
				break;
			case 't':;
			case 'T':
				memset(Parameter,0,100);
				DPos = Index->GetCurrent(Parameter);
				if (DPos != -1) cout <<"\nThe key :"<<Parameter<<"   Data pos ="<<DPos<<"\n";
				else cout <<"\nNot found\n";
				break;
			case '.':
				memset(Parameter,0,100);
				DPos = Index->GetFirst(Parameter);
				if (DPos != -1) cout <<"\nThe key :"<<Parameter<<"   Data pos ="<<DPos<<"\n";
				else cout <<"\nNot found\n";
				break;
			case '+':
				if (Index->GetEOF()) {
					cout << "\nEnd of file ...\n";
					}
				else {
					memset(Parameter,0,100);
					DPos = Index->GetNext(Parameter);
					if (DPos != -1) cout <<"\nThe key :"<<Parameter<<"   Data pos ="<<DPos<<"\n";
					else cout << "\nNot found\n";
					};
				break;
			case '-':
				if (Index->GetBOF()) {
					cout << "\nBegin of file ...\n";
					}
				else {
					memset(Parameter,0,100);
					DPos = Index->GetPrev(Parameter);
					if (DPos != -1) cout <<"\nThe key :"<<Parameter<<"   Data pos ="<<DPos<<"\n";
					else cout <<"\nThe key :"<<Parameter<<"   Not found\n";
					}
				break;
			case 's':;
			case 'S':
				// DisplayLevels(Index);
				ComCode = 0;
				break;
			case 'x':;
			case 'X':
				ComCode = 1;
				break;
			default:
				ComCode = 0;
				break;
			}
		} while (ComCode != 1);
	else cout << "\nCan`t open or create index file ....\n";
	delete Index;
}

main()
{
	Commander();
	return 0;
}


