#include "Arith256.h"
#include "Runtime.h"
#include "Type.h"
#include "Endianness.h"

#include <llvm/IR/Function.h>
#include <gmp.h>

namespace dev
{
namespace eth
{
namespace jit
{

Arith256::Arith256(llvm::IRBuilder<>& _builder) :
	CompilerHelper(_builder)
{
	using namespace llvm;

	m_result = m_builder.CreateAlloca(Type::Word, nullptr, "arith.result");
	m_arg1 = m_builder.CreateAlloca(Type::Word, nullptr, "arith.arg1");
	m_arg2 = m_builder.CreateAlloca(Type::Word, nullptr, "arith.arg2");
	m_arg3 = m_builder.CreateAlloca(Type::Word, nullptr, "arith.arg3");

	using Linkage = GlobalValue::LinkageTypes;

	llvm::Type* arg2Types[] = {Type::WordPtr, Type::WordPtr, Type::WordPtr};
	llvm::Type* arg3Types[] = {Type::WordPtr, Type::WordPtr, Type::WordPtr, Type::WordPtr};

	m_mul = Function::Create(FunctionType::get(Type::Void, arg2Types, false), Linkage::ExternalLinkage, "arith_mul", getModule());
	m_div = Function::Create(FunctionType::get(Type::Void, arg3Types, false), Linkage::ExternalLinkage, "arith_div", getModule());
	m_sdiv = Function::Create(FunctionType::get(Type::Void, arg3Types, false), Linkage::ExternalLinkage, "arith_sdiv", getModule());
	m_exp = Function::Create(FunctionType::get(Type::Void, arg2Types, false), Linkage::ExternalLinkage, "arith_exp", getModule());
	m_addmod = Function::Create(FunctionType::get(Type::Void, arg3Types, false), Linkage::ExternalLinkage, "arith_addmod", getModule());
	m_mulmod = Function::Create(FunctionType::get(Type::Void, arg3Types, false), Linkage::ExternalLinkage, "arith_mulmod", getModule());
}

Arith256::~Arith256()
{}

llvm::Value* Arith256::binaryOp(llvm::Function* _op, llvm::Value* _arg1, llvm::Value* _arg2)
{
	m_builder.CreateStore(_arg1, m_arg1);
	m_builder.CreateStore(_arg2, m_arg2);
	m_builder.CreateCall3(_op, m_arg1, m_arg2, m_result);
	return m_builder.CreateLoad(m_result);
}

llvm::Value* Arith256::ternaryOp(llvm::Function* _op, llvm::Value* _arg1, llvm::Value* _arg2, llvm::Value* _arg3)
{
	m_builder.CreateStore(_arg1, m_arg1);
	m_builder.CreateStore(_arg2, m_arg2);
	m_builder.CreateStore(_arg3, m_arg3);
	m_builder.CreateCall4(_op, m_arg1, m_arg2, m_arg3, m_result);
	return m_builder.CreateLoad(m_result);
}

llvm::Value* Arith256::mul(llvm::Value* _arg1, llvm::Value* _arg2)
{
	return binaryOp(m_mul, _arg1, _arg2);
}

std::pair<llvm::Value*, llvm::Value*> Arith256::div(llvm::Value* _arg1, llvm::Value* _arg2)
{
	m_builder.CreateStore(_arg1, m_arg1);
	m_builder.CreateStore(_arg2, m_arg2);
	createCall(m_div, {m_arg1, m_arg2, m_arg3, m_result});
	return std::make_pair(m_builder.CreateLoad(m_arg3), m_builder.CreateLoad(m_result));
}

std::pair<llvm::Value*, llvm::Value*> Arith256::sdiv(llvm::Value* _x, llvm::Value* _y)
{
	auto xIsNeg = m_builder.CreateICmpSLT(_x, Constant::get(0));
	auto xNeg = m_builder.CreateSub(Constant::get(0), _x);
	auto xAbs = m_builder.CreateSelect(xIsNeg, xNeg, _x);

	auto yIsNeg = m_builder.CreateICmpSLT(_y, Constant::get(0));
	auto yNeg = m_builder.CreateSub(Constant::get(0), _y);
	auto yAbs = m_builder.CreateSelect(yIsNeg, yNeg, _y);

	m_builder.CreateStore(xAbs, m_arg1);
	m_builder.CreateStore(yAbs, m_arg2);
	createCall(m_div, {m_arg1, m_arg2, m_arg3, m_result});
	auto qAbs = m_builder.CreateLoad(m_arg3);
	auto rAbs = m_builder.CreateLoad(m_result);

	// the reminder has the same sign as dividend
	auto rNeg = m_builder.CreateSub(Constant::get(0), rAbs);
	auto r = m_builder.CreateSelect(xIsNeg, rNeg, rAbs);

	auto qNeg = m_builder.CreateSub(Constant::get(0), qAbs);
	auto xyOpposite = m_builder.CreateXor(xIsNeg, yIsNeg);
	auto q = m_builder.CreateSelect(xyOpposite, qNeg, qAbs);

	return std::make_pair(q, r);
}

llvm::Value* Arith256::exp(llvm::Value* _arg1, llvm::Value* _arg2)
{
	return binaryOp(m_exp, _arg1, _arg2);
}

llvm::Value* Arith256::addmod(llvm::Value* _arg1, llvm::Value* _arg2, llvm::Value* _arg3)
{
	return ternaryOp(m_addmod, _arg1, _arg2, _arg3);
}

llvm::Value* Arith256::mulmod(llvm::Value* _arg1, llvm::Value* _arg2, llvm::Value* _arg3)
{
	return ternaryOp(m_mulmod, _arg1, _arg2, _arg3);
}

namespace
{
	using uint128 = __uint128_t;

//	uint128 add(uint128 a, uint128 b) { return a + b; }
//	uint128 mul(uint128 a, uint128 b) { return a * b; }
//
//	uint128 mulq(uint64_t x, uint64_t y)
//	{
//		return (uint128)x * (uint128)y;
//	}
//
//	uint128 addc(uint64_t x, uint64_t y)
//	{
//		return (uint128)x * (uint128)y;
//	}

