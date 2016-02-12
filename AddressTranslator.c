#Renzo Tejada
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GET_VPN(VA)				(VA >> 10)						//Before: 7 VPN bits + 10 VPO bits, After: 7 VPN bits 
#define GET_VPO(VA) 			(VA & 0x3FF)					//Mask 7 top VPN bits
#define GET_TLBI(VPN)			(VPN & 0x7)						//Mask 4 top TLBT bits
#define GET_TLBT(VPN)			(VPN >> 3)						//Before: 4 TLBT bits + 3 TLBI bits, After: 4 TLBT bits
#define PACK_PPN_PPO(PPN, PPO) 	((PPN) | (PPO))					//Upper 15 bits + 10 PPO bits(same as VPO)																		
typedef enum{ ILLEGAL, FROM_PT, FROM_TLB, PAGE_FAULT} log_type;
void log_it(log_type,int,int, FILE*);

typedef struct pte{												//Page table entries
	int PPN;													//Physical page number
	char valid;	
} pte_rec, *pte_ptr;
typedef struct tlbe{											//TLB entry
	int tag;
	int PPN;
	char valid;
} tlbe_rec, *tlbe_ptr;

void readPageTableFile(int argc, char **argv, pte_ptr *pageTable);
int readInputVA(int *virtualAddress);												
void bufferOverflowCheck(char *inputString);												
void printPageTable(pte_ptr *table);

char checkPageTable(int virtualAddress, int *physicalAddress, pte_ptr *pageTable);
void createTLB(tlbe_ptr *TLB);
void updateTLB(int virtualAddress, int physicalAddress, tlbe_ptr *TLB);
char checkTLB(int virtualAddress, int *physicalAddress, tlbe_ptr *TLB);
void printTLB(tlbe_ptr *tlb);

