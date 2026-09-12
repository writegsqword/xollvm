#pragma once

#include "llvm/IR/PassManager.h"

namespace llvm
{
	class SubstitutionPass : public RequiredPassInfoMixin<SubstitutionPass> {
	public:
		PreservedAnalyses run(Function& F, FunctionAnalysisManager& AM);
		static bool isRequired() { return true; }
	};
} // namespace llvm

