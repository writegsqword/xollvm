#include "llvm/Transforms/Obfuscator/ObfuscationAnnotationAnalysis.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Obfuscator/ObfuscationConfig.h"
#include "llvm/Transforms/Obfuscator/ObfuscationOptions.h"
#include "llvm/Transforms/Obfuscator/Rng.h"
#include "llvm/Support/ErrorHandling.h"
#include <algorithm>
#include <random>


using namespace llvm;
AnalysisKey ObfuscationAnnotationAnalysis::Key;
uint64_t ObfuscationAnnotationCache::getFunctionSeed(const Function& F) const {
	// Stable across runs if ModuleSeed is stable.
	return llvm::obf::mix64(ModuleSeed ^ llvm::obf::fnv1a64(F.getName()) ^ 0x9E3779B97F4A7C15ull);
}
static uint64_t computeModuleSeed(const Module& M) {
	if (ObfSeed.getNumOccurrences() > 0)
		return ObfSeed;
	if (ObfDeterministic) {
		return llvm::obf::mix64(llvm::obf::fnv1a64(M.getModuleIdentifier()) ^ 0xD1B54A32D192ED03ull);
	}
	// Non-deterministic: OS entropy
	std::random_device rd;
	uint64_t a = (uint64_t)rd();
	uint64_t b = (uint64_t)rd();
	uint64_t c = (uint64_t)rd();
	uint64_t d = (uint64_t)rd();
	return (a << 48) ^ (b << 32) ^ (c << 16) ^ d;
}

static void overlayConfig(ObfuscationConfig& Base,
	const ObfuscationConfig& Overlay) {
	for (const PassConfig& Incoming : Overlay.passes) {
		auto Existing = std::find_if(
			Base.passes.begin(), Base.passes.end(), [&](const PassConfig& PC) {
				return PC.passName == Incoming.passName;
			});
		if (Existing == Base.passes.end()) {
			Base.passes.push_back(Incoming);
			continue;
		}

		Existing->enabled = Incoming.enabled;
		if (!Incoming.rawInner.empty())
			Existing->rawInner = Incoming.rawInner;
		for (const auto& KV : Incoming.params)
			Existing->params[KV.first] = KV.second;
	}

	if (Overlay.budgetMultiplier)
		Base.budgetMultiplier = Overlay.budgetMultiplier;
	if (Overlay.budgetHardCap)
		Base.budgetHardCap = Overlay.budgetHardCap;
}



ObfuscationAnnotationAnalysis::Result
ObfuscationAnnotationAnalysis::run(Module& M, ModuleAnalysisManager& MAM) {
	ObfuscationAnnotationCache Out;

	Out.ModuleSeed = computeModuleSeed(M);

	// Snapshot llvm.global.annotations initializer for content-based invalidation
	if (auto* GA = M.getGlobalVariable("llvm.global.annotations")) {
		Out.AnnotationsInit = GA->hasInitializer() ? GA->getInitializer() : nullptr;
	}
	else {
		Out.AnnotationsInit = nullptr;
	}

	ObfuscationConfig DefaultConfig;
	if (!ObfDefaultConfig.empty()) {
		DefaultConfig =
			AnnotationParser::parseAnnotationString(ObfDefaultConfig);
		if (DefaultConfig.passes.empty())
			report_fatal_error(
				"-obf-default-config did not contain any valid passes");
	}

	for (Function& F : M) {
		if (F.isDeclaration())
			continue;

		ObfuscationConfig Cfg = DefaultConfig;
		overlayConfig(Cfg, AnnotationParser::parseAnnotations(&F));
		if (!Cfg.passes.empty())
			Out.PerFunction[&F] = std::move(Cfg);
	}

	return Out;
}
