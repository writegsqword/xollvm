#pragma once

#include "llvm/IR/PassManager.h"

namespace llvm {
	class SplitBasicBlockPass : public RequiredPassInfoMixin<SplitBasicBlockPass> {
	public:
		PreservedAnalyses run(Function& F, FunctionAnalysisManager& AM);
		static bool isRequired() { return true; }
	};
} // namespace llvm
