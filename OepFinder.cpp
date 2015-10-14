#include "OepFinder.h"
#include "Debug.h"
namespace W {
    #include <windows.h>
}


OepFinder::OepFinder(void){
	
}


OepFinder::~OepFinder(void){
}


VOID handleWrite(ADDRINT ip, ADDRINT startAddr, UINT32 size)
{	
	//if(libHandler.checkWriteInExeSpace(ip)){
	//MYINFO("Examining Write instruction %x\n",ip);
		WxorXHandler wxorxHandler=WxorXHandler::getInstance();
		wxorxHandler.writeSetManager(ip,startAddr,size);
	//}
	

}

UINT32 OepFinder::IsCurrentInOEP(INS ins){
	WxorXHandler wxorxHandler=WxorXHandler::getInstance();

	//W::Sleep(1);
	UINT32 writeItemIndex=-1;
	ADDRINT curEip = INS_Address(ins);
	
	

	//check if current instruction is a write
	if(wxorxHandler.isWriteINS(ins)){	
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)handleWrite, IARG_INST_PTR, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
		
	}

	//check if current instruction is inside a library
	BOOL isLib = libHandler.isLibInstruction(curEip);
	if(isLib){
		return OEPFINDER_LIB_FILTERED; 
	}
	MYINFO("Examining if WxorX %x\n",curEip);



	//If the instruction violate WxorX return the index of the WriteItem in which the EIP is
	//If the instruction doesn't violate WxorX return -1
	writeItemIndex = wxorxHandler.getWxorXindex(ins);

	if(writeItemIndex != -1 ){
			
		BOOL isOEP = heuristics(ins,writeItemIndex);
		wxorxHandler.deleteWriteItem(writeItemIndex);
			
		if(isOEP){
			return OEPFINDER_FOUND_OEP;
		}
		return OEPFINDER_HEURISTIC_FAIL;
	}
	return OEPFINDER_NOT_WXORX_INST;

}






BOOL OepFinder::heuristics(INS ins,UINT32 WriteItemIndex){
	return FALSE;
}
