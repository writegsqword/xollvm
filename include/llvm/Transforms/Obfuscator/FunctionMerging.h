#pragma once

#include "llvm/IR/PassManager.h"

namespace llvm {

	class FunctionMergingPass : public RequiredPassInfoMixin<FunctionMergingPass> {
	public:
		PreservedAnalyses run(Module& M, ModuleAnalysisManager& AM);
		static bool isRequired() { return true; }
	};

} // namespace llvm
