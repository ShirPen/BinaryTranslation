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

// a map for easy access for rutines information
map<RTN, RTNInfo*> rtnMap;
// use vector so we can sort the statistics 
vector<RTNInfo*> rtnStats;
ofstream outFile;

// count the number of instructions for each rutine
VOID CountInstructions(RTNInfo* info) {
    info->instrCount++;
}

// count the number  of calls for each rutine
VOID OnRoutineCall(RTNInfo* info) {
    info->callCount++;
}

// the main function collecting information for each rutine and sets the treatment rutines for each rutine and instruction
VOID InstrumentRoutine(RTN rtn, VOID* v) {

    // check that the rutine is valid
    if (!RTN_Valid(rtn)) return;

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