	struct uint256
	{
		uint64_t lo;
		uint64_t mid;
		uint128 hi;
	};

//	uint256 add(uint256 x, uint256 y)
//	{
//		auto lo = (uint128) x.lo + y.lo;
//		auto mid = (uint128) x.mid + y.mid + (lo >> 64);
//		return {lo, mid, x.hi + y.hi + (mid >> 64)};
//	}

	uint256 mul(uint256 x, uint256 y)
	{
		auto t1 = (uint128) x.lo * y.lo;
		auto t2 = (uint128) x.lo * y.mid;
		auto t3 = x.lo * y.hi;
		auto t4 = (uint128) x.mid * y.lo;
		auto t5 = (uint128) x.mid * y.mid;
		auto t6 = x.mid * y.hi;
		auto t7 = x.hi * y.lo;
		auto t8 = x.hi * y.mid;

		auto lo = (uint64_t) t1;
		auto m1 = (t1 >> 64) + (uint64_t) t2;
		auto m2 = (uint64_t) m1;
		auto mid = (uint128) m2 + (uint64_t) t4;
		auto hi = (t2 >> 64) + t3 + (t4 >> 64) + t5 + (t6 << 64) + t7
			 + (t8 << 64) + (m1 >> 64) + (mid >> 64);

		return {lo, (uint64_t)mid, hi};
	}

	bool isZero(i256 const* _n)
	{
		return _n->a == 0 && _n->b == 0 && _n->c == 0 && _n->d == 0;
	}

	const auto nLimbs = sizeof(i256) / sizeof(mp_limb_t);

