#include <pin.H>
#include <fstream>
#include <map>
#include <vector>
#include <algorithm>

using std::ofstream;
using std::hex;
using std::string;
using std::endl;
using std::map;
using std::vector;
using std::pair;

// struct to hold rutines information
struct RTNInfo {
    string imgName;
    ADDRINT imgAddr;
    string rtnName;
    ADDRINT rtnAddr;
    UINT64 instrCount = 0;
    UINT64 callCount = 0;
};

struct BBLInfo {
    ADDRINT bblAddr;
    UINT64 execCount = 0;
    UINT64 takenCount = 0;
    UINT64 fallthruCount = 0;
    UINT64 unCondCount = 0;
    map<ADDRINT, UINT64> addrCountMap;
};

ofstream outFile;


VOID unCondCount(BBLInfo* info) {
    info->fallthruCount++;
}


VOID docount_inc_call(INT32 taken) { CountSeen._call++;  if (taken) CountTaken._call++; }
VOID docount_inc_call_indirect(INT32 taken){ CountSeen._call_indirect++;  if (taken) CountTaken._call_indirect++; }
VOID docount_inc_branch(INT32 taken){ if (taken) info->takencCount++; else info->fallthruCount++; }
VOID docount_inc_branch_indirect(INT32 taken, BBL info){ if (taken) info->takencCount++; else info->fallthruCount++; }
VOID docount_inc_syscall(INT32 taken){ CountSeen._branch_indirect++; CountTaken._branch_indirect++; }
VOID docount_inc_return(INT32 taken){ CountSeen._return++;  if (taken) CountTaken._return++; }


// the main function collecting information for each rutine and sets the treatment rutines for each rutine and instruction
VOID BlockRoutine(BBL blk, VOID* v) {

    // check that the rutine is valid
    INS lastIns = BBL_InsTail(blk);
       
    if (INS_IsDirectControlFlow(lastIns))
    {
        if (INS_IsCall(lastIns))
            INS_InsertCall(lastIns, IPOINT_BEFORE, (AFUNPTR)docount_inc_call, IARG_BRANCH_TAKEN, IARG_END);
        else
            INS_InsertCall(lastIns, IPOINT_BEFORE, (AFUNPTR)docount_inc_branch, IARG_BRANCH_TAKEN, IARG_END);
    }
    else if (INS_IsIndirectControlFlow(lastIns))
    {
        //call = unconditional jump
        if (INS_IsCall(lastIns))
            INS_InsertCall(lastIns, IPOINT_BEFORE, (AFUNPTR)docount_inc_call_indirect, IARG_BRANCH_TAKEN, IARG_END);
        //return = unconditional jump
        else if (INS_IsRet(lastIns))
        {
            INS_InsertCall(lastIns, IPOINT_BEFORE, (AFUNPTR)docount_inc_return, IARG_BRANCH_TAKEN, IARG_END);
        }
        //syscall = unconditional jump
        else if (INS_IsSyscall(lastIns))
        {
            INS_InsertCall(lastIns, IPOINT_BEFORE, (AFUNPTR)docount_inc_syscall, IARG_BRANCH_TAKEN, IARG_END);
        }
        else
            INS_InsertCall(lastIns, IPOINT_BEFORE, (AFUNPTR)docount_inc_branch_indirect, IARG_BRANCH_TAKEN, blk, IARG_END);
    }

    //this instruction is not a jump instruction
    else 
        return;





    // open the rutine so we can change it
    RTN_Open(rtn);

    //hold the rutine's information
    RTNInfo* info = new RTNInfo;
    info->rtnName = RTN_Name(rtn);
    info->rtnAddr = RTN_Address(rtn);
    IMG img = IMG_FindByAddress(info->rtnAddr);

    // check that the image is valid
    if (!IMG_Valid(img)) {
        RTN_Close(rtn);
        delete info;
        return;
    }

    //hold the image's information
    info->imgName = IMG_Name(img);
    info->imgAddr = IMG_LowAddress(img);

    rtnMap[rtn] = info;
    rtnStats.push_back(info);

    // add a function of increasing the function call count before the rutine runs 
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)OnRoutineCall, IARG_PTR, info, IARG_END);

    // for each instruction in the rutine add a function of increasing the instruction count before every instruction is executed
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountInstructions, IARG_PTR, info, IARG_END);
    }

    // closing the rutine
    RTN_Close(rtn);
}

// a function called in the end of the program
VOID Fini(INT32 code, VOID* v) {
    // sort the the rutine vectors by the instruction count for highest to lowest
    std::sort(rtnStats.begin(), rtnStats.end(), [](RTNInfo* a, RTNInfo* b) {
        return a->instrCount > b->instrCount;
    });

    // open output file 
    outFile.open("rtn-output.csv");

    // add instruction information to the file by the specific formating
    for (auto info : rtnStats) {
        if (info->instrCount > 0 || info->callCount > 0) {
            outFile << info->imgName << ", 0x" << hex << info->imgAddr << ", "
                    << info->rtnName << ", 0x" << hex << info->rtnAddr << ", "
                    << info->instrCount << ", " << info->callCount << endl;
        }
    }
    //close the output file 
    outFile.close();
}

int main(int argc, char* argv[]) {
    if (PIN_Init(argc, argv)) {
        return 1;
    }
    
    //initialize the instrumantation and fini function for each rutine
    RTN_AddInstrumentFunction(InstrumentRoutine, 0);
    PIN_AddFiniFunction(Fini, 0);

    // start the program
    PIN_StartProgram();
    return 0;
}
