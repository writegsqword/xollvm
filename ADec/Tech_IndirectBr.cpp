#include "llvm/Transforms/Obfuscator/ADec/Technique.h"
#include "llvm/Transforms/Obfuscator/ADec/Types.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"

#define DEBUG_TYPE "adec"
STATISTIC(ADecIndirectBrs, "Branches converted to indirectbr trampolines");

using namespace llvm;
using namespace llvm::obf::adec;

namespace {

static bool canConvertBranch(llvm::UncondBrInst* BI) {
	if (!BI)
		return false;

	llvm::BasicBlock* BB = BI->getParent();
	if (BB == &BB->getParent()->getEntryBlock())
		return false;
	if (BB->isEHPad())
		return false;

	llvm::BasicBlock* Succ = BI->getSuccessor(0);
	if (Succ == BB)
		return false;

	return true;
}

class IndirectBrTechnique final : public ADecTechnique {
public:
	llvm::StringRef name() const override { return "indirectBr"; }

	bool supportsTarget(const llvm::Triple&) const override { return true; }

	bool isEnabled(const llvm::AntiDecompilerConfig& Cfg) const override {
		return Cfg.enableIndirectBr;
	}

	unsigned run(ADecCtx& Ctx, unsigned Budget) override {
		llvm::SmallVector<llvm::UncondBrInst*, 32> Cands;
		for (llvm::BasicBlock& BB : Ctx.F) {
			auto* BI = llvm::dyn_cast<llvm::UncondBrInst>(BB.getTerminator());
			if (canConvertBranch(BI))
				Cands.push_back(BI);
		}

		if (Cands.empty())
			return 0;

		Ctx.ShuffleRng.shuffle(llvm::MutableArrayRef<llvm::UncondBrInst*>(
		    Cands.data(), Cands.size()));

		// Decoy destinations cannot be picked from existing BBs: adding a
		// new edge SrcBB→ExistingBB can violate SSA dominance for any
		// value used downstream of ExistingBB whose definition is not on
		// every path through SrcBB. The runtime branch always lands on
		// the real Target (the slot is loaded with that BlockAddress), so
		// decoys exist only to keep `indirectbr` honest for the verifier
		// and to confuse static analysis. Use synthetic dead BBs
		// containing only `unreachable` — they have zero SSA operands
		// and zero successors, so they are always safe to list.

		llvm::LLVMContext& C = Ctx.F.getContext();
		llvm::BasicBlock& Entry = Ctx.F.getEntryBlock();
		llvm::Type* PtrTy = llvm::PointerType::getUnqual(C);

		int EffProb = Ctx.Cfg.effectiveProb(name());

		unsigned Converted = 0;
		for (llvm::UncondBrInst* BI : Cands) {
			if (Converted >= Budget)
				break;
			if (Ctx.SelectRng.range(100) >= (uint32_t)EffProb)
				continue;

			llvm::BasicBlock* Source = BI->getParent();
			llvm::BasicBlock* Target = BI->getSuccessor(0);

			llvm::IRBuilder<> EntryB(&*Entry.getFirstInsertionPt());
			llvm::AllocaInst* Slot =
			    EntryB.CreateAlloca(PtrTy, nullptr, Ctx.prefixed("ibr.slot"));
			llvm::AllocaInst* Trace = EntryB.CreateAlloca(
			    llvm::Type::getInt8Ty(C), nullptr, Ctx.prefixed("ibr.trace"));

			// A single known BlockAddress, even when routed through a volatile
			// stack slot, is folded back to a direct branch by the post-extension
			// optimization pipeline. Route through two semantically equivalent
			// destination blocks and choose between them using an address-derived
			// value that is unavailable at compile time. The distinct volatile
			// stores prevent the destinations from being merged. Both paths join
			// before entering Target, so the original edge semantics are retained.
			llvm::BasicBlock* Dest0 = llvm::BasicBlock::Create(
			    C, Ctx.prefixed("ibr.dest0"), &Ctx.F);
			llvm::BasicBlock* Dest1 = llvm::BasicBlock::Create(
			    C, Ctx.prefixed("ibr.dest1"), &Ctx.F);
			llvm::BasicBlock* Join = llvm::BasicBlock::Create(
			    C, Ctx.prefixed("ibr.join"), &Ctx.F);

			llvm::IRBuilder<> D0(Dest0);
			auto* D0Store = D0.CreateStore(
			    llvm::ConstantInt::get(llvm::Type::getInt8Ty(C), 0), Trace);
			D0Store->setVolatile(true);
			D0.CreateBr(Join);

			llvm::IRBuilder<> D1(Dest1);
			auto* D1Store = D1.CreateStore(
			    llvm::ConstantInt::get(llvm::Type::getInt8Ty(C), 1), Trace);
			D1Store->setVolatile(true);
			D1.CreateBr(Join);

			llvm::IRBuilder<> JoinB(Join);
			JoinB.CreateBr(Target);
			for (llvm::PHINode& Phi : Target->phis())
				Phi.replaceIncomingBlockWith(Source, Join);

			llvm::IRBuilder<> B(BI);
			llvm::Value* SlotInt = B.CreatePtrToInt(
			    Slot, llvm::Type::getInt64Ty(C), Ctx.prefixed("ibr.entropy"));
			llvm::Value* Shifted = B.CreateLShr(
			    SlotInt, llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), 4));
			llvm::Value* Pick = B.CreateTrunc(
			    Shifted, llvm::Type::getInt1Ty(C), Ctx.prefixed("ibr.pick"));
			llvm::Value* BA = B.CreateSelect(
			    Pick, llvm::BlockAddress::get(&Ctx.F, Dest1),
			    llvm::BlockAddress::get(&Ctx.F, Dest0), Ctx.prefixed("ibr.target"));
			auto* St = B.CreateStore(BA, Slot);
			St->setVolatile(true);

			auto* Ld = B.CreateLoad(PtrTy, Slot, Ctx.prefixed("ibr.addr"));
			Ld->setVolatile(true);

			auto* IBr = llvm::IndirectBrInst::Create(
			    Ld, 2, BI->getIterator());
			IBr->addDestination(Dest0);
			IBr->addDestination(Dest1);

			BI->eraseFromParent();

			++Converted;
			++ADecIndirectBrs;
		}

		return Converted;
	}
};

} // namespace

namespace llvm {
namespace obf {
namespace adec {

std::unique_ptr<ADecTechnique> makeIndirectBrTechnique() {
	return std::make_unique<IndirectBrTechnique>();
}

} // namespace adec
} // namespace obf
} // namespace llvm
