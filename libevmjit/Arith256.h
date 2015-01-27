#pragma once

#include "CompilerHelper.h"

namespace dev
{
namespace eth
{
namespace jit
{

class Arith256 : public CompilerHelper
{
public:
	Arith256(llvm::IRBuilder<>& _builder);

	llvm::Value* mul(llvm::Value* _arg1, llvm::Value* _arg2);
	llvm::Value* div(llvm::Value* _arg1, llvm::Value* _arg2);
	llvm::Value* mod(llvm::Value* _arg1, llvm::Value* _arg2);
	llvm::Value* sdiv(llvm::Value* _arg1, llvm::Value* _arg2);
	llvm::Value* smod(llvm::Value* _arg1, llvm::Value* _arg2);
	llvm::Value* exp(llvm::Value* _arg1, llvm::Value* _arg2);
	llvm::Value* mulmod(llvm::Value* _arg1, llvm::Value* _arg2, llvm::Value* _arg3);
	llvm::Value* addmod(llvm::Value* _arg1, llvm::Value* _arg2, llvm::Value* _arg3);

private:
	llvm::Function* getDivFunc();

	llvm::Value* binaryOp(llvm::Function* _op, llvm::Value* _arg1, llvm::Value* _arg2);
	llvm::Value* ternaryOp(llvm::Function* _op, llvm::Value* _arg1, llvm::Value* _arg2, llvm::Value* _arg3);

	llvm::Function* m_mul;
	llvm::Function* m_div;
	llvm::Function* m_mod;
	llvm::Function* m_sdiv;
	llvm::Function* m_smod;
	llvm::Function* m_exp;
	llvm::Function* m_mulmod;
	llvm::Function* m_addmod;

	llvm::Function* m_newDiv = nullptr;

	llvm::Value* m_arg1;
	llvm::Value* m_arg2;
	llvm::Value* m_arg3;
	llvm::Value* m_result;
};


}
}
}
