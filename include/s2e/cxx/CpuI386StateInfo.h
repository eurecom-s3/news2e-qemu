
#ifndef _S2E_CXX_CPUI386STATEINFO_H
#define _S2E_CXX_CPUI386STATEINFO_H

#if !defined(__cplusplus)
#error This file is not supposed to be included from C!
#endif /* !defined(__cplusplus) */

#define ENV_FIELDSIZE(field) sizeof(((CPUX86State *) 0)->field)
#define ENV_FIELDOFFSET(field) offsetof(CPUX86State, field)

class CPUArchStateInfo
{
public:
	enum FieldIndex {
		//Registers XXX _MUST_ have the same number as in tcg/i386/tcg-target.h!!!
		//Otherwise the bitmask set by tcg_calc_regmask will be wrong.
	};

	/**
	 * List of fields which are allowed to be symbolic.
	 */
	static const FieldIndex SYMBOLIC_FIELDS[];
		
	/**
	 * Get the offset of a CPUARMState field from its index.
	 * @param idx Field index
	 * @return Field offset
	 */
	static inline size_t getOffset(FieldIndex idx) {
		switch(idx) {
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
		}
		abort(); //Should never be reached
	}

	static size_t getSize(FieldIndex idx) {
		switch(idx) {
		}
		abort(); //Should never be reached
	}



};

const CPUArchStateInfo::FieldIndex CPUArchStateInfo::SYMBOLIC_FIELDS[] = {};

#undef ENV_FIELDSIZE
#undef ENV_FIELDOFFSET

#endif /* _S2E_CXX_CPUI386STATEINFO_H */

