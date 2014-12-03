
#include "Ext.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/TypeBuilder.h>
#include <llvm/IR/IntrinsicInst.h>

//#include <libdevcrypto/SHA3.h>
//#include <libevm/FeeStructure.h>

#include "Runtime.h"
#include "Type.h"
#include "Endianness.h"

namespace dev
{
namespace eth
{
namespace jit
{

// TODO: Copy of fromAddress in VM.h
inline u256 fromAddress(Address _a)
{
	return (u160)_a;
}

Ext::Ext(RuntimeManager& _runtimeManager):
	RuntimeHelper(_runtimeManager),
	m_data()
{
	auto&& ctx = m_builder.getContext();
	auto module = getModule();

	auto i256Ty = m_builder.getIntNTy(256);

	m_args[0] = m_builder.CreateAlloca(i256Ty, nullptr, "ext.index");
	m_args[1] = m_builder.CreateAlloca(i256Ty, nullptr, "ext.value");
	m_arg2 = m_builder.CreateAlloca(i256Ty, nullptr, "ext.arg2");
	m_arg3 = m_builder.CreateAlloca(i256Ty, nullptr, "ext.arg3");
	m_arg4 = m_builder.CreateAlloca(i256Ty, nullptr, "ext.arg4");
	m_arg5 = m_builder.CreateAlloca(i256Ty, nullptr, "ext.arg5");
	m_arg6 = m_builder.CreateAlloca(i256Ty, nullptr, "ext.arg6");
	m_arg7 = m_builder.CreateAlloca(i256Ty, nullptr, "ext.arg7");
	m_arg8 = m_builder.CreateAlloca(i256Ty, nullptr, "ext.arg8");

	using Linkage = llvm::GlobalValue::LinkageTypes;

	llvm::Type* argsTypes[] = {Type::RuntimePtr, Type::WordPtr, Type::WordPtr, Type::WordPtr, Type::WordPtr, Type::WordPtr, Type::WordPtr, Type::WordPtr, Type::WordPtr, Type::WordPtr};

	m_store = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {argsTypes, 3}, false), Linkage::ExternalLinkage, "ext_store", module);
	m_setStore = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {argsTypes, 3}, false), Linkage::ExternalLinkage, "ext_setStore", module);
	m_calldataload = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {argsTypes, 3}, false), Linkage::ExternalLinkage, "ext_calldataload", module);
	m_balance = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {argsTypes, 3}, false), Linkage::ExternalLinkage, "ext_balance", module);
	m_suicide = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {argsTypes, 2}, false), Linkage::ExternalLinkage, "ext_suicide", module);
	m_create = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {argsTypes, 5}, false), Linkage::ExternalLinkage, "ext_create", module);
	m_call = llvm::Function::Create(llvm::FunctionType::get(Type::Void, argsTypes, false), Linkage::ExternalLinkage, "ext_call", module);
	m_sha3 = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {argsTypes, 4}, false), Linkage::ExternalLinkage, "ext_sha3", module);
	m_exp = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {argsTypes, 4}, false), Linkage::ExternalLinkage, "ext_exp", module);
	m_codeAt = llvm::Function::Create(llvm::FunctionType::get(Type::BytePtr, {argsTypes, 2}, false), Linkage::ExternalLinkage, "ext_codeAt", module);
	m_codesizeAt = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {argsTypes, 3}, false), Linkage::ExternalLinkage, "ext_codesizeAt", module);
	m_log0 = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {argsTypes, 3}, false), Linkage::ExternalLinkage, "ext_log0", module);
	m_log1 = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {argsTypes, 4}, false), Linkage::ExternalLinkage, "ext_log1", module);
	m_log2 = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {argsTypes, 5}, false), Linkage::ExternalLinkage, "ext_log2", module);
	m_log3 = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {argsTypes, 6}, false), Linkage::ExternalLinkage, "ext_log3", module);
	m_log4 = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {argsTypes, 7}, false), Linkage::ExternalLinkage, "ext_log4", module);
}

