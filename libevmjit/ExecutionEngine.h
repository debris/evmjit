#pragma once

namespace llvm
{
	class Module;
}

#include "RuntimeData.h"

namespace dev
{
namespace eth
{
namespace jit
{

class ExecutionEngine
{
public:
	ExecutionEngine() = default;
	ExecutionEngine(ExecutionEngine const&) = delete;
	void operator=(ExecutionEngine) = delete;

	ReturnCode run(bytes const& _code, RuntimeData* _data, Env* _env);
	ReturnCode run(std::unique_ptr<llvm::Module> module, RuntimeData* _data, Env* _env);

	bytes returnData;
};

}
}
}
