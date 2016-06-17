#ifndef _S2E_CXX_ARMENVWRAPPER_H
#define _S2E_CXX_ARMENVWRAPPER_H

#if !defined(__cplusplus)
#error This file is not supposed to be included from C!
#endif /* !defined(__cplusplus) */

#define ENV_FIELDSIZE(field) sizeof(((CPUARMState *) 0)->field)
#define ENV_FIELDOFFSET(field) offsetof(CPUARMState, field)

class CPUArchStateInfo
{
public:
	enum FieldIndex {
		//Registers R0 - PC _MUST_ have the same number as in tcg/arm/tcg-target.h!!!
		//Otherwise the bitmask set by tcg_calc_regmask will be wrong.
		R0 = 0,
		R1 = 1,
		R2 = 2,
		R3 = 3,
		R4 = 4,
		R5 = 5,
		R6 = 6,
		R7 = 7,
		R8 = 8,
		R9 = 9,
		R10 = 10,
		R11 = 11,
		R12 = 12,
		R13 = 13,
		SP = 13,
		R14 = 14,
		LR = 14,
		R15 = 15,
		PC = 15,
		CF = 16,
		VF = 17,
		NF = 18,
		ZF = 19,
		SPSR = 21,
		BANKED_SP_USR = 22,
		BANKED_SP_FIQ = 23,
		BANKED_SP_IRQ = 24,
		BANKED_SP_SVC = 25,
		BANKED_SP_ABT = 26,
		BANKED_SP_UND = 27,
		BANKED_SP_SYS = 28,
		BANKED_SP_MON = 29,
		BANKED_LR_USR = 30,
		BANKED_LR_FIQ = 31,
		BANKED_LR_IRQ = 32,
		BANKED_LR_SVC = 33,
		BANKED_LR_ABT = 34,
		BANKED_LR_UND = 35,
		BANKED_LR_SYS = 36,
		BANKED_LR_MON = 37,
		BANKED_R8_USR = 38,
		BANKED_R9_USR = 39,
		BANKED_R10_USR = 40,
		BANKED_R11_USR = 41,
		BANKED_R12_USR = 42,
		BANKED_R8_FIQ = 43,
		BANKED_R9_FIQ = 44,
		BANKED_R10_FIQ = 45,
		BANKED_R11_FIQ = 46,
		BANKED_R12_FIQ = 47,
	};

	/**
	 * List of fields which are allowed to be symbolic.
	 */
	static const FieldIndex SYMBOLIC_FIELDS[];

	static const size_t SYMBOLIC_BOUNDARY;

		
	/**
	 * Get the offset of a CPUARMState field from its index.
	 * @param idx Field index
	 * @return Field offset
	 */
	static inline size_t getOffset(FieldIndex idx) {
		switch(idx) {
			case CF: return ENV_FIELDOFFSET(CF);
			case VF: return ENV_FIELDOFFSET(VF);
			case NF: return ENV_FIELDOFFSET(NF);
			case ZF: return ENV_FIELDOFFSET(ZF);
			case R0 ... R15: return ENV_FIELDOFFSET(regs[0]) + (idx - R0) * ENV_FIELDSIZE(regs[0]);
			case SPSR: return ENV_FIELDOFFSET(spsr);
			case BANKED_SP_USR ... BANKED_SP_MON: return ENV_FIELDOFFSET(banked_r13[0]) + (idx - BANKED_SP_USR) * ENV_FIELDSIZE(banked_r13[0]);
			case BANKED_LR_USR ... BANKED_LR_MON: return ENV_FIELDOFFSET(banked_r14[0]) + (idx - BANKED_LR_USR) * ENV_FIELDSIZE(banked_r14[0]);
			case BANKED_R8_USR ... BANKED_R12_USR: return ENV_FIELDOFFSET(usr_regs[0]) + (idx - BANKED_R8_USR) * ENV_FIELDSIZE(usr_regs[0]);
			case BANKED_R8_FIQ ... BANKED_R12_FIQ: return ENV_FIELDOFFSET(fiq_regs[0]) + (idx - BANKED_R8_FIQ) * ENV_FIELDSIZE(fiq_regs[0]);	
		}
		abort(); //Should never be reached
	}	