llvm::Value* Ext::store(llvm::Value* _index)
{
	m_builder.CreateStore(_index, m_args[0]);
	m_builder.CreateCall3(m_store, getRuntimeManager().getRuntimePtr(), m_args[0], m_args[1]); // Uses native endianness
	return m_builder.CreateLoad(m_args[1]);
}

void Ext::setStore(llvm::Value* _index, llvm::Value* _value)
{
	m_builder.CreateStore(_index, m_args[0]);
	m_builder.CreateStore(_value, m_args[1]);
	m_builder.CreateCall3(m_setStore, getRuntimeManager().getRuntimePtr(), m_args[0], m_args[1]); // Uses native endianness
}

llvm::Value* Ext::calldataload(llvm::Value* _index)
{
	m_builder.CreateStore(_index, m_args[0]);
	m_builder.CreateCall3(m_calldataload, getRuntimeManager().getRuntimePtr(), m_args[0], m_args[1]);
	auto ret = m_builder.CreateLoad(m_args[1]);
	return Endianness::toNative(m_builder, ret);
}

llvm::Value* Ext::balance(llvm::Value* _address)
{
	auto address = Endianness::toBE(m_builder, _address);
	m_builder.CreateStore(address, m_args[0]);
	m_builder.CreateCall3(m_balance, getRuntimeManager().getRuntimePtr(), m_args[0], m_args[1]);
	return m_builder.CreateLoad(m_args[1]);
}

void Ext::suicide(llvm::Value* _address)
{
	auto address = Endianness::toBE(m_builder, _address);
	m_builder.CreateStore(address, m_args[0]);
	m_builder.CreateCall2(m_suicide, getRuntimeManager().getRuntimePtr(), m_args[0]);
}

llvm::Value* Ext::create(llvm::Value* _endowment, llvm::Value* _initOff, llvm::Value* _initSize)
{
	m_builder.CreateStore(_endowment, m_args[0]);
	m_builder.CreateStore(_initOff, m_arg2);
	m_builder.CreateStore(_initSize, m_arg3);
	createCall(m_create, getRuntimeManager().getRuntimePtr(), m_args[0], m_arg2, m_arg3, m_args[1]);
	llvm::Value* address = m_builder.CreateLoad(m_args[1]);
	address = Endianness::toNative(m_builder, address);
	return address;
}

llvm::Value* Ext::call(llvm::Value*& _gas, llvm::Value* _receiveAddress, llvm::Value* _value, llvm::Value* _inOff, llvm::Value* _inSize, llvm::Value* _outOff, llvm::Value* _outSize, llvm::Value* _codeAddress)
{
	m_builder.CreateStore(_gas, m_args[0]);
	auto receiveAddress = Endianness::toBE(m_builder, _receiveAddress);
	m_builder.CreateStore(receiveAddress, m_arg2);
	m_builder.CreateStore(_value, m_arg3);
	m_builder.CreateStore(_inOff, m_arg4);
	m_builder.CreateStore(_inSize, m_arg5);
	m_builder.CreateStore(_outOff, m_arg6);
	m_builder.CreateStore(_outSize, m_arg7);
	auto codeAddress = Endianness::toBE(m_builder, _codeAddress);
	m_builder.CreateStore(codeAddress, m_arg8);
	createCall(m_call, getRuntimeManager().getRuntimePtr(), m_args[0], m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_args[1]);
	_gas = m_builder.CreateLoad(m_args[0]); // Return gas
	return m_builder.CreateLoad(m_args[1]);
}

llvm::Value* Ext::sha3(llvm::Value* _inOff, llvm::Value* _inSize)
{
	m_builder.CreateStore(_inOff, m_args[0]);
	m_builder.CreateStore(_inSize, m_arg2);
	createCall(m_sha3, getRuntimeManager().getRuntimePtr(), m_args[0], m_arg2, m_args[1]);
	llvm::Value* hash = m_builder.CreateLoad(m_args[1]);
	hash = Endianness::toNative(m_builder, hash);
	return hash;
}

