#include <fstream>
#include "pin.H"
#include "doit-ins.hpp"
using std::ofstream;
using std::endl;

static ofstream fout ("trace.txt");
static int doit_ins_table[XED_ICLASS_LAST] = {0};

VOID PrintAddr (ADDRINT ptr) {
  fout << "0x" << std::hex << ptr << ' ';
}

VOID PrintEndl () {
  fout << endl;
}

VOID PrintMemVal (UINT32 size, ADDRINT ptr) {
  if (size == 2) {
    fout << "0x" << std::hex << *((UINT16*) ptr) << ' ';
  } else if (size == 4) {
    fout << "0x" << std::hex << *((UINT32*) ptr) << ' ';
  } else {
    fout << "0x" << std::hex << *((UINT64*) ptr) << ' ';
  }
}

VOID Instruction (INS ins, VOID* v) {
  INT32 cat = INS_Category (ins);
  if (!doit_ins_table [INS_Opcode (ins)]
      && !(cat == XED_CATEGORY_CALL ||
	   cat == XED_CATEGORY_RET ||
	   cat == XED_CATEGORY_UNCOND_BR ||
	   cat == XED_CATEGORY_COND_BR ||
	   cat == XED_CATEGORY_NOP ||
	   cat == XED_CATEGORY_WIDENOP
      )
  ) {
    fout << "Warning: Unallowed opcode " << INS_Mnemonic (ins) << endl;
  }
  INS_InsertCall (ins, IPOINT_BEFORE, (AFUNPTR) PrintAddr, IARG_INST_PTR, IARG_END);
  UINT32 memOperands = INS_MemoryOperandCount (ins);
  for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
    INS_InsertCall (ins, IPOINT_BEFORE, (AFUNPTR) PrintAddr, IARG_MEMORYOP_EA, memOp, IARG_END);
  }

  if (INS_Opcode (ins) == XED_ICLASS_POPCNT) {
    if (INS_OperandIsReg (ins, 1)) {
      INS_InsertCall (ins, IPOINT_BEFORE, (AFUNPTR) PrintAddr, IARG_REG_VALUE, INS_OperandReg (ins, 1), IARG_END);
    } else {
      INS_InsertCall (ins, IPOINT_BEFORE, (AFUNPTR) PrintMemVal, IARG_MEMORYREAD_SIZE, IARG_MEMORYREAD_EA, IARG_END);
    }
  }

  INS_InsertCall (ins, IPOINT_BEFORE, (AFUNPTR) PrintEndl, IARG_END);
}

VOID Fini (INT32 code, VOID* v) {
  fout << "Exit Called" << endl;
}

int main (int argc, char* argv[]) {
  if (PIN_Init (argc, argv)) {
    fout << "PIN_Init failed" << endl;
    return 1;
  }

  for (uint32_t i = 0; i < sizeof (doit_ins) / sizeof (int); i++) {
    doit_ins_table[doit_ins[i]] = 1;
  }

  INS_AddInstrumentFunction (Instruction, 0);

  PIN_AddFiniFunction (Fini, 0);

  PIN_StartProgram ();

  return 0;
}
