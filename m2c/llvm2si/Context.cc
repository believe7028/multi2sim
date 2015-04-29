/*
 *  Multi2Sim
 *  Copyright (C) 2013  Rafael Ubal (ubal@ece.neu.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <fstream>
#include <iostream>

#include <lib/cpp/Misc.h>
#include <llvm/ADT/OwningPtr.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/system_error.h>
#include <llvm/Support/MemoryBuffer.h>

#include "Context.h"
#include "Module.h"
#include "Function.h"

// TODO: Figure out a way to include all passes once.
#include "passes/DataDependencyPass.h"
#include "passes/LivenessAnalysisPass.h"

namespace llvm2si
{

std::unique_ptr<Context> Context::instance;

bool Context::active;


Context *Context::getInstance()
{
	// Instance already exists
	if (instance.get())
		return instance.get();

	// Create Instance
	instance.reset(new Context());
	return instance.get();
}


void Context::Parse(const std::string &in, const std::string &out)
{
	// Load file content
	llvm::LLVMContext &llvm_context = llvm::getGlobalContext();
	llvm::OwningPtr<llvm::MemoryBuffer> llvm_memory_buffer;
	llvm::error_code ec = llvm::MemoryBuffer::getFile(in, llvm_memory_buffer);
	if (ec)
		throw Error(misc::fmt("[%s] %s", in.c_str(),
				ec.message().c_str()));

	// Load module
	llvm::OwningPtr<llvm::Module> llvm_module(
			llvm::ParseBitcodeFile(llvm_memory_buffer.get(),
					llvm_context));


	// Open output file
	std::ofstream f(out);
	if (!f)
		throw Error(misc::fmt("[%s] Cannot open output file",
				out.c_str()));

	// The module for this context.
	Module module(llvm_module.get());

	// Translate all functions
	for (auto &llvm_function : llvm_module->getFunctionList())
	{
		// Ignore built-in functions. Built-in function declarations are
		// generated by the CL-to-LLVM front-end with the 'nounwind'
		// attribute.
		if (!llvm_function.hasFnAttribute(llvm::Attribute::NoUnwind))
			continue;

		// Create function
		Function *function = module.newFunction(&llvm_function);

		// TODO: Remove this in place of actual Pass running.
		DataDependencyPass ddp;
		ddp.run();

		// Emit code for function
		function->EmitHeader();
		function->EmitArgs();
		function->EmitBody();
		function->EmitControlFlow();
		function->EmitPhi();

		LivenessAnalysisPass lap(function);
		lap.run();
		//// lap.dump(std::cout);

		// Dump code
		f << *function;
	}
}


void Context::RegisterOptions()
{
	// Get command line object
	misc::CommandLine *command_line = misc::CommandLine::getInstance();

	// Category
	command_line->setCategory("Southern Islands back-end");

	// Option to activate stand-alone Southern Islands back-end
	command_line->RegisterBool("--llvm2si", active,
			"Interpret the source files as LLVM files (.llvm) and "
			"translate them to Southern Islands assembly code "
			"(.s) using the Southern Islands back-end.");
}


void Context::ProcessOptions()
{
}


}  // namespace si2bin
