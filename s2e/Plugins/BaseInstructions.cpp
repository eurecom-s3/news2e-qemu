extern "C" {
#include "config.h"
#include "qemu-common.h"
}


#include "BaseInstructions.h"
#include <s2e/S2E.h>
#include <s2e/Database.h>
#include <s2e/S2EExecutor.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>

#include <llvm/System/TimeValue.h>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(BaseInstructions, "Default set of custom instructions plugin", "",);

void BaseInstructions::initialize()
{
    s2e()->getCorePlugin()->onCustomInstruction.connect(
            sigc::mem_fun(*this, &BaseInstructions::onCustomInstruction));
}

bool BaseInstructions::createTables()
{
    const char *query = "create table TimingInstructions(" 
          "'timestamp' unsigned big int,"  
          "'data' unsigned big int,"
          "'pc' unsigned big int,"
          "'pid' unsigned big int"
          "); create index if not exists TimingInstructionsIdx on TimingInstructions(timestamp);";
    
    Database *db = s2e()->getDb();
    return db->executeQuery(query);
}

bool BaseInstructions::insertTiming(S2EExecutionState *state, uint64_t id)
{
    char buf[512];
    uint64_t timestamp = llvm::sys::TimeValue::now().usec();
    snprintf(buf, sizeof(buf), "insert into TimingInstructions('%"PRIu64"', '%"PRIu64"', '%"PRIu64"', '%"PRIu64"');", 
        timestamp, id, state->getPc(), state->getPid());
    
    Database *db = s2e()->getDb();
    return db->executeQuery(buf);
}

void BaseInstructions::handleBuiltInOps(S2EExecutionState* state, 
        uint64_t opcode, uint64_t value1)
{
    switch(opcode & 0xFF) {
        case 1: s2e()->getExecutor()->enableSymbolicExecution(state); break;
        case 2: s2e()->getExecutor()->disableSymbolicExecution(state); break;
        case 3: {
            int width = (opcode >> 8) & 0xff;
            if(width != 1 && width != 8 && width != 16
                    && width != 32 && width != 64) {
                s2e()->getWarningsStream()
                        << "Guest requested insertion of symbolic value"
                        << " of incorrect width " << width << std::endl;
            } else {
                s2e()->getMessagesStream()
                        << "Inserting symbolic value of width " << width
                        << " at " << hexval(value1) << std::endl;
                if(!state->writeMemory(value1,
                                       state->createSymbolicValue(width))) {
                    s2e()->getWarningsStream()
                        << "Can not insert symbolic value of width " << width
                        << " at " << hexval(value1)
                        << ": can not write to memory" << std::endl;
                }
            }
            break;
        }
        case 4: 
            {
                //Timing opcode
                uint64_t low = state->readCpuState(offsetof(CPUX86State, regs[R_EAX]), sizeof(uint32_t));
                uint64_t high = state->readCpuState(offsetof(CPUX86State, regs[R_EDX]), sizeof(uint32_t));
                insertTiming(state, (high << 32LL) | low);
            }

        case 5:
            {
                //Get current path
                state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]),
                    klee::ConstantExpr::create(123, klee::Expr::Int32));
            }
        default:
            s2e()->getWarningsStream()
                << "Invalid built-in opcode " << hexval(opcode) << std::endl;
            break;
    }
}

void BaseInstructions::onCustomInstruction(S2EExecutionState* state, 
        uint64_t opcode, uint64_t value1)
{
    TRACE("Custom instructions %#"PRIx64" %#"PRIx64"\n", opcode, value1);

    switch(opcode & 0xFF) {
        case 0x00:
            handleBuiltInOps(state, opcode>>8, value1);
            break;
        default:
            std::cout << "Invalid custom operation 0x"<< std::hex << opcode<< " at 0x" << 
                state->getPc() << std::endl;
    }
}

}
}