llvm::Value* Ext::exp(llvm::Value* _left, llvm::Value* _right)
{
	// TODO: Move ext to Arith256
	m_builder.CreateStore(_left, m_args[0]);
	m_builder.CreateStore(_right, m_arg2);
	createCall(m_exp, getRuntimeManager().getRuntimePtr(), m_args[0], m_arg2, m_args[1]);
	return m_builder.CreateLoad(m_args[1]);
}

llvm::Value* Ext::codeAt(llvm::Value* _addr)
{
	auto addr = Endianness::toBE(m_builder, _addr);
	m_builder.CreateStore(addr, m_args[0]);
	return m_builder.CreateCall2(m_codeAt, getRuntimeManager().getRuntimePtr(), m_args[0]);
}

llvm::Value* Ext::codesizeAt(llvm::Value* _addr)
{
	auto addr = Endianness::toBE(m_builder, _addr);
	m_builder.CreateStore(addr, m_args[0]);
	createCall(m_codesizeAt, getRuntimeManager().getRuntimePtr(), m_args[0], m_args[1]);
	return m_builder.CreateLoad(m_args[1]);
}

void Ext::log(llvm::Value* _memIdx, llvm::Value* _numBytes, size_t _numTopics, std::array<llvm::Value*,4> const& _topics)
{
	llvm::Value* args[] = {nullptr, m_args[0], m_args[1], m_arg2, m_arg3, m_arg4, m_arg5};
	llvm::Value* funcs[] = {m_log0, m_log1, m_log2, m_log3, m_log4};

	args[0] = getRuntimeManager().getRuntimePtr();
	m_builder.CreateStore(_memIdx, m_args[0]);
	m_builder.CreateStore(_numBytes, m_args[1]);

	for (size_t i = 0; i < _numTopics; ++i)
		m_builder.CreateStore(_topics[i], args[i + 3]);

	m_builder.CreateCall(funcs[_numTopics], llvm::ArrayRef<llvm::Value*>(args, _numTopics + 3));
}

}


