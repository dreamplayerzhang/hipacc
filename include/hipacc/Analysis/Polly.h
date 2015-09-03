//
// Copyright (c) 2012, University of Erlangen-Nuremberg
// Copyright (c) 2012, Siemens AG
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

//===--- Polly.h - Polyhedral Analysis for Kernels using Polly ------------===//
//
// This file implements the interface to Polly for kernel transformations like
// loop fusion.
//
//===----------------------------------------------------------------------===//

#ifndef _POLLY_H_
#define _POLLY_H_

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclGroup.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/CodeGen/ModuleBuilder.h>

namespace clang {
namespace hipacc {
  class Polly {
    private:
      ASTContext &Ctx;
      CompilerInstance &Clang;
      FunctionDecl *func;

    public:
      Polly(ASTContext &Ctx, CompilerInstance &Clang, FunctionDecl *func) :
        Ctx(Ctx),
        Clang(Clang),
        func(func)
      {}

      void analyzeKernel();
  };
} // namespace hipacc
} // namespace clang

#endif  // _POLLY_H_

// vim: set ts=2 sw=2 sts=2 et ai:

