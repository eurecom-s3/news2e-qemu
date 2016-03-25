DEF_HELPER_4(s2e_ld, i64 /* val */, env, i64 /* addr */, i32 /* memop */, i32 /* idx */) 
DEF_HELPER_5(s2e_st, void, env, i64 /* addr */, i32 /* memop */, i32 /* idx */, i64 /* val */)
DEF_HELPER_2(s2e_base_instruction, void, env, i32 /* op_idx */)
DEF_HELPER_3(s2e_instrument_code, void, env, ptr /* signal */, i64 /* pc */)
