#include "HookFunctions.h"


HookFunctions::HookFunctions(void)
{
	//this->functionsMap.insert( std::pair<string,int>("VirtualAlloc",VIRTUALALLOC_INDEX) );
	this->functionsMap.insert( std::pair<string,int>("VirtualFree",VIRTUALFREE_INDEX) );

	this->functionsMap.insert( std::pair<string,int>("RtlAllocateHeap",RTLALLOCATEHEAP_INDEX) );
	this->functionsMap.insert( std::pair<string,int>("IsDebuggerPresent",ISDEBUGGERPRESENT_INDEX) );

	this->functionsMap.insert( std::pair<string,int>("RtlReAllocateHeap",RTLREALLOCATEHEAP_INDEX) );

	//this->functionsMap.insert( std::pair<string,int>("MapViewOfFile",MAPVIEWOFFILE_INDEX) );   //hookata la syscall NtMapViewOfSection 

	this->functionsMap.insert( std::pair<string,int>("VirtualQuery",VIRTUALQUERY_INDEX) );
	this->functionsMap.insert( std::pair<string,int>("VirtualProtect",VIRTUALPROTECT_INDEX) );
	this->functionsMap.insert( std::pair<string,int>("VirtualQueryEx",VIRTUALQUERYEX_INDEX) );

	//this->functionsMap.insert( std::pair<string,int>("ZwSetInformationThread",SETINFOTHREAD_INDEX) );

	this->functionsMap.insert( std::pair<string,int>("SetUnhandledExceptionFilter",UNHANDLED_INDEX) );

	
	//this->functionsMap.insert( std::pair<string,int>("RtlEncodePointer",UNHANDLED_INDEX) );


	// DEBUGGING OBSIDIUM -----
	/*
	this->functionsMap.insert( std::pair<string,int>("CreateFileW",ANOTHERHOOK) );

	this->functionsMap.insert( std::pair<string,int>("RegOpenKeyExW",ANOTHERHOOK2));

	this->functionsMap.insert( std::pair<string,int>("RegGetValueW",ANOTHERHOOK3));

	this->functionsMap.insert( std::pair<string,int>("RegQueryValueExW",ANOTHERHOOK3));

	this->functionsMap.insert( std::pair<string,int>("GetEnvironmentStringsW",ANOTHERHOOK4));

	this->functionsMap.insert( std::pair<string,int>("memcpy",ANOTHERHOOK5));
	*/	
}


HookFunctions::~HookFunctions(void)
{
	
}



//----------------------------- HOOKED FUNCTIONS -----------------------------//

// hook the VirtualAlloc() in order to retrieve the memory range allocated and build ours data structures
// NOT USED ANYMORE, WE HOOKED THE NtAllocateVirtualMemory syscall in order to be more generic ( see HookSyscalls.cpp row 126 )
VOID VirtualAllocHook(UINT32 virtual_alloc_size , UINT32 ret_heap_address ){
  
	ProcInfo *proc_info = ProcInfo::getInstance();

	HeapZone hz;
	hz.begin = ret_heap_address;
	hz.size = virtual_alloc_size;
	hz.end = ret_heap_address + virtual_alloc_size;
  
	MYINFO("Virtualloc insert in Heap Zone %08x -> %08x",hz.begin,hz.end);

	//saving this heap zone in the map inside ProcInfo
	proc_info->insertHeapZone(hz); 

}

//hook the  HeapAllocHook() in order to retrieve the memory range allocated and build ours data structures
static HeapZone prev_heap_alloc;
VOID RtlAllocateHeapHook(int heap_alloc_size , UINT32 ret_heap_address ){
	 
	if (heap_alloc_size == 0 ){
		return;
	}
	
	ProcInfo *proc_info = ProcInfo::getInstance();
	//need this code because sometimes RTLAllocHeap is invoked twice (because of the IPOINT_AFTER insert)and the second time is the correct one
	if (prev_heap_alloc.begin == ret_heap_address){
		proc_info->removeLastHeapZone();
	
	}

	HeapZone hz;
	hz.begin = ret_heap_address;
	hz.size = heap_alloc_size;
	hz.end = ret_heap_address + heap_alloc_size;
	prev_heap_alloc =hz;
	
	//MYINFO("AllocateHeap insert in Heap Zone %08x -> %08x",hz.begin,hz.end);
	//saving this heap zone in the map inside ProcInfo
	proc_info->insertHeapZone(hz); 

}



