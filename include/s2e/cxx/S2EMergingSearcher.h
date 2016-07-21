#ifndef _S2E_CXX_S2EMERGINGSEARCHER_H
#define _S2E_CXX_S2EMERGINGSEARCHER_H

#ifndef __cplusplus
#error This file should only be included from C++ code
#endif /* !defined(__cplusplus) */

//KLEE includes
#include "lib/Core/Searcher.h"

#include <map>
#include <set>

namespace s2e {

class S2EMergingSearcher : public klee::Searcher {
    klee::Executor &executor;
    std::map<klee::ExecutionState*, uint64_t> statesAtMerge;
    klee::Searcher *baseSearcher;
    llvm::Function *mergeFunction;

  private:
    uint64_t getMergePoint(klee::ExecutionState &es);
    static const std::string NAME;

  public:
    S2EMergingSearcher(klee::Executor &executor, klee::Searcher *baseSearcher);
    ~S2EMergingSearcher();

    void queueStateForMerge(klee::ExecutionState &es, uint64_t mergePoint);

    klee::ExecutionState &selectState();
    void update(klee::ExecutionState *current,
                const std::set<klee::ExecutionState*> &addedStates,
                const std::set<klee::ExecutionState*> &removedStates);
    bool empty() { return baseSearcher->empty() && statesAtMerge.empty(); }

    virtual const std::string& getName() const override {return NAME;}
    //LLVM-style RTTI
    static bool classof(klee::Searcher* s) {return s->getName() == NAME;}
  };


} //namespace s2e



#endif /* !defined(_S2E_CXX_S2EMERGINGSEARCHER_H) */
