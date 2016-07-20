#include "s2e/cxx/S2EMergingSearcher.h"

//KLEE includes
#include "lib/Core/Searcher.h"
#include "lib/Core/Executor.h"

using klee::Executor;
using klee::Searcher;
using klee::ExecutionState;
using klee::DebugLogMerge;

using llvm::Instruction;
using llvm::CallInst;
using llvm::CallSite;

using std::set;
using std::map;

namespace s2e {

S2EMergingSearcher::S2EMergingSearcher(Executor &_executor, Searcher *_baseSearcher)
  : executor(_executor),
    baseSearcher(_baseSearcher),
    mergeFunction(executor.getKModule()->kleeMergeFn) {
}

S2EMergingSearcher::~S2EMergingSearcher() {
  delete baseSearcher;
}

///

uint64_t S2EMergingSearcher::getMergePoint(ExecutionState &es) {
  if (mergeFunction) {
    Instruction *i = es.pc->inst;

    if (i->getOpcode()==Instruction::Call) {
      CallSite cs(cast<CallInst>(i));
      if (mergeFunction==cs.getCalledFunction())
        return (uint64_t) i;
    }
  }

  return 0;
}

void S2EMergingSearcher::queueStateForMerge(ExecutionState &es, uint64_t mergePoint) {
  baseSearcher->removeState(&es, &es);
  statesAtMerge.insert(std::make_pair(&es, mergePoint));
}

ExecutionState &S2EMergingSearcher::selectState() {
  while (!baseSearcher->empty()) {
    ExecutionState &es = baseSearcher->selectState();
    uint64_t mp = getMergePoint(es);
    if (mp) {
      baseSearcher->removeState(&es, &es);
      statesAtMerge.insert(std::make_pair(&es, mp));
    } else {
      return es;
    }
  }

  // build map of merge point -> state list
  std::map<uint64_t, std::vector<ExecutionState*> > merges;
  for (const auto stateAtMerge : statesAtMerge) {
    merges[stateAtMerge.second].push_back(stateAtMerge.first);
  }

  if (DebugLogMerge)
    llvm::errs() << "-- all at merge --\n";
  for (auto merge : merges) {
    int mergeCount = 0;
    if (DebugLogMerge) {
      llvm::errs() << "\tmerge: " << merge.first << " [";
      for (auto state : merge.second) {
        llvm::errs() << state << ", ";
      }
      llvm::errs() << "]\n";
    }

    // merge states
    set<ExecutionState*> toMerge(merge.second.begin(), merge.second.end());
    while (!toMerge.empty()) {
      ExecutionState *base = *toMerge.begin();
      toMerge.erase(toMerge.begin());

      set<ExecutionState*> toErase;
      for (auto mergeWith : toMerge) {
        if (executor.merge(*base, *mergeWith)) {
          mergeCount += 1;
          toErase.insert(mergeWith);
        }
      }
      if (DebugLogMerge && !toErase.empty()) {
        llvm::errs() << "\t\tmerged: " << base << " with [";
        bool first = true;
        for (auto state : toErase) {
          if (!first) llvm::errs() << ", ";
          llvm::errs() << state;
          first = false;
        }
        llvm::errs() << "]\n";
      }
      for (auto state : toErase) {
        auto it2 = toMerge.find(state);
        assert(it2!=toMerge.end());
        executor.terminateState(*state);
        toMerge.erase(it2);
      }

      // step past merge and toss base back in pool
      statesAtMerge.erase(statesAtMerge.find(base));
      ++base->pc;
      baseSearcher->addState(base);
    }
    if (DebugLogMerge)
      llvm::errs() << "\t\t" << mergeCount << " states merged\n";
  }

  if (DebugLogMerge) {
    llvm::errs() << "-- merge complete, continuing --\n";
  }

  return selectState();
}

void S2EMergingSearcher::update(ExecutionState *current,
                             const set<ExecutionState*> &addedStates,
                             const set<ExecutionState*> &removedStates) {
  if (!removedStates.empty()) {
    set<ExecutionState *> alt = removedStates;
    for (auto es : removedStates) {
      auto itm = statesAtMerge.find(es);
      if (itm!=statesAtMerge.end()) {
        statesAtMerge.erase(itm);
        alt.erase(alt.find(es));
      }
    }
    baseSearcher->update(current, addedStates, alt);
  } else {
    baseSearcher->update(current, addedStates, removedStates);
  }
}


} //namespace s2e