main(int argc, char **argv)
{	
	FILE *log_file = NULL;																		//Log file
	log_file = fopen("schung21_lab3.log","w");	

	int virtualAddress;																			//Hold virtual Address user input
	int physicalAddress;																		//Hold physical Address
	char notDone = 1;																			//Exit tag	
	char option;																				//Hold user input validation tag
	pte_ptr *pageTable = (pte_ptr*)malloc(128 * sizeof(pte_ptr));								//Page table array, hold 128 page table entries
	readPageTableFile(argc, argv, pageTable);													//Read page table file and Create page table 
	tlbe_ptr *TLB = (tlbe_ptr*)malloc(8 * sizeof(tlbe_ptr));									//TLB
	createTLB(TLB);																				//Create TLB table
//DEBUG		printPageTable(pageTable);
	while(notDone)
	{
//DEBUG		printTLB(TLB);
		virtualAddress = 0x0;																	//initialize virtual address
		physicalAddress = 0x0;																	//initialize physical address
		printf("Enter a virtual address or -1 to exit:  ");										//Prompt for virtual address	
		option = readInputVA(&virtualAddress);
		if(option == 'I')
		{
			printf("Illegal virtual address\n");												//Illegal virtuall address user input				
			log_it(ILLEGAL,virtualAddress, physicalAddress, log_file);
		}
		else if(option == 'V')																	//Valid User input
		{
			char tlbValid;
			printf("Virtual Address: 0x%x\n", virtualAddress); 
			tlbValid = checkTLB(virtualAddress, &physicalAddress, TLB);							//Check Set,Tag, Valid in TLB
			if(tlbValid)																		//If found in TLB
			{
				printf("Physical Address: 0x%x from the tlb\n", physicalAddress);
				log_it(FROM_TLB,virtualAddress, physicalAddress, log_file);
			}
			else if( checkPageTable(virtualAddress, &physicalAddress, pageTable) )				//Not in TLB check VPN Page Table
			{
				printf("Physical Address: 0x%x from the page table\n", physicalAddress);
				updateTLB(virtualAddress, physicalAddress, TLB);								//Update TLB table 
				log_it(FROM_PT,virtualAddress, physicalAddress, log_file);
			}
			else	
			{																			//Else processor takes page fault
				printf("Page fault\n");											
				log_it(PAGE_FAULT,virtualAddress, physicalAddress, log_file);
			}
		}
		else																					//Option == 'E' exit program
			notDone = 0;																		
	} 
	fclose(log_file);																			//Close log file

	int i;
	for(i = 0; i < 128; i++)
		free(pageTable[i]);																		//Free Page Table entries
	free(pageTable);																			//Free Page Table
	for(i = 0; i < 8; i++)
		free(TLB[i]);																			//Free TLB entries
	free(TLB);																					//Free TLB
}
void readPageTableFile(int argc, char **argv, pte_ptr *pageTable)
{
	FILE *pageTableFile;					
	char *fileName;
	if(argc != 2)																				//Not valid command line input
	{
		printf("Usage: %s page_table_file\n", argv[0]);
	}
	else
	{
		fileName = argv[1];
		fileName = strcat(fileName,".pt");														//Add file type to the file Name
	}
	if( pageTableFile = fopen(fileName,"r"))
	{
		int index;																				//count entries total 128
		int PPN;
		int valid;
		for(index = 0; index < 128; index++)
		{
			fscanf(pageTableFile, "%d %d", &valid, &PPN);										//Read entries from page table File
			pte_ptr newPTEntry = (pte_ptr)malloc(sizeof(pte_rec));								//Allocate memory for page table entries
			newPTEntry->PPN = PPN;																//Assign PPN
			newPTEntry->valid = valid;															//Assign valid
			pageTable[index] = newPTEntry;														//Save PTE pointer into pageTable
		}
		fclose(pageTableFile);																	//Close file
	}
	else
		printf("Unknown page table file\n");
}
int readInputVA(int *virtualAddress)															//Read user input (Virtual address or -1 to exit)
{						
	char inputString[10];																		//Length 7:  |1|2|3|4|5|6|\n| 
	char option = 'V';																			// Option: V = valid, I = illegal, E = exit
	if(fgets(inputString, sizeof(inputString), stdin))											//fgets stops when either (n-1) characters are read,
	{																							//newline character is read, or endOfFile reached.
		bufferOverflowCheck(inputString);														//Check buffer overflow in stdin
//DEBUG		printf("7:%c 8:%c\n", inputString[5], inputString[6]);
		if(sscanf(inputString, "%d", virtualAddress))											//Success, else virtualAddress is 0x0 
		{
			if( (*virtualAddress > 131071) || (*virtualAddress < -1))							//Range 0 ~ 131071		
				option = 'I';																	//Illegal input
			else if(*virtualAddress == -1)
				option = 'E';																	//Input -1: exit
		}																						//else if sscanf fail, char input virtualAddress stay 0x0
	}
	else																						//fgets fail, Illegal virtual address -2
		option = 'I';																			//Illegal input (fgets fail)	
	return option;
}
void bufferOverflowCheck(char *inputString)														//Check if we need to clear stdin buffer
{
	int length = strlen(inputString);															//Get string length
	if( inputString[length-1] != '\n')															//Last characher is not \n <<< We have problem here >>>
	{
		char trashHolder;
		while( (trashHolder = getchar()) != '\n' );												//Read trash left in stdin buffer untill we read \n
	}
}
char checkPageTable(int virtualAddress, int *physicalAddress, pte_ptr *pageTable)
{
	char valid = pageTable[GET_VPN(virtualAddress)]->valid;										//Hold valid tag
	if(valid)
	{
		int PPN = pageTable[GET_VPN(virtualAddress)]->PPN;										//Get Physical Page Number from Page Table
		PPN = PPN << 10;																		//PPN will be left 15 bits with 10 bits PPO
		*physicalAddress = PACK_PPN_PPO(PPN, GET_VPO(virtualAddress));							//Combine PPN+PPO, PPO and VPO are the same
	}	
	return valid;	
}
void createTLB(tlbe_ptr *TLB)
{
	int index;																					//Count entries total 8
	for(index = 0; index < 8; index++)
	{
		tlbe_ptr newTLBE = (tlbe_ptr)malloc(sizeof(tlbe_rec));									//Allocate memory for TLB entries
		newTLBE->valid = 0;																		//Initialize to invalid
		newTLBE->tag = -1;																		//Initialize tag to -1 
		newTLBE->PPN = -1;																		//Initialize PPN to -1
		TLB[index] = newTLBE;																	//Save TLB entry into TLB		
	}
}
char checkTLB(int virtualAddress, int *physicalAddress, tlbe_ptr *TLB)
{
	int tag = TLB[ GET_TLBI( GET_VPN(virtualAddress) ) ]->tag;									//Get TLB entry Tag
	char valid = 0;
	if( tag == GET_TLBT( GET_VPN(virtualAddress) ) )											//Check if tags are the same
	{
		valid =	TLB[ GET_TLBI( GET_VPN(virtualAddress) ) ]->valid;								//Get TLB entry valid
		if(valid)																				//Check if entry is valid
		{
			int PPN = TLB[ GET_TLBI( GET_VPN(virtualAddress) ) ]->PPN;								//Get TLB entry PPN
			PPN = PPN << 10;																		//PPN will be left 15 bits with 10 bits PPO
			*physicalAddress = PACK_PPN_PPO(PPN, GET_VPO(virtualAddress));							//Combine PPN+PPO, PPO and VPO are the same
		}
	}
	return valid;	
}
void updateTLB(int virtualAddress, int physicalAddress, tlbe_ptr *TLB)
{
	int TLBT = GET_TLBT( GET_VPN(virtualAddress) );													//Get TLB tag
	int TLBI = GET_TLBI( GET_VPN(virtualAddress) );													//Get TLB set
	int PPN = GET_VPN(physicalAddress);															//Since PPO and VPO are the same we can just use GET_VPN
	TLB[TLBI]->PPN = PPN;																		//Update entry PPN
	TLB[TLBI]->tag = TLBT;																		//Update entry tag
	TLB[TLBI]->valid = 1;																		//Update entry valid
}
void log_it(log_type entry, int va, int pa, FILE *log_file) {
/* param1:  event type
 *  * param2:  virtual address
 *   * param3:  physical address (ignored if ILLEGAL or PAGE_FAULT event)
 *    */
   switch(entry) {
      case ILLEGAL:  fprintf(log_file,"Illegal address: 0x%x\n",va); break;
      case FROM_PT:  fprintf(log_file,"Page table maps 0x%x to 0x%x\n",va,pa); break;
      case FROM_TLB:  fprintf(log_file,"TLB maps 0x%x to 0x%x\n",va,pa); break;
      case PAGE_FAULT:  fprintf(log_file,"Page fault: 0x%x\n",va); break;
   }
}
void printPageTable(pte_ptr *table)
{
	int i;
	for(i = 0; i < 128; i++)
		printf("VPN:%d PPN:%d Valid:%d\n", i, table[i]->PPN, table[i]->valid);
}
void printTLB(tlbe_ptr *tlb)
{
	int i;
	for(i = 0; i < 8; i++)
		printf("Set: %d Tag: %d PPN: %d Valid: %d\n", i, tlb[i]->tag, tlb[i]->PPN, tlb[i]->valid);
}