VOID RtlReAllocateHeapHook(ADDRINT heap_address, UINT32 size ){
	
	ProcInfo *proc_info = ProcInfo::getInstance();

	HeapZone hz;
	hz.begin = heap_address;
	hz.size = size;
	hz.end = heap_address + size;
	//MYINFO("ReAllocateHeap insert in Heap Zone %08x -> %08x",hz.begin,hz.end);

	//saving this heap zone in the map inside ProcInfo
	proc_info->insertHeapZone(hz); 
}


VOID MapViewOfFileHookAfter(W::DWORD dwDesiredAccess,W::DWORD dwFileOffsetHigh, W::DWORD dwFileOffsetLow, UINT32 size,ADDRINT file_view_addr ){
	MYINFO("Found After mapViewOfFile Access %08x OffsetHigh %08x OffsetLow %08x  at %08x of size %08x ",dwDesiredAccess,dwFileOffsetHigh,dwFileOffsetLow,file_view_addr,size);
	ProcInfo *proc_info = ProcInfo::getInstance();
	proc_info->addMappedFilesAddress(file_view_addr);
}

VOID VirtualFreeHook(UINT32 address_to_free){

	MYINFO("Have to free the address %08x\n" , address_to_free);

	ProcInfo *pInfo = ProcInfo::getInstance();
	std::vector<HeapZone> HeapMap = pInfo->getHeapMap();

	int index_to_remove = -1;

	MYINFO("HeapZone before free");
	//pInfo->printHeapList();

	for(unsigned index=0; index <  HeapMap.size(); index++) {
		if(address_to_free == pInfo->getHeapZoneByIndex(index)->begin){
			index_to_remove = index;
		}
	}

	if(index_to_remove != -1){
		pInfo->deleteHeapZone(index_to_remove);
	}

	MYINFO("HeapZone after free");
	//pInfo->printHeapList();

}

//REMEMBER!!! : PIN wants a function pointer in the AFUNCPTR agument!!!
//avoid the detection of the debugger replacing the function IsDebuggerPresent() with a new one that returns always false
//very basic way to avoid this anti-debugging technique
bool * IsDebuggerPresentHook(){
	return false;
}

VOID VirtualQueryHook (W::LPCVOID baseAddress, W::PMEMORY_BASIC_INFORMATION mbi, W::SIZE_T *numBytes) {
	FakeMemoryHandler* fake_memory_handler = new FakeMemoryHandler();
	if (!fake_memory_handler->isAddrInWhiteList((ADDRINT)baseAddress) && numBytes && mbi) {
		*numBytes = 0;
		mbi->State = MEM_FREE;
	}
}

VOID VirtualProtectHook (W::LPVOID baseAddress, W::DWORD size, W::PDWORD oldProtection, BOOL* success) {
	MYINFO("calling Virutalprotect at address %08x ->  %08x",(ADDRINT)baseAddress,size + (ADDRINT)baseAddress);
	FakeMemoryHandler* fake_memory_handler = new FakeMemoryHandler();
	if (!fake_memory_handler->isAddrInWhiteList((ADDRINT)baseAddress) && success && *success && oldProtection) {
		*success = 0;
		*oldProtection = NULL;
	}
	MYINFO("calling Virutalprotect at address %08x ->  %08x",(ADDRINT)baseAddress,size + (ADDRINT)baseAddress);
}

/*
VOID AnotherHook2 ( W::LPCWSTR key_name) {
	
	MYINFO("Opening a reg-key  %S\n" , key_name);

	W::WCHAR new_name = (W::WCHAR)"c";

	file_name = &new_name;
}


VOID AnotherHook3 ( W::LPCWSTR lib_name) {
	
	MYINFO("querying the reg key %S\n" , lib_name);

	W::WCHAR new_name = (W::WCHAR)"c";

	file_name = &new_name;

}


VOID AnotherHook5 ( void * buffer) {
	
	int i;

	MYINFO("Destination buffer of the memcpy is %08x\n" , (unsigned int ) buffer);

	for(i=0;i<200;i++){
	
		MYINFO("memcpy called\n in buffer @ position (buffer+%d) we have: %c\n" , i, *(unsigned int *)buffer+i);
	}

	W::WCHAR new_name = (W::WCHAR)"c";

	file_name = &new_name;

	
}
*/

VOID VirtualQueryExHook (W::HANDLE hProcess, W::LPCVOID baseAddress, W::PMEMORY_BASIC_INFORMATION mbi, W::SIZE_T *numBytes) {
	if (hProcess == W::GetCurrentProcess())
		VirtualQueryHook(baseAddress, mbi, numBytes);
}