	/**
	 * Get the index of a CPUARMState field from its offset.
	 * @param offset Field offset
	 * @return Field index
	 */
	static inline FieldIndex getIndex(size_t offset) {
		switch(offset) {
			case ENV_FIELDOFFSET(CF): return CF;
			case ENV_FIELDOFFSET(VF): return VF;
			case ENV_FIELDOFFSET(NF): return NF;
			case ENV_FIELDOFFSET(ZF): return ZF;
			case ENV_FIELDOFFSET(regs[0]): return R0;
			case ENV_FIELDOFFSET(regs[1]): return R1;
			case ENV_FIELDOFFSET(regs[2]): return R2;
			case ENV_FIELDOFFSET(regs[3]): return R3;
			case ENV_FIELDOFFSET(regs[4]): return R4;
			case ENV_FIELDOFFSET(regs[5]): return R5;
			case ENV_FIELDOFFSET(regs[6]): return R6;
			case ENV_FIELDOFFSET(regs[7]): return R7;
			case ENV_FIELDOFFSET(regs[8]): return R8;
			case ENV_FIELDOFFSET(regs[9]): return R9;
			case ENV_FIELDOFFSET(regs[10]): return R10;
			case ENV_FIELDOFFSET(regs[11]): return R11;
			case ENV_FIELDOFFSET(regs[12]): return R12;
			case ENV_FIELDOFFSET(regs[13]): return R13;
			case ENV_FIELDOFFSET(regs[14]): return R14;
			case ENV_FIELDOFFSET(regs[15]): return R15;
			case ENV_FIELDOFFSET(spsr): return SPSR;
			case ENV_FIELDOFFSET(banked_r13[0]): return BANKED_SP_USR;
			case ENV_FIELDOFFSET(banked_r13[1]): return BANKED_SP_FIQ;
			case ENV_FIELDOFFSET(banked_r13[2]): return BANKED_SP_IRQ;
			case ENV_FIELDOFFSET(banked_r13[3]): return BANKED_SP_SVC;
			case ENV_FIELDOFFSET(banked_r13[4]): return BANKED_SP_ABT;
			case ENV_FIELDOFFSET(banked_r13[5]): return BANKED_SP_UND;
			case ENV_FIELDOFFSET(banked_r13[6]): return BANKED_SP_SYS;
			case ENV_FIELDOFFSET(banked_r13[7]): return BANKED_SP_MON;
			case ENV_FIELDOFFSET(banked_r14[0]): return BANKED_LR_USR;
			case ENV_FIELDOFFSET(banked_r14[1]): return BANKED_LR_FIQ;
			case ENV_FIELDOFFSET(banked_r14[2]): return BANKED_LR_IRQ;
			case ENV_FIELDOFFSET(banked_r14[3]): return BANKED_LR_SVC;
			case ENV_FIELDOFFSET(banked_r14[4]): return BANKED_LR_ABT;
			case ENV_FIELDOFFSET(banked_r14[5]): return BANKED_LR_UND;
			case ENV_FIELDOFFSET(banked_r14[6]): return BANKED_LR_SYS;
			case ENV_FIELDOFFSET(banked_r14[7]): return BANKED_LR_MON;
			case ENV_FIELDOFFSET(usr_regs[0]): return BANKED_R8_USR;
			case ENV_FIELDOFFSET(usr_regs[1]): return BANKED_R9_USR;
			case ENV_FIELDOFFSET(usr_regs[2]): return BANKED_R10_USR;
			case ENV_FIELDOFFSET(usr_regs[3]): return BANKED_R11_USR;
			case ENV_FIELDOFFSET(usr_regs[4]): return BANKED_R12_USR;
			case ENV_FIELDOFFSET(fiq_regs[0]): return BANKED_R8_FIQ;
			case ENV_FIELDOFFSET(fiq_regs[1]): return BANKED_R9_FIQ;
			case ENV_FIELDOFFSET(fiq_regs[2]): return BANKED_R10_FIQ;
			case ENV_FIELDOFFSET(fiq_regs[3]): return BANKED_R11_FIQ;
			case ENV_FIELDOFFSET(fiq_regs[4]): return BANKED_R12_FIQ;
		}
		abort(); //Should never be reached
	}

