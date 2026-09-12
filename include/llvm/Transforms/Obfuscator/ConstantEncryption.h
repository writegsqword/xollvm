#pragma once

#include "llvm/IR/PassManager.h"

namespace llvm {
	struct ConstEncPass : public RequiredPassInfoMixin<ConstEncPass> {
	public:
		PreservedAnalyses run(Function& F, FunctionAnalysisManager& AM);
		static bool isRequired() { return true; }
	};

} // namespace llvm