	int countLimbs(i256 const* _n)
	{
		static const auto limbsInWord = sizeof(_n->a) / sizeof(mp_limb_t);
		static_assert(limbsInWord == 1, "E?");

		int l = nLimbs;
		if (_n->d != 0) return l;
		l -= limbsInWord;
		if (_n->c != 0) return l;
		l -= limbsInWord;
		if (_n->b != 0) return l;
		l -= limbsInWord;
		if (_n->a != 0) return l;
		return 0;
	}
}

}
}
}


extern "C"
{

	using namespace dev::eth::jit;

	EXPORT void arith_mul(uint256* _arg1, uint256* _arg2, uint256* o_result)
	{
		*o_result = mul(*_arg1, *_arg2);
	}

	EXPORT void arith_div(i256* _arg1, i256* _arg2, i256* o_div, i256* o_mod)
	{
		*o_div = {};
		*o_mod = {};
		if (isZero(_arg2))
			return;

		mpz_t x{nLimbs, countLimbs(_arg1), reinterpret_cast<mp_limb_t*>(_arg1)};
		mpz_t y{nLimbs, countLimbs(_arg2), reinterpret_cast<mp_limb_t*>(_arg2)};
		mpz_t q{nLimbs, 0, reinterpret_cast<mp_limb_t*>(o_div)};
		mpz_t r{nLimbs, 0, reinterpret_cast<mp_limb_t*>(o_mod)};

		mpz_tdiv_qr(q, r, x, y);
	}

	EXPORT void arith_exp(i256* _arg1, i256* _arg2, i256* o_result)
	{
		*o_result = {};

		static mp_limb_t mod_limbs[nLimbs + 1] = {};
		mod_limbs[nLimbs] = 1;
		static const mpz_t mod{nLimbs + 1, nLimbs + 1, &mod_limbs[0]};

		mpz_t x{nLimbs, countLimbs(_arg1), reinterpret_cast<mp_limb_t*>(_arg1)};
		mpz_t y{nLimbs, countLimbs(_arg2), reinterpret_cast<mp_limb_t*>(_arg2)};
		mpz_t z{nLimbs, 0, reinterpret_cast<mp_limb_t*>(o_result)};

		mpz_powm(z, x, y, mod);
	}

	EXPORT void arith_mulmod(i256* _arg1, i256* _arg2, i256* _arg3, i256* o_result)
	{
		*o_result = {};
		if (isZero(_arg3))
			return;

		mpz_t x{nLimbs, countLimbs(_arg1), reinterpret_cast<mp_limb_t*>(_arg1)};
		mpz_t y{nLimbs, countLimbs(_arg2), reinterpret_cast<mp_limb_t*>(_arg2)};
		mpz_t m{nLimbs, countLimbs(_arg3), reinterpret_cast<mp_limb_t*>(_arg3)};
		mpz_t z{nLimbs, 0, reinterpret_cast<mp_limb_t*>(o_result)};
		static mp_limb_t p_limbs[nLimbs * 2] = {};
		static mpz_t p{nLimbs * 2, 0, &p_limbs[0]};

		mpz_mul(p, x, y);
		mpz_tdiv_r(z, p, m);
	}

	EXPORT void arith_addmod(i256* _arg1, i256* _arg2, i256* _arg3, i256* o_result)
	{
		*o_result = {};
		if (isZero(_arg3))
			return;

		mpz_t x{nLimbs, countLimbs(_arg1), reinterpret_cast<mp_limb_t*>(_arg1)};
		mpz_t y{nLimbs, countLimbs(_arg2), reinterpret_cast<mp_limb_t*>(_arg2)};
		mpz_t m{nLimbs, countLimbs(_arg3), reinterpret_cast<mp_limb_t*>(_arg3)};
		mpz_t z{nLimbs, 0, reinterpret_cast<mp_limb_t*>(o_result)};
		static mp_limb_t s_limbs[nLimbs + 1] = {};
		static mpz_t s{nLimbs + 1, 0, &s_limbs[0]};

		mpz_add(s, x, y);
		mpz_tdiv_r(z, s, m);
	}

}


