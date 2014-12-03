
#include "GasMeter.h"

#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IntrinsicInst.h>

#include "Type.h"
#include "Ext.h"
#include "RuntimeManager.h"

namespace dev
{
namespace eth
{
namespace jit
{

namespace // Helper functions
{

uint64_t const c_stepGas = 1;
uint64_t const c_balanceGas = 20;
uint64_t const c_sha3Gas = 20;
uint64_t const c_sloadGas = 20;
uint64_t const c_sstoreSetGas = 300;
uint64_t const c_sstoreResetGas = 100;
uint64_t const c_sstoreRefundGas = 100;
uint64_t const c_createGas = 100;
uint64_t const c_callGas = 20;
uint64_t const c_expGas = 1;
uint64_t const c_expByteGas = 1;
uint64_t const c_memoryGas = 1;
uint64_t const c_txDataZeroGas = 1;
uint64_t const c_txDataNonZeroGas = 5;
uint64_t const c_txGas = 500;
uint64_t const c_logGas = 32;
uint64_t const c_logDataGas = 1;
uint64_t const c_logTopicGas = 32;
uint64_t const c_copyGas = 1;

uint64_t getStepCost(Instruction inst) // TODO: Add this function to FeeSructure (pull request submitted)
{
	switch (inst)
	{
	case Instruction::STOP:
	case Instruction::SUICIDE:
	case Instruction::SSTORE: // Handle cost of SSTORE separately in GasMeter::countSStore()
		return 0;

	case Instruction::SLOAD:	return c_sloadGas;

	case Instruction::SHA3:		return c_sha3Gas;

	case Instruction::BALANCE:	return c_balanceGas;

	case Instruction::CALL:
	case Instruction::CALLCODE:	return c_callGas;

	case Instruction::CREATE:	return c_createGas;

	case Instruction::LOG0:
	case Instruction::LOG1:
	case Instruction::LOG2:
	case Instruction::LOG3:
	case Instruction::LOG4:
	{
		auto numTopics = static_cast<uint64_t>(inst) - static_cast<uint64_t>(Instruction::LOG0);
		return c_logGas + numTopics * c_logTopicGas;
	}

	default: // Assumes instruction code is valid
		return 1;
	}
}

bool isCostBlockEnd(Instruction _inst)
{
	// Basic block terminators like STOP are not needed on the list
	// as cost will be commited at the end of basic block

	// CALL & CALLCODE are commited manually

	switch (_inst)
	{
	case Instruction::CALLDATACOPY:
	case Instruction::CODECOPY:
	case Instruction::MLOAD:
	case Instruction::MSTORE:
	case Instruction::MSTORE8:
	case Instruction::SSTORE:
	case Instruction::GAS:
	case Instruction::CREATE:
		return true;

	default:
		return false;
	}
}

}

GasMeter::GasMeter(llvm::IRBuilder<>& _builder, RuntimeManager& _runtimeManager) :
	CompilerHelper(_builder),
	m_runtimeManager(_runtimeManager)
{
	auto module = getModule();

	m_gasCheckFunc = llvm::Function::Create(llvm::FunctionType::get(Type::Void, Type::Word, false), llvm::Function::PrivateLinkage, "gas.check", module);
	InsertPointGuard guard(m_builder);

	auto checkBB = llvm::BasicBlock::Create(_builder.getContext(), "Check", m_gasCheckFunc);
	auto outOfGasBB = llvm::BasicBlock::Create(_builder.getContext(), "OutOfGas", m_gasCheckFunc);
	auto updateBB = llvm::BasicBlock::Create(_builder.getContext(), "Update", m_gasCheckFunc);

	m_builder.SetInsertPoint(checkBB);
	llvm::Value* cost = m_gasCheckFunc->arg_begin();
	cost->setName("cost");
	auto gas = m_runtimeManager.getGas();
	auto isOutOfGas = m_builder.CreateICmpUGT(cost, gas, "isOutOfGas");
	m_builder.CreateCondBr(isOutOfGas, outOfGasBB, updateBB);

	m_builder.SetInsertPoint(outOfGasBB);
	_runtimeManager.raiseException(ReturnCode::OutOfGas);
	m_builder.CreateUnreachable();

	m_builder.SetInsertPoint(updateBB);
	gas = m_builder.CreateSub(gas, cost);
	m_runtimeManager.setGas(gas);
	m_builder.CreateRetVoid();
}

void GasMeter::count(Instruction _inst)
{
	if (!m_checkCall)
	{
		// Create gas check call with mocked block cost at begining of current cost-block
		m_checkCall = m_builder.CreateCall(m_gasCheckFunc, llvm::UndefValue::get(Type::Word));
	}

	m_blockCost += getStepCost(_inst);

	if (isCostBlockEnd(_inst))
		commitCostBlock();
}

void GasMeter::countSStore(Ext& _ext, llvm::Value* _index, llvm::Value* _newValue)
{
	assert(!m_checkCall); // Everything should've been commited before

	auto oldValue = _ext.store(_index);
	auto oldValueIsZero = m_builder.CreateICmpEQ(oldValue, Constant::get(0), "oldValueIsZero");
	auto newValueIsZero = m_builder.CreateICmpEQ(_newValue, Constant::get(0), "newValueIsZero");
	auto oldValueIsntZero = m_builder.CreateICmpNE(oldValue, Constant::get(0), "oldValueIsntZero");
	auto newValueIsntZero = m_builder.CreateICmpNE(_newValue, Constant::get(0), "newValueIsntZero");
	auto isInsert = m_builder.CreateAnd(oldValueIsZero, newValueIsntZero, "isInsert");
	auto isDelete = m_builder.CreateAnd(oldValueIsntZero, newValueIsZero, "isDelete");
	auto cost = m_builder.CreateSelect(isInsert, Constant::get(c_sstoreSetGas), Constant::get(c_sstoreResetGas), "cost");
	cost = m_builder.CreateSelect(isDelete, Constant::get(0), cost, "cost");
	createCall(m_gasCheckFunc, cost);
}

void GasMeter::countLogData(llvm::Value* _dataLength)
{
	assert(m_checkCall);
	assert(m_blockCost > 0); // LOGn instruction is already counted
	auto cost = m_builder.CreateMul(_dataLength, Constant::get(c_logDataGas), "logdata_cost");
	commitCostBlock(cost);
}

void GasMeter::giveBack(llvm::Value* _gas)
{
	m_runtimeManager.setGas(m_builder.CreateAdd(m_runtimeManager.getGas(), _gas));
}

void GasMeter::commitCostBlock(llvm::Value* _additionalCost)
{
	assert(!_additionalCost || m_checkCall); // _additionalCost => m_checkCall; Must be inside cost-block

	// If any uncommited block
	if (m_checkCall)
	{
		if (m_blockCost == 0) // Do not check 0
		{
			m_checkCall->eraseFromParent(); // Remove the gas check call
			m_checkCall = nullptr;
			return;
		}

		m_checkCall->setArgOperand(0, Constant::get(m_blockCost)); // Update block cost in gas check call
		m_checkCall = nullptr; // End cost-block
		m_blockCost = 0;

		if (_additionalCost)
		{
			m_builder.CreateCall(m_gasCheckFunc, _additionalCost);
		}
	}
	assert(m_blockCost == 0);
}

void GasMeter::checkMemory(llvm::Value* _additionalMemoryInWords)
{
	auto cost = m_builder.CreateNUWMul(_additionalMemoryInWords, Constant::get(static_cast<uint64_t>(c_memoryGas)), "memcost");
	m_builder.CreateCall(m_gasCheckFunc, cost);
}

}
}
}