	/**
	 * Get the human-readable name of a field.
	 * @param idx Field index
	 * @return Field name
	 */
	static const char* getName(FieldIndex idx) {
		switch(idx) {
			case CF: return "CF";
			case VF: return "VF";
			case NF: return "NF";
			case ZF: return "ZF";
			case R0: return "r0";
			case R1: return "r1";
			case R2: return "r2";
			case R3: return "r3";
			case R4: return "r4";
			case R5: return "r5";
			case R6: return "r6";
			case R7: return "r7";
			case R8: return "r8";
			case R9: return "r9";
			case R10: return "r10";
			case R11: return "r11";
			case R12: return "r12";
			case R13: return "sp";
			case R14: return "lr";
			case R15: return "pc";
			case SPSR: return "spsr";
			case BANKED_SP_USR: return "sp_usr";
			case BANKED_SP_FIQ: return "sp_fiq";
			case BANKED_SP_IRQ: return "sp_irq";
			case BANKED_SP_SVC: return "sp_svc";
			case BANKED_SP_ABT: return "sp_abt";
			case BANKED_SP_UND: return "sp_und";
			case BANKED_SP_SYS: return "sp_sys";
			case BANKED_SP_MON: return "sp_mon";
			case BANKED_LR_USR: return "lr_usr";
			case BANKED_LR_FIQ: return "lr_fiq";
			case BANKED_LR_IRQ: return "lr_irq";
			case BANKED_LR_SVC: return "lr_svc";
			case BANKED_LR_ABT: return "lr_abt";
			case BANKED_LR_UND: return "lr_und";
			case BANKED_LR_SYS: return "lr_sys";
			case BANKED_LR_MON: return "lr_mon";
			case BANKED_R8_USR: return "r8_usr";
			case BANKED_R9_USR: return "r9_usr";
			case BANKED_R10_USR: return "r10_usr";
			case BANKED_R11_USR: return "r11_usr";
			case BANKED_R12_USR: return "r12_usr";
			case BANKED_R8_FIQ: return "r8_fiq";
			case BANKED_R9_FIQ: return "r9_fiq";
			case BANKED_R10_FIQ: return "r10_fiq";
			case BANKED_R11_FIQ: return "r11_fiq";
			case BANKED_R12_FIQ: return "r12_fiq";
		}
		abort(); //Should never be reached
	}

	/**
	 * Get a field's size.
	 * @param idx Field index
	 * @return Field size in bytes
	 */
	static size_t getSize(FieldIndex idx) {
		switch(idx) {
			case CF ... ZF: return ENV_FIELDSIZE(CF);
			case R0 ... R15: return ENV_FIELDSIZE(regs[0]);
			case SPSR: return ENV_FIELDSIZE(spsr);
			case BANKED_SP_USR ... BANKED_SP_MON: return ENV_FIELDSIZE(banked_r13[0]);
			case BANKED_LR_USR ... BANKED_LR_MON: return ENV_FIELDSIZE(banked_r14[0]);
			case BANKED_R8_USR ... BANKED_R12_USR: return ENV_FIELDSIZE(usr_regs[0]);
			case BANKED_R8_FIQ ... BANKED_R12_FIQ: return ENV_FIELDSIZE(fiq_regs[0]);
		}
		abort(); //Should never be reached
	}
};


const CPUArchStateInfo::FieldIndex CPUArchStateInfo::SYMBOLIC_FIELDS[] = {
		CF, VF, NF, ZF,
		R0, R1, R2, R3, R4, R5, R6, R7, R8, R9,
		R10, R11, R12, SP, LR,
		SPSR,
		BANKED_SP_USR, BANKED_SP_FIQ,
		BANKED_SP_IRQ, BANKED_SP_SVC,
		BANKED_SP_ABT, BANKED_SP_UND,
		BANKED_SP_SYS, BANKED_SP_MON,
		BANKED_LR_USR, BANKED_LR_FIQ,
		BANKED_LR_IRQ, BANKED_LR_SVC,
		BANKED_LR_ABT, BANKED_LR_UND,
		BANKED_LR_SYS, BANKED_LR_MON,
		BANKED_R8_USR, BANKED_R9_USR,
		BANKED_R10_USR, BANKED_R11_USR,
		BANKED_R12_USR,
		BANKED_R8_FIQ, BANKED_R9_FIQ,
		BANKED_R10_FIQ, BANKED_R11_FIQ,
		BANKED_R12_FIQ};

const size_t CPUArchStateInfo::SYMBOLIC_BOUNDARY = CPUArchStateInfo::getOffset(CPUArchStateInfo::PC);

#undef ENV_FIELDSIZE
#undef ENV_FIELDOFFSET

#endif /* _S2E_CXX_ARMENVWRAPPER_H */