VOID UnhandledHook(){

	printf("#####CALLED THE SetUnhandledExceptionFilter\n");
	fflush(stdout);

}
//----------------------------- HOOKED DISPATCHER -----------------------------//

//scan the image and try to hook all the function specified above
void HookFunctions::hookDispatcher(IMG img){
	//for each function that we want to hook or replace
	for (std::map<string,int>::iterator item = this->functionsMap.begin(); item != this->functionsMap.end(); ++item){
		//get the pointer to the specified function
		const char * func_name = item->first.c_str();
		RTN rtn = RTN_FindByName(img, func_name);
		//if we found a valid routine
		if(rtn != RTN_Invalid()){
			
			ADDRINT va_address = RTN_Address(rtn);
			MYINFO("Inside %s Address of %s: %08x" ,IMG_Name(img).c_str(),func_name, va_address);

			RTN_Open(rtn); 	
			int index = item->second;

			//decide what to do based on the function hooked
			//Different arguments are passed to the hooking routine based on the function
			switch(index){
				case(VIRTUALALLOC_INDEX):
					RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)VirtualAllocHook , IARG_FUNCARG_ENTRYPOINT_VALUE,1 , IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
					break;
				case(RTLALLOCATEHEAP_INDEX):
					//need to be IPOINT_AFTER because the allocated address is returned as return value
					RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)RtlAllocateHeapHook , IARG_FUNCARG_ENTRYPOINT_VALUE,2, IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
					break;
				case(ISDEBUGGERPRESENT_INDEX):
					RTN_Replace(rtn, AFUNPTR(IsDebuggerPresentHook));
					break;
				case(RTLREALLOCATEHEAP_INDEX):
					//IPOINT_BEFORE because the address to be realloc is passed as an input paramenter
					RTN_InsertCall(rtn,IPOINT_BEFORE,(AFUNPTR)RtlReAllocateHeapHook, IARG_FUNCARG_ENTRYPOINT_VALUE,2 , IARG_FUNCARG_ENTRYPOINT_VALUE,3, IARG_END);
					break;
				case(MAPVIEWOFFILE_INDEX):
					//need to be IPOINT_AFTER because the allocated address is returned as return value
					RTN_InsertCall(rtn,IPOINT_AFTER,(AFUNPTR)MapViewOfFileHookAfter,IARG_FUNCARG_ENTRYPOINT_VALUE,1,IARG_FUNCARG_ENTRYPOINT_VALUE,2,IARG_FUNCARG_ENTRYPOINT_VALUE,3, IARG_FUNCARG_ENTRYPOINT_VALUE,4,IARG_FUNCRET_EXITPOINT_VALUE,  IARG_END);
					break;
				case(VIRTUALFREE_INDEX):
					RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)VirtualFreeHook , IARG_FUNCARG_ENTRYPOINT_VALUE,0, IARG_END);
					break;
				case(VIRTUALQUERY_INDEX):
					//IPOINT_AFTER because we have to check if the query is on a whitelisted address
					RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)VirtualQueryHook, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_FUNCRET_EXITPOINT_REFERENCE, IARG_END);
					break;
					/*	case(VIRTUALPROTECT_INDEX):
					//IPOINT_AFTER because we have to check if the query is on a whitelisted address
					RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)VirtualProtectHook, IARG_FUNCARG_ENTRYPOINT_VALUE, 0,  IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_FUNCARG_ENTRYPOINT_VALUE, 3, IARG_FUNCRET_EXITPOINT_REFERENCE, IARG_END);
				*/
				/*
				case (ANOTHERHOOK2):
					RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)AnotherHook2, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,  IARG_END);
					break;
				case (ANOTHERHOOK3):
					RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)AnotherHook3, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,  IARG_END);
					break;
				case (ANOTHERHOOK5):
					RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)AnotherHook5, IARG_FUNCARG_ENTRYPOINT_VALUE, 0,  IARG_END);
					break;
				*/
				case(VIRTUALQUERYEX_INDEX):
					//IPOINT_AFTER because we have to check if the query is on a whitelisted address
					RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)VirtualQueryExHook, IARG_FUNCARG_ENTRYPOINT_VALUE, 0,  IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCRET_EXITPOINT_REFERENCE, IARG_END);
					break;
				case(UNHANDLED_INDEX):
					RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)UnhandledHook,  IARG_END);	
				}			
			RTN_Close(rtn);
		}
	}
}


