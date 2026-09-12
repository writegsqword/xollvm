#pragma once

#include "llvm/IR/PassManager.h"

namespace llvm {
	class FlatteningPass : public RequiredPassInfoMixin<FlatteningPass> {
	public:
		PreservedAnalyses run(Function& F, FunctionAnalysisManager& AM);
		static bool isRequired() { return true; }

	};
} // namespace llvm