extern "C"
{

	using namespace dev::eth::jit;

	EXPORT void ext_store(Runtime* _rt, i256* _index, i256* o_value)
	{
		auto index = llvm2eth(*_index);
		auto value = _rt->getExt().store(index); // Interface uses native endianness
		*o_value = eth2llvm(value);
	}

	EXPORT void ext_setStore(Runtime* _rt, i256* _index, i256* _value)
	{
		auto index = llvm2eth(*_index);
		auto value = llvm2eth(*_value);
		auto& ext = _rt->getExt();

		if (value == 0 && ext.store(index) != 0)	// If delete
			ext.sub.refunds += c_sstoreRefundGas;	// Increase refund counter

		ext.setStore(index, value);	// Interface uses native endianness
	}

	EXPORT void ext_calldataload(Runtime* _rt, i256* _index, i256* o_value)
	{
		auto index = static_cast<size_t>(llvm2eth(*_index));
		assert(index + 31 > index); // TODO: Handle large index
		auto b = reinterpret_cast<byte*>(o_value);
		for (size_t i = index, j = 0; i <= index + 31; ++i, ++j)
			b[j] = i < _rt->getExt().data.size() ? _rt->getExt().data[i] : 0; // Keep Big Endian
		// TODO: It all can be done by adding padding to data or by using min() algorithm without branch
	}

	EXPORT void ext_balance(Runtime* _rt, h256* _address, i256* o_value)
	{
		auto u = _rt->getExt().balance(right160(*_address));
		*o_value = eth2llvm(u);
	}

	EXPORT void ext_suicide(Runtime* _rt, h256 const* _address)
	{
		_rt->getExt().suicide(right160(*_address));
	}

	EXPORT void ext_create(Runtime* _rt, i256* _endowment, i256* _initOff, i256* _initSize, h256* o_address)
	{
		auto&& ext = _rt->getExt();
		auto endowment = llvm2eth(*_endowment);

		if (ext.balance(ext.myAddress) >= endowment)
		{
			ext.subBalance(endowment);
			u256 gas;   // TODO: Handle gas
			auto initOff = static_cast<size_t>(llvm2eth(*_initOff));
			auto initSize = static_cast<size_t>(llvm2eth(*_initSize));
			auto&& initRef = bytesConstRef(_rt->getMemory().data() + initOff, initSize);
			OnOpFunc onOp {}; // TODO: Handle that thing
			h256 address(ext.create(endowment, &gas, initRef, onOp), h256::AlignRight);
			*o_address = address;
		}
		else
			*o_address = {};
	}



	EXPORT void ext_call(Runtime* _rt, i256* io_gas, h256* _receiveAddress, i256* _value, i256* _inOff, i256* _inSize, i256* _outOff, i256* _outSize, h256* _codeAddress, i256* o_ret)
	{
		auto&& ext = _rt->getExt();
		auto value = llvm2eth(*_value);

		auto ret = false;
		auto gas = llvm2eth(*io_gas);
		if (ext.balance(ext.myAddress) >= value)
		{
			ext.subBalance(value);
			auto receiveAddress = right160(*_receiveAddress);
			auto inOff = static_cast<size_t>(llvm2eth(*_inOff));
			auto inSize = static_cast<size_t>(llvm2eth(*_inSize));
			auto outOff = static_cast<size_t>(llvm2eth(*_outOff));
			auto outSize = static_cast<size_t>(llvm2eth(*_outSize));
			auto&& inRef = bytesConstRef(_rt->getMemory().data() + inOff, inSize);
			auto&& outRef = bytesConstRef(_rt->getMemory().data() + outOff, outSize);
			OnOpFunc onOp {}; // TODO: Handle that thing
			auto codeAddress = right160(*_codeAddress);
			ret = ext.call(receiveAddress, value, inRef, &gas, outRef, onOp, {}, codeAddress);
		}

		*io_gas = eth2llvm(gas);
		o_ret->a = ret ? 1 : 0;
	}

	EXPORT void ext_sha3(Runtime* _rt, i256* _inOff, i256* _inSize, i256* o_ret)
	{
		auto inOff = static_cast<size_t>(llvm2eth(*_inOff));
		auto inSize = static_cast<size_t>(llvm2eth(*_inSize));
		auto dataRef = bytesConstRef(_rt->getMemory().data() + inOff, inSize);
		auto hash = sha3(dataRef);
		*o_ret = *reinterpret_cast<i256*>(&hash);
	}

	EXPORT void ext_exp(Runtime*, i256* _left, i256* _right, i256* o_ret)
	{
		bigint left = llvm2eth(*_left);
		bigint right = llvm2eth(*_right);
		auto ret = static_cast<u256>(boost::multiprecision::powm(left, right, bigint(2) << 256));
		*o_ret = eth2llvm(ret);
	}

	EXPORT unsigned char* ext_codeAt(Runtime* _rt, h256* _addr256)
	{
		auto&& ext = _rt->getExt();
		auto addr = right160(*_addr256);
		auto& code = ext.codeAt(addr);
		return const_cast<unsigned char*>(code.data());
	}

	EXPORT void ext_codesizeAt(Runtime* _rt, h256* _addr256, i256* o_ret)
	{
		auto&& ext = _rt->getExt();
		auto addr = right160(*_addr256);
		auto& code = ext.codeAt(addr);
		*o_ret = eth2llvm(u256(code.size()));
	}

	void ext_show_bytes(bytesConstRef _bytes)
	{
		for (auto b : _bytes)
			std::cerr << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(b) << " ";
		std::cerr << std::endl;
	}

	EXPORT void ext_log0(Runtime* _rt, i256* _memIdx, i256* _numBytes)
	{
		auto&& ext = _rt->getExt();

		auto memIdx = llvm2eth(*_memIdx).convert_to<size_t>();
		auto numBytes = llvm2eth(*_numBytes).convert_to<size_t>();

		auto dataRef = bytesConstRef(_rt->getMemory().data() + memIdx, numBytes);
		ext.log({}, dataRef);

		if (_rt->outputLogs())
		{
			std::cerr << "LOG: ";
			ext_show_bytes(dataRef);
		}
	}

	EXPORT void ext_log1(Runtime* _rt, i256* _memIdx, i256* _numBytes, i256* _topic1)
	{
		auto&& ext = _rt->getExt();

		auto memIdx = static_cast<size_t>(llvm2eth(*_memIdx));
		auto numBytes = static_cast<size_t>(llvm2eth(*_numBytes));

		auto topic1 = llvm2eth(*_topic1);

		auto dataRef = bytesConstRef(_rt->getMemory().data() + memIdx, numBytes);
		ext.log({topic1}, dataRef);

		if (_rt->outputLogs())
		{
			std::cerr << "LOG [" << topic1 << "]: ";
			ext_show_bytes(dataRef);
		}
	}

	EXPORT void ext_log2(Runtime* _rt, i256* _memIdx, i256* _numBytes, i256* _topic1, i256* _topic2)
	{
		auto&& ext = _rt->getExt();

		auto memIdx = static_cast<size_t>(llvm2eth(*_memIdx));
		auto numBytes = static_cast<size_t>(llvm2eth(*_numBytes));

		auto topic1 = llvm2eth(*_topic1);
		auto topic2 = llvm2eth(*_topic2);

		auto dataRef = bytesConstRef(_rt->getMemory().data() + memIdx, numBytes);
		ext.log({topic1, topic2}, dataRef);

		if (_rt->outputLogs())
		{
			std::cerr << "LOG [" << topic1 << "][" << topic2 << "]: ";
			ext_show_bytes(dataRef);
		}
	}

	EXPORT void ext_log3(Runtime* _rt, i256* _memIdx, i256* _numBytes, i256* _topic1, i256* _topic2, i256* _topic3)
	{
		auto&& ext = _rt->getExt();

		auto memIdx = static_cast<size_t>(llvm2eth(*_memIdx));
		auto numBytes = static_cast<size_t>(llvm2eth(*_numBytes));

		auto topic1 = llvm2eth(*_topic1);
		auto topic2 = llvm2eth(*_topic2);
		auto topic3 = llvm2eth(*_topic3);

		auto dataRef = bytesConstRef(_rt->getMemory().data() + memIdx, numBytes);
		ext.log({topic1, topic2, topic3}, dataRef);

		if (_rt->outputLogs())
		{
			std::cerr << "LOG [" << topic1 << "][" << topic2 << "][" << topic3 << "]: ";
			ext_show_bytes(dataRef);
		}
	}

	EXPORT void ext_log4(Runtime* _rt, i256* _memIdx, i256* _numBytes, i256* _topic1, i256* _topic2, i256* _topic3, i256* _topic4)
	{
		auto&& ext = _rt->getExt();

		auto memIdx = static_cast<size_t>(llvm2eth(*_memIdx));
		auto numBytes = static_cast<size_t>(llvm2eth(*_numBytes));

		auto topic1 = llvm2eth(*_topic1);
		auto topic2 = llvm2eth(*_topic2);
		auto topic3 = llvm2eth(*_topic3);
		auto topic4 = llvm2eth(*_topic4);

		auto dataRef = bytesConstRef(_rt->getMemory().data() + memIdx, numBytes);
		ext.log({topic1, topic2, topic3, topic4}, dataRef);

		if (_rt->outputLogs())
		{
			std::cerr << "LOG [" << topic1 << "][" << topic2 << "][" << topic3 << "][" << topic4 << "]: ";
			ext_show_bytes(dataRef);
		}
	}
}
}
}

