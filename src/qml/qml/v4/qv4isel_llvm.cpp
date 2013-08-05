/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunused-parameter"
#endif // __clang__

#include <llvm/Analysis/Passes.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/ExecutionEngine/JITMemoryManager.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/system_error.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Linker.h>

#ifdef __clang__
#  pragma clang diagnostic pop
#endif // __clang__

#include <QtCore/QFileInfo>
#include <QtCore/QLibrary>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <cstdio>
#include <iostream>

// These includes have to come last, because WTF/Platform.h defines some macros
// with very unfriendly names that collide with class fields in LLVM.
#include "qv4isel_llvm_p.h"
#include "qv4_llvm_p.h"
#include "qv4jsir_p.h"
#include "qv4string_p.h"
#include "qv4global_p.h"

namespace QQmlJS {

Q_QML_EXPORT int compileWithLLVM(V4IR::Module *module, const QString &fileName, LLVMOutputType outputType, int (*exec)(void *))
{
    Q_ASSERT(module);
    Q_ASSERT(exec || outputType != LLVMOutputJit);

    // TODO: should this be done here?
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86Target();
    LLVMInitializeX86AsmPrinter();
    LLVMInitializeX86AsmParser();
    LLVMInitializeX86Disassembler();
    LLVMInitializeX86TargetMC();

    //----

    llvm::InitializeNativeTarget();
    LLVM::InstructionSelection llvmIsel(llvm::getGlobalContext());

    const QString moduleName = QFileInfo(fileName).fileName();
    llvm::StringRef moduleId(moduleName.toUtf8().constData());
    llvm::Module *llvmModule = new llvm::Module(moduleId, llvmIsel.getContext());

    if (outputType == LLVMOutputJit) {
        // The execution engine takes ownership of the model. No need to delete it anymore.
        std::string errStr;
        llvm::ExecutionEngine *execEngine = llvm::EngineBuilder(llvmModule)
//                .setUseMCJIT(true)
                .setErrorStr(&errStr).create();
        if (!execEngine) {
            std::cerr << "Could not create LLVM JIT: " << errStr << std::endl;
            return EXIT_FAILURE;
        }

        llvm::FunctionPassManager functionPassManager(llvmModule);
        // Set up the optimizer pipeline.  Start with registering info about how the
        // target lays out data structures.
        functionPassManager.add(new llvm::DataLayout(*execEngine->getDataLayout()));
        // Promote allocas to registers.
        functionPassManager.add(llvm::createPromoteMemoryToRegisterPass());
        // Provide basic AliasAnalysis support for GVN.
        functionPassManager.add(llvm::createBasicAliasAnalysisPass());
        // Do simple "peephole" optimizations and bit-twiddling optzns.
        functionPassManager.add(llvm::createInstructionCombiningPass());
        // Reassociate expressions.
        functionPassManager.add(llvm::createReassociatePass());
        // Eliminate Common SubExpressions.
        functionPassManager.add(llvm::createGVNPass());
        // Simplify the control flow graph (deleting unreachable blocks, etc).
        functionPassManager.add(llvm::createCFGSimplificationPass());

        functionPassManager.doInitialization();

        llvmIsel.buildLLVMModule(module, llvmModule, &functionPassManager);

        llvm::Function *entryPoint = llvmModule->getFunction("%entry");
        Q_ASSERT(entryPoint);
        void *funcPtr = execEngine->getPointerToFunction(entryPoint);
        return exec(funcPtr);
    } else {
        llvm::FunctionPassManager functionPassManager(llvmModule);
        // Set up the optimizer pipeline.
        // Promote allocas to registers.
        functionPassManager.add(llvm::createPromoteMemoryToRegisterPass());
        // Provide basic AliasAnalysis support for GVN.
        functionPassManager.add(llvm::createBasicAliasAnalysisPass());
        // Do simple "peephole" optimizations and bit-twiddling optzns.
        functionPassManager.add(llvm::createInstructionCombiningPass());
        // Reassociate expressions.
        functionPassManager.add(llvm::createReassociatePass());
        // Eliminate Common SubExpressions.
        functionPassManager.add(llvm::createGVNPass());
        // Simplify the control flow graph (deleting unreachable blocks, etc).
        functionPassManager.add(llvm::createCFGSimplificationPass());

        functionPassManager.doInitialization();

        llvmIsel.buildLLVMModule(module, llvmModule, &functionPassManager);

        // TODO: if output type is .ll, print the module to file

        const std::string triple = llvm::sys::getDefaultTargetTriple();

        std::string err;
        const llvm::Target *target = llvm::TargetRegistry::lookupTarget(triple, err);
        if (! err.empty()) {
            std::cerr << err << ", triple: " << triple << std::endl;
            assert(!"cannot create target for the host triple");
        }

        std::string cpu;
        std::string features;
        llvm::TargetOptions options;
        llvm::TargetMachine *targetMachine = target->createTargetMachine(triple, cpu, features, options, llvm::Reloc::PIC_);
        assert(targetMachine);

        llvm::TargetMachine::CodeGenFileType ft;
        QString ofName;

        if (outputType == LLVMOutputObject) {
            ft = llvm::TargetMachine::CGFT_ObjectFile;
            ofName = fileName + QLatin1String(".o");
        } else if (outputType == LLVMOutputAssembler) {
            ft = llvm::TargetMachine::CGFT_AssemblyFile;
            ofName = fileName + QLatin1String(".s");
        } else {
            // ft is not used.
            ofName = fileName + QLatin1String(".ll");
        }

        llvm::raw_fd_ostream dest(ofName.toUtf8().constData(), err, llvm::raw_fd_ostream::F_Binary);
        llvm::formatted_raw_ostream destf(dest);
        if (!err.empty()) {
            std::cerr << err << std::endl;
            delete llvmModule;
        }

        llvm::PassManager globalPassManager;
        globalPassManager.add(llvm::createScalarReplAggregatesPass());
        globalPassManager.add(llvm::createInstructionCombiningPass());
        globalPassManager.add(llvm::createGlobalOptimizerPass());
        globalPassManager.add(llvm::createFunctionInliningPass(25));
//        globalPassManager.add(llvm::createFunctionInliningPass(125));

        if (outputType == LLVMOutputObject || outputType == LLVMOutputAssembler) {
            if (targetMachine->addPassesToEmitFile(globalPassManager, destf, ft)) {
                std::cerr << err << " (probably no DataLayout in TargetMachine)" << std::endl;
            } else {
                globalPassManager.run(*llvmModule);

                destf.flush();
                dest.flush();
            }
        } else { // .ll
            globalPassManager.run(*llvmModule);
            llvmModule->print(destf, 0);

            destf.flush();
            dest.flush();
        }

        delete llvmModule;
        return EXIT_SUCCESS;
    }
}

} // QQmlJS

using namespace QQmlJS;
using namespace QQmlJS::LLVM;

namespace {
QTextStream qerr(stderr, QIODevice::WriteOnly);
}

InstructionSelection::InstructionSelection(llvm::LLVMContext &context)
    : llvm::IRBuilder<>(context)
    , _llvmModule(0)
    , _llvmFunction(0)
    , _llvmValue(0)
    , _numberTy(0)
    , _valueTy(0)
    , _contextPtrTy(0)
    , _stringPtrTy(0)
    , _functionTy(0)
    , _allocaInsertPoint(0)
    , _function(0)
    , _block(0)
    , _fpm(0)
{
}

void InstructionSelection::buildLLVMModule(V4IR::Module *module, llvm::Module *llvmModule, llvm::FunctionPassManager *fpm)
{
    qSwap(_llvmModule, llvmModule);
    qSwap(_fpm, fpm);

    _numberTy = getDoubleTy();

    std::string err;

    llvm::OwningPtr<llvm::MemoryBuffer> buffer;
    qDebug()<<"llvm runtime:"<<LLVM_RUNTIME;
    llvm::error_code ec = llvm::MemoryBuffer::getFile(llvm::StringRef(LLVM_RUNTIME), buffer);
    if (ec) {
        qWarning() << ec.message().c_str();
        assert(!"cannot load QML/JS LLVM runtime, you can generate the runtime with the command `make llvm_runtime'");
    }

    llvm::Module *llvmRuntime = llvm::getLazyBitcodeModule(buffer.get(), getContext(), &err);
    if (! err.empty()) {
        qWarning() << err.c_str();
        assert(!"cannot load QML/JS LLVM runtime");
    }

    err.clear();
    llvm::Linker::LinkModules(_llvmModule, llvmRuntime, llvm::Linker::DestroySource, &err);
    if (! err.empty()) {
        qWarning() << err.c_str();
        assert(!"cannot link the QML/JS LLVM runtime");
    }

    _valueTy = _llvmModule->getTypeByName("struct.QV4::Value");
    _contextPtrTy = _llvmModule->getTypeByName("struct.QV4::ExecutionContext")->getPointerTo();
    _stringPtrTy = _llvmModule->getTypeByName("struct.QV4::String")->getPointerTo();

    {
        llvm::Type *args[] = { _contextPtrTy };
        _functionTy = llvm::FunctionType::get(getVoidTy(), llvm::makeArrayRef(args), false);
    }


    foreach (V4IR::Function *function, module->functions)
        (void) compileLLVMFunction(function);
    qSwap(_fpm, fpm);
    qSwap(_llvmModule, llvmModule);
}

void InstructionSelection::callBuiltinInvalid(V4IR::Name *func, V4IR::ExprList *args, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinTypeofMember(V4IR::Temp *base, const QString &name, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinTypeofSubscript(V4IR::Temp *base, V4IR::Temp *index, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinTypeofName(const QString &name, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinTypeofValue(V4IR::Temp *value, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinDeleteMember(V4IR::Temp *base, const QString &name, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinDeleteSubscript(V4IR::Temp *base, V4IR::Temp *index, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinDeleteName(const QString &name, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinDeleteValue(V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinPostDecrementMember(V4IR::Temp *base, const QString &name, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinPostDecrementSubscript(V4IR::Temp *base, V4IR::Temp *index, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinPostDecrementName(const QString &name, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinPostDecrementValue(V4IR::Temp *value, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinPostIncrementMember(V4IR::Temp *base, const QString &name, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinPostIncrementSubscript(V4IR::Temp *base, V4IR::Temp *index, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinPostIncrementName(const QString &name, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinPostIncrementValue(V4IR::Temp *value, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinThrow(V4IR::Temp *arg, int line)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinCreateExceptionHandler(V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinFinishTry()
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinForeachIteratorObject(V4IR::Temp *arg, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinForeachNextPropertyname(V4IR::Temp *arg, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinPushWithScope(V4IR::Temp *arg)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinPopScope()
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinDeclareVar(bool deletable, const QString &name)
{
    llvm::ConstantInt *isDeletable = getInt1(deletable != 0);
    llvm::Value *varName = getIdentifier(name);
    CreateCall3(getRuntimeFunction("__qmljs_builtin_declare_var"),
                _llvmFunction->arg_begin(), isDeletable, varName);
}

void InstructionSelection::callBuiltinDefineGetterSetter(V4IR::Temp *object, const QString &name, V4IR::Temp *getter, V4IR::Temp *setter)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinDefineProperty(V4IR::Temp *object, const QString &name, V4IR::Temp *value)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinDefineArray(V4IR::Temp *result, V4IR::ExprList *args)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callBuiltinDefineObjectLiteral(V4IR::Temp *result, V4IR::ExprList *args)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callValue(V4IR::Temp *value, V4IR::ExprList *args, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callProperty(V4IR::Temp *base, const QString &name, V4IR::ExprList *args, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::callSubscript(V4IR::Temp *base, V4IR::Temp *index, V4IR::ExprList *args, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::constructActivationProperty(V4IR::Name *func,
                                                       V4IR::ExprList *args,
                                                       V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::constructProperty(V4IR::Temp *base, const QString &name, V4IR::ExprList *args, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::constructValue(V4IR::Temp *value, V4IR::ExprList *args, V4IR::Temp *result)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::loadThisObject(V4IR::Temp *temp)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::loadConst(V4IR::Const *con, V4IR::Temp *temp)
{
    llvm::Value *target = getLLVMTemp(temp);
    llvm::Value *source = CreateLoad(createValue(con));
    CreateStore(source, target);
}

void InstructionSelection::loadString(const QString &str, V4IR::Temp *targetTemp)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::loadRegexp(V4IR::RegExp *sourceRegexp, V4IR::Temp *targetTemp)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::getActivationProperty(const V4IR::Name *name, V4IR::Temp *temp)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

void InstructionSelection::setActivationProperty(V4IR::Temp *source, const QString &targetName)
{
    llvm::Value *name = getIdentifier(targetName);
    llvm::Value *src = toValuePtr(source);
    CreateCall3(getRuntimeFunction("__qmljs_llvm_set_activation_property"),
                _llvmFunction->arg_begin(), name, src);
}

void InstructionSelection::initClosure(V4IR::Closure *closure, V4IR::Temp *target)
{
    V4IR::Function *f = closure->value;
    QString name;
    if (f->name)
        name = *f->name;

    llvm::Value *args[] = {
        _llvmFunction->arg_begin(),
        getLLVMTemp(target),
        getIdentifier(name),
        getInt1(f->hasDirectEval),
        getInt1(f->usesArgumentsObject),
        getInt1(f->isStrict),
        getInt1(!f->nestedFunctions.isEmpty()),
        genStringList(f->formals, "formals", "formal"),
        getInt32(f->formals.size()),
        genStringList(f->locals, "locals", "local"),
        getInt32(f->locals.size())
    };
    llvm::Function *callee = _llvmModule->getFunction("__qmljs_llvm_init_closure");
    CreateCall(callee, args);
}

void InstructionSelection::getProperty(V4IR::Temp *sourceBase, const QString &sourceName, V4IR::Temp *target)
{
    llvm::Value *base = getLLVMTempReference(sourceBase);
    llvm::Value *name = getIdentifier(sourceName);
    llvm::Value *t = getLLVMTemp(target);
    CreateCall4(getRuntimeFunction("__qmljs_llvm_get_property"),
                _llvmFunction->arg_begin(), t, base, name);
}

void InstructionSelection::setProperty(V4IR::Temp *source, V4IR::Temp *targetBase, const QString &targetName)
{
    llvm::Value *base = getLLVMTempReference(targetBase);
    llvm::Value *name = getIdentifier(targetName);
    llvm::Value *src = toValuePtr(source);
    CreateCall4(getRuntimeFunction("__qmljs_llvm_set_property"),
                _llvmFunction->arg_begin(), base, name, src);
}

void InstructionSelection::getElement(V4IR::Temp *sourceBase, V4IR::Temp *sourceIndex, V4IR::Temp *target)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();

    llvm::Value *base = getLLVMTempReference(sourceBase);
    llvm::Value *index = getLLVMTempReference(sourceIndex);
    llvm::Value *t = getLLVMTemp(target);
    CreateCall4(getRuntimeFunction("__qmljs_llvm_get_element"),
                _llvmFunction->arg_begin(), t, base, index);
}

void InstructionSelection::setElement(V4IR::Temp *source, V4IR::Temp *targetBase, V4IR::Temp *targetIndex)
{
    llvm::Value *base = getLLVMTempReference(targetBase);
    llvm::Value *index = getLLVMTempReference(targetIndex);
    llvm::Value *src = toValuePtr(source);
    CreateCall4(getRuntimeFunction("__qmljs_llvm_set_element"),
                _llvmFunction->arg_begin(), base, index, src);
}

void InstructionSelection::copyValue(V4IR::Temp *sourceTemp, V4IR::Temp *targetTemp)
{
    llvm::Value *t = getLLVMTemp(targetTemp);
    llvm::Value *s = getLLVMTemp(sourceTemp);
    CreateStore(s, t);
}

void InstructionSelection::unop(V4IR::AluOp oper, V4IR::Temp *sourceTemp, V4IR::Temp *targetTemp)
{
    const char *opName = 0;
    switch (oper) {
    case V4IR::OpNot: opName = "__qmljs_not"; break;
    case V4IR::OpUMinus: opName = "__qmljs_uminus"; break;
    case V4IR::OpUPlus: opName = "__qmljs_uplus"; break;
    case V4IR::OpCompl: opName = "__qmljs_compl"; break;
    case V4IR::OpIncrement: opName = "__qmljs_increment"; break;
    case V4IR::OpDecrement: opName = "__qmljs_decrement"; break;
    default: assert(!"unreachable"); break;
    }

    if (opName) {
        llvm::Value *t = getLLVMTemp(targetTemp);
        llvm::Value *s = getLLVMTemp(sourceTemp);
        CreateCall3(getRuntimeFunction(opName),
                    _llvmFunction->arg_begin(), t, s);
    }
}

void InstructionSelection::binop(V4IR::AluOp oper, V4IR::Expr *leftSource, V4IR::Expr *rightSource, V4IR::Temp *target)
{
    const char *opName = 0;
    switch (oper) {
    case V4IR::OpBitAnd: opName = "__qmljs_llvm_bit_and"; break;
    case V4IR::OpBitOr: opName = "__qmljs_llvm_bit_or"; break;
    case V4IR::OpBitXor: opName = "__qmljs_llvm_bit_xor"; break;
    case V4IR::OpAdd: opName = "__qmljs_llvm_add"; break;
    case V4IR::OpSub: opName = "__qmljs_llvm_sub"; break;
    case V4IR::OpMul: opName = "__qmljs_llvm_mul"; break;
    case V4IR::OpDiv: opName = "__qmljs_llvm_div"; break;
    case V4IR::OpMod: opName = "__qmljs_llvm_mod"; break;
    case V4IR::OpLShift: opName = "__qmljs_llvm_shl"; break;
    case V4IR::OpRShift: opName = "__qmljs_llvm_shr"; break;
    case V4IR::OpURShift: opName = "__qmljs_llvm_ushr"; break;
    default:
        Q_UNREACHABLE();
        break;
    }

    if (opName) {
        llvm::Value *t = getLLVMTemp(target);
        llvm::Value *s1 = toValuePtr(leftSource);
        llvm::Value *s2 = toValuePtr(rightSource);
        CreateCall4(getRuntimeFunction(opName),
                    _llvmFunction->arg_begin(), t, s1, s2);
        return;
    }
}

void InstructionSelection::inplaceNameOp(V4IR::AluOp oper, V4IR::Temp *rightSource, const QString &targetName)
{
    const char *opName = 0;
    switch (oper) {
    case V4IR::OpBitAnd: opName = "__qmljs_llvm_inplace_bit_and_name"; break;
    case V4IR::OpBitOr: opName = "__qmljs_llvm_inplace_bit_or_name"; break;
    case V4IR::OpBitXor: opName = "__qmljs_llvm_inplace_bit_xor_name"; break;
    case V4IR::OpAdd: opName = "__qmljs_llvm_inplace_add_name"; break;
    case V4IR::OpSub: opName = "__qmljs_llvm_inplace_sub_name"; break;
    case V4IR::OpMul: opName = "__qmljs_llvm_inplace_mul_name"; break;
    case V4IR::OpDiv: opName = "__qmljs_llvm_inplace_div_name"; break;
    case V4IR::OpMod: opName = "__qmljs_llvm_inplace_mod_name"; break;
    case V4IR::OpLShift: opName = "__qmljs_llvm_inplace_shl_name"; break;
    case V4IR::OpRShift: opName = "__qmljs_llvm_inplace_shr_name"; break;
    case V4IR::OpURShift: opName = "__qmljs_llvm_inplace_ushr_name"; break;
    default:
        Q_UNREACHABLE();
        break;
    }

    if (opName) {
        llvm::Value *dst = getIdentifier(targetName);
        llvm::Value *src = toValuePtr(rightSource);
        CreateCall3(getRuntimeFunction(opName),
                    _llvmFunction->arg_begin(), dst, src);
        return;
    }
}

void InstructionSelection::inplaceElementOp(V4IR::AluOp oper, V4IR::Temp *source, V4IR::Temp *targetBaseTemp, V4IR::Temp *targetIndexTemp)
{
    const char *opName = 0;
    switch (oper) {
    case V4IR::OpBitAnd: opName = "__qmljs_llvm_inplace_bit_and_element"; break;
    case V4IR::OpBitOr: opName = "__qmljs_llvm_inplace_bit_or_element"; break;
    case V4IR::OpBitXor: opName = "__qmljs_llvm_inplace_bit_xor_element"; break;
    case V4IR::OpAdd: opName = "__qmljs_llvm_inplace_add_element"; break;
    case V4IR::OpSub: opName = "__qmljs_llvm_inplace_sub_element"; break;
    case V4IR::OpMul: opName = "__qmljs_llvm_inplace_mul_element"; break;
    case V4IR::OpDiv: opName = "__qmljs_llvm_inplace_div_element"; break;
    case V4IR::OpMod: opName = "__qmljs_llvm_inplace_mod_element"; break;
    case V4IR::OpLShift: opName = "__qmljs_llvm_inplace_shl_element"; break;
    case V4IR::OpRShift: opName = "__qmljs_llvm_inplace_shr_element"; break;
    case V4IR::OpURShift: opName = "__qmljs_llvm_inplace_ushr_element"; break;
    default:
        Q_UNREACHABLE();
        break;
    }

    if (opName) {
        llvm::Value *base = getLLVMTemp(targetBaseTemp);
        llvm::Value *index = getLLVMTemp(targetIndexTemp);
        llvm::Value *value = toValuePtr(source);
        CreateCall4(getRuntimeFunction(opName),
                    _llvmFunction->arg_begin(), base, index, value);
    }
}

void InstructionSelection::inplaceMemberOp(V4IR::AluOp oper, V4IR::Temp *source, V4IR::Temp *targetBase, const QString &targetName)
{
    const char *opName = 0;
    switch (oper) {
    case V4IR::OpBitAnd: opName = "__qmljs_llvm_inplace_bit_and_member"; break;
    case V4IR::OpBitOr: opName = "__qmljs_llvm_inplace_bit_or_member"; break;
    case V4IR::OpBitXor: opName = "__qmljs_llvm_inplace_bit_xor_member"; break;
    case V4IR::OpAdd: opName = "__qmljs_llvm_inplace_add_member"; break;
    case V4IR::OpSub: opName = "__qmljs_llvm_inplace_sub_member"; break;
    case V4IR::OpMul: opName = "__qmljs_llvm_inplace_mul_member"; break;
    case V4IR::OpDiv: opName = "__qmljs_llvm_inplace_div_member"; break;
    case V4IR::OpMod: opName = "__qmljs_llvm_inplace_mod_member"; break;
    case V4IR::OpLShift: opName = "__qmljs_llvm_inplace_shl_member"; break;
    case V4IR::OpRShift: opName = "__qmljs_llvm_inplace_shr_member"; break;
    case V4IR::OpURShift: opName = "__qmljs_llvm_inplace_ushr_member"; break;
    default:
        Q_UNREACHABLE();
        break;
    }

    if (opName) {
        llvm::Value *base = getLLVMTemp(targetBase);
        llvm::Value *member = getIdentifier(targetName);
        llvm::Value *value = toValuePtr(source);
        CreateCall4(getRuntimeFunction(opName),
                    _llvmFunction->arg_begin(), value, base, member);
    }
}

llvm::Function *InstructionSelection::getLLVMFunction(V4IR::Function *function)
{
    llvm::Function *&f = _functionMap[function];
    if (! f) {
        QString name = QStringLiteral("__qmljs_native_");
        if (function->name) {
            if (*function->name == QStringLiteral("%entry"))
                name = *function->name;
            else
                name += *function->name;
        }
        f = llvm::Function::Create(_functionTy, llvm::Function::ExternalLinkage, // ### make it internal
                                   qPrintable(name), _llvmModule);
    }
    return f;
}

llvm::Function *InstructionSelection::compileLLVMFunction(V4IR::Function *function)
{
    llvm::Function *llvmFunction = getLLVMFunction(function);

    QHash<V4IR::BasicBlock *, llvm::BasicBlock *> blockMap;
    QVector<llvm::Value *> tempMap;

    qSwap(_llvmFunction, llvmFunction);
    qSwap(_function, function);
    qSwap(_tempMap, tempMap);
    qSwap(_blockMap, blockMap);

    // create the LLVM blocks
    foreach (V4IR::BasicBlock *block, _function->basicBlocks)
        (void) getLLVMBasicBlock(block);

    // entry block
    SetInsertPoint(getLLVMBasicBlock(_function->basicBlocks.first()));

    llvm::Instruction *allocaInsertPoint = new llvm::BitCastInst(llvm::UndefValue::get(getInt32Ty()),
                                                                 getInt32Ty(), "", GetInsertBlock());
    qSwap(_allocaInsertPoint, allocaInsertPoint);

    for (int i = 0; i < _function->tempCount; ++i) {
        llvm::AllocaInst *t = newLLVMTemp(_valueTy);
        _tempMap.append(t);
    }

    foreach (llvm::Value *t, _tempMap) {
        CreateStore(llvm::Constant::getNullValue(_valueTy), t);
    }

//    CreateCall(getRuntimeFunction("__qmljs_llvm_init_this_object"),
//               _llvmFunction->arg_begin());

    foreach (V4IR::BasicBlock *block, _function->basicBlocks) {
        qSwap(_block, block);
        SetInsertPoint(getLLVMBasicBlock(_block));
        foreach (V4IR::Stmt *s, _block->statements)
            s->accept(this);
        qSwap(_block, block);
    }

    qSwap(_allocaInsertPoint, allocaInsertPoint);

    allocaInsertPoint->eraseFromParent();

    qSwap(_blockMap, blockMap);
    qSwap(_tempMap, tempMap);
    qSwap(_function, function);
    qSwap(_llvmFunction, llvmFunction);

    // Validate the generated code, checking for consistency.
    llvm::verifyFunction(*llvmFunction);
    // Optimize the function.
    if (_fpm)
        _fpm->run(*llvmFunction);

    return llvmFunction;
}

llvm::BasicBlock *InstructionSelection::getLLVMBasicBlock(V4IR::BasicBlock *block)
{
    llvm::BasicBlock *&llvmBlock = _blockMap[block];
    if (! llvmBlock)
        llvmBlock = llvm::BasicBlock::Create(getContext(), llvm::Twine(),
                                             _llvmFunction);
    return llvmBlock;
}

llvm::Value *InstructionSelection::getLLVMTempReference(V4IR::Expr *expr)
{
    if (V4IR::Temp *t = expr->asTemp())
        return getLLVMTemp(t);

    assert(!"TODO!");
    llvm::Value *addr = newLLVMTemp(_valueTy);
//    CreateStore(getLLVMValue(expr), addr);
    return addr;
}

llvm::Value *InstructionSelection::getLLVMCondition(V4IR::Expr *expr)
{
    llvm::Value *value = 0;
    if (V4IR::Temp *t = expr->asTemp()) {
        value = getLLVMTemp(t);
    } else {
        assert(!"TODO!");
        Q_UNREACHABLE();

#if 0
        value = getLLVMValue(expr);
        if (! value) {
            Q_UNIMPLEMENTED();
            return getInt1(false);
        }

        llvm::Value *tmp = newLLVMTemp(_valueTy);
        CreateStore(value, tmp);
        value = tmp;
#endif
    }

    return CreateCall2(getRuntimeFunction("__qmljs_llvm_to_boolean"),
                       _llvmFunction->arg_begin(),
                       value);
}

llvm::Value *InstructionSelection::getLLVMTemp(V4IR::Temp *temp)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
    return 0;
#if 0
    if (temp->idx < 0) {
        const int index = -temp->idx -1;
        return CreateCall2(getRuntimeFunction("__qmljs_llvm_get_argument"),
                           _llvmFunction->arg_begin(), getInt32(index));
    }

    return _tempMap[temp->idx];
#endif
}

llvm::Value *InstructionSelection::getStringPtr(const QString &s)
{
    llvm::Value *&value = _stringMap[s];
    if (! value) {
        const QByteArray bytes = s.toUtf8();
        value = CreateGlobalStringPtr(llvm::StringRef(bytes.constData(), bytes.size()));
        _stringMap[s] = value;
    }
    return value;
}

llvm::Value *InstructionSelection::getIdentifier(const QString &s)
{
    llvm::Value *str = getStringPtr(s);
    llvm::Value *id = CreateCall2(getRuntimeFunction("__qmljs_identifier_from_utf8"),
                                  _llvmFunction->arg_begin(), str);
    return id;
}

void InstructionSelection::visitJump(V4IR::Jump *s)
{
    CreateBr(getLLVMBasicBlock(s->target));
}

void InstructionSelection::visitCJump(V4IR::CJump *s)
{
    CreateCondBr(getLLVMCondition(s->cond),
                 getLLVMBasicBlock(s->iftrue),
                 getLLVMBasicBlock(s->iffalse));
}

void InstructionSelection::visitRet(V4IR::Ret *s)
{
    V4IR::Temp *t = s->expr->asTemp();
    assert(t != 0);
    llvm::Value *result = getLLVMTemp(t);
    llvm::Value *ctx = _llvmFunction->arg_begin();
    CreateCall2(getRuntimeFunction("__qmljs_llvm_return"), ctx, result);
    CreateRetVoid();
}

void InstructionSelection::visitTry(V4IR::Try *)
{
    // TODO
    assert(!"TODO!");
    Q_UNREACHABLE();
}

#if 0
void InstructionSelection::visitString(V4IR::String *e)
{
    llvm::Value *tmp = newLLVMTemp(_valueTy);
    CreateCall3(getRuntimeFunction("__qmljs_llvm_init_string"),
                _llvmFunction->arg_begin(), tmp,
                getStringPtr(*e->value));
    _llvmValue = CreateLoad(tmp);
}
#endif

llvm::AllocaInst *InstructionSelection::newLLVMTemp(llvm::Type *type, llvm::Value *size)
{
    llvm::AllocaInst *addr = new llvm::AllocaInst(type, size, llvm::Twine(), _allocaInsertPoint);
    return addr;
}

llvm::Value * InstructionSelection::genArguments(V4IR::ExprList *exprs, int &argc)
{
    llvm::Value *args = 0;

    argc = 0;
    for (V4IR::ExprList *it = exprs; it; it = it->next)
        ++argc;

    if (argc)
        args = newLLVMTemp(_valueTy, getInt32(argc));
    else
        args = llvm::Constant::getNullValue(_valueTy->getPointerTo());

    int i = 0;
    for (V4IR::ExprList *it = exprs; it; it = it->next) {
//        llvm::Value *arg = getLLVMValue(it->expr);
//        CreateStore(arg, CreateConstGEP1_32(args, i++));
    }

    return args;
}

void InstructionSelection::genCallMember(V4IR::Call *e, llvm::Value *result)
{
    if (! result)
        result = newLLVMTemp(_valueTy);

    V4IR::Member *m = e->base->asMember();
    llvm::Value *thisObject = getLLVMTemp(m->base->asTemp());
    llvm::Value *name = getIdentifier(*m->name);

    int argc = 0;
    llvm::Value *args = genArguments(e->args, argc);

    llvm::Value *actuals[] = {
        _llvmFunction->arg_begin(),
        result,
        thisObject,
        name,
        args,
        getInt32(argc)
    };

    CreateCall(getRuntimeFunction("__qmljs_llvm_call_property"), llvm::ArrayRef<llvm::Value *>(actuals));
    _llvmValue = CreateLoad(result);
}

void InstructionSelection::genConstructMember(V4IR::New *e, llvm::Value *result)
{
    if (! result)
        result = newLLVMTemp(_valueTy);

    V4IR::Member *m = e->base->asMember();
    llvm::Value *thisObject = getLLVMTemp(m->base->asTemp());
    llvm::Value *name = getIdentifier(*m->name);

    int argc = 0;
    llvm::Value *args = genArguments(e->args, argc);

    llvm::Value *actuals[] = {
        _llvmFunction->arg_begin(),
        result,
        thisObject,
        name,
        args,
        getInt32(argc)
    };

    CreateCall(getRuntimeFunction("__qmljs_llvm_construct_property"), llvm::ArrayRef<llvm::Value *>(actuals));
    _llvmValue = CreateLoad(result);
}

void InstructionSelection::genCallTemp(V4IR::Call *e, llvm::Value *result)
{
    if (! result)
        result = newLLVMTemp(_valueTy);

    llvm::Value *func = getLLVMTempReference(e->base);

    int argc = 0;
    llvm::Value *args = genArguments(e->args, argc);

    llvm::Value *thisObject = llvm::Constant::getNullValue(_valueTy->getPointerTo());

    llvm::Value *actuals[] = {
        _llvmFunction->arg_begin(),
        result,
        thisObject,
        func,
        args,
        getInt32(argc)
    };

    CreateCall(getRuntimeFunction("__qmljs_llvm_call_value"), actuals);

    _llvmValue = CreateLoad(result);
}

void InstructionSelection::genConstructTemp(V4IR::New *e, llvm::Value *result)
{
    if (! result)
        result = newLLVMTemp(_valueTy);

    llvm::Value *func = getLLVMTempReference(e->base);

    int argc = 0;
    llvm::Value *args = genArguments(e->args, argc);

    llvm::Value *actuals[] = {
        _llvmFunction->arg_begin(),
        result,
        func,
        args,
        getInt32(argc)
    };

    CreateCall(getRuntimeFunction("__qmljs_llvm_construct_value"), actuals);

    _llvmValue = CreateLoad(result);
}

void InstructionSelection::genCallName(V4IR::Call *e, llvm::Value *result)
{
    V4IR::Name *base = e->base->asName();

    if (! result)
        result = newLLVMTemp(_valueTy);

    if (! base->id) {
        switch (base->builtin) {
        case V4IR::Name::builtin_invalid:
            break;

        case V4IR::Name::builtin_typeof:
            CreateCall3(getRuntimeFunction("__qmljs_llvm_typeof"),
                        _llvmFunction->arg_begin(), result, getLLVMTempReference(e->args->expr));
            _llvmValue = CreateLoad(result);
            return;

        case V4IR::Name::builtin_throw:
            CreateCall2(getRuntimeFunction("__qmljs_llvm_throw"),
                        _llvmFunction->arg_begin(), getLLVMTempReference(e->args->expr));
            _llvmValue = llvm::UndefValue::get(_valueTy);
            return;

        case V4IR::Name::builtin_finish_try:
            // ### FIXME.
            return;

        case V4IR::Name::builtin_foreach_iterator_object:
            CreateCall3(getRuntimeFunction("__qmljs_llvm_foreach_iterator_object"),
                        _llvmFunction->arg_begin(), result, getLLVMTempReference(e->args->expr));
            _llvmValue = CreateLoad(result);
            return;

        case V4IR::Name::builtin_foreach_next_property_name:
            CreateCall2(getRuntimeFunction("__qmljs_llvm_foreach_next_property_name"),
                        result, getLLVMTempReference(e->args->expr));
            _llvmValue = CreateLoad(result);
            return;

        case V4IR::Name::builtin_delete: {
            if (V4IR::Subscript *subscript = e->args->expr->asSubscript()) {
                CreateCall4(getRuntimeFunction("__qmljs_llvm_delete_subscript"),
                           _llvmFunction->arg_begin(),
                           result,
                           getLLVMTempReference(subscript->base),
                           getLLVMTempReference(subscript->index));
                _llvmValue = CreateLoad(result);
                return;
            } else if (V4IR::Member *member = e->args->expr->asMember()) {
                CreateCall4(getRuntimeFunction("__qmljs_llvm_delete_member"),
                           _llvmFunction->arg_begin(),
                           result,
                           getLLVMTempReference(member->base),
                           getIdentifier(*member->name));
                _llvmValue = CreateLoad(result);
                return;
            } else if (V4IR::Name *name = e->args->expr->asName()) {
                CreateCall3(getRuntimeFunction("__qmljs_llvm_delete_property"),
                           _llvmFunction->arg_begin(),
                           result,
                           getIdentifier(*name->id));
                _llvmValue = CreateLoad(result);
                return;
            } else {
                CreateCall3(getRuntimeFunction("__qmljs_llvm_delete_value"),
                           _llvmFunction->arg_begin(),
                           result,
                           getLLVMTempReference(e->args->expr));
                _llvmValue = CreateLoad(result);
                return;
            }
        } break;

        default:
            Q_UNREACHABLE();
        }
    } else {
        llvm::Value *name = getIdentifier(*base->id);

        int argc = 0;
        llvm::Value *args = genArguments(e->args, argc);

        CreateCall5(getRuntimeFunction("__qmljs_llvm_call_activation_property"),
                    _llvmFunction->arg_begin(), result, name, args, getInt32(argc));

        _llvmValue = CreateLoad(result);
    }
}

void InstructionSelection::genConstructName(V4IR::New *e, llvm::Value *result)
{
    V4IR::Name *base = e->base->asName();

    if (! result)
        result = newLLVMTemp(_valueTy);

    if (! base->id) {
        Q_UNREACHABLE();
    } else {
        llvm::Value *name = getIdentifier(*base->id);

        int argc = 0;
        llvm::Value *args = genArguments(e->args, argc);

        CreateCall5(getRuntimeFunction("__qmljs_llvm_construct_activation_property"),
                    _llvmFunction->arg_begin(), result, name, args, getInt32(argc));

        _llvmValue = CreateLoad(result);
    }
}

#if 0
void InstructionSelection::visitCall(V4IR::Call *e)
{
    if (e->base->asMember()) {
        genCallMember(e);
    } else if (e->base->asTemp()) {
        genCallTemp(e);
    } else if (e->base->asName()) {
        genCallName(e);
    } else if (V4IR::Temp *t = e->base->asTemp()) {
        llvm::Value *base = getLLVMTemp(t);

        int argc = 0;
        llvm::Value *args = genArguments(e->args, argc);

        llvm::Value *result = newLLVMTemp(_valueTy);
        CreateStore(llvm::Constant::getNullValue(_valueTy), result);
        CreateCall5(getRuntimeFunction("__qmljs_llvm_call_value"),
                    _llvmFunction->arg_begin(), result, base, args, getInt32(argc));
        _llvmValue = CreateLoad(result);
    } else {
        Q_UNIMPLEMENTED();
    }
}
#endif

#if 0
void InstructionSelection::visitNew(V4IR::New *e)
{
    if (e->base->asMember()) {
        genConstructMember(e);
    } else if (e->base->asTemp()) {
        genConstructTemp(e);
    } else if (e->base->asName()) {
        genConstructName(e);
    } else if (V4IR::Temp *t = e->base->asTemp()) {
        llvm::Value *base = getLLVMTemp(t);

        int argc = 0;
        llvm::Value *args = genArguments(e->args, argc);

        llvm::Value *result = newLLVMTemp(_valueTy);
        CreateStore(llvm::Constant::getNullValue(_valueTy), result);
        CreateCall5(getRuntimeFunction("__qmljs_llvm_construct_value"),
                    _llvmFunction->arg_begin(), result, base, args, getInt32(argc));
        _llvmValue = CreateLoad(result);
    } else {
        Q_UNIMPLEMENTED();
    }
}
#endif

#if 0
void InstructionSelection::visitSubscript(V4IR::Subscript *e)
{
    llvm::Value *result = newLLVMTemp(_valueTy);
    llvm::Value *base = getLLVMTempReference(e->base);
    llvm::Value *index = getLLVMTempReference(e->index);
    CreateCall4(getRuntimeFunction("__qmljs_llvm_get_element"),
                _llvmFunction->arg_begin(), result, base, index);
    _llvmValue = CreateLoad(result);
}
#endif

#if 0
void InstructionSelection::visitMember(V4IR::Member *e)
{
    llvm::Value *result = newLLVMTemp(_valueTy);
    llvm::Value *base = getLLVMTempReference(e->base);
    llvm::Value *name = getIdentifier(*e->name);

    CreateCall4(getRuntimeFunction("__qmljs_llvm_get_property"),
                _llvmFunction->arg_begin(), result, base, name);
    _llvmValue = CreateLoad(result);
}
#endif

llvm::Function *InstructionSelection::getRuntimeFunction(llvm::StringRef str)
{
    llvm::Function *func = _llvmModule->getFunction(str);
    if (!func) {
        std::cerr << "Cannot find runtime function \""
                  << str.str() << "\"!" << std::endl;
        assert(func);
    }
    return func;
}

llvm::Value *InstructionSelection::createValue(V4IR::Const *e)
{
    llvm::Value *tmp = newLLVMTemp(_valueTy);

    switch (e->type) {
    case V4IR::UndefinedType:
        CreateCall(getRuntimeFunction("__qmljs_llvm_init_undefined"), tmp);
        break;

    case V4IR::NullType:
        CreateCall(getRuntimeFunction("__qmljs_llvm_init_null"), tmp);
        break;

    case V4IR::BoolType:
        CreateCall2(getRuntimeFunction("__qmljs_llvm_init_boolean"), tmp,
                    getInt1(e->value ? 1 : 0));
        break;

    case V4IR::NumberType:
        CreateCall2(getRuntimeFunction("__qmljs_llvm_init_number"), tmp,
                    llvm::ConstantFP::get(_numberTy, e->value));
        break;

    default:
        Q_UNREACHABLE();
    }

    return tmp;
}

llvm::Value *InstructionSelection::toValuePtr(V4IR::Expr *e)
{
    if (V4IR::Temp *t = e->asTemp()) {
        return getLLVMTemp(t);
    } else if (V4IR::Const *c = e->asConst()) {
        return createValue(c);
    } else {
        Q_UNREACHABLE();
    }
}

llvm::Value *InstructionSelection::genStringList(const QList<const QString *> &strings, const char *arrayName, const char *elementName)
{
    llvm::Value *array = CreateAlloca(_stringPtrTy, getInt32(strings.size()),
                                      arrayName);
    for (int i = 0, ei = strings.size(); i < ei; ++i) {
        llvm::Value *el;
        if (const QString *string = strings.at(i))
            el = getIdentifier(*string);
        else
            el = llvm::Constant::getNullValue(_stringPtrTy);
        llvm::Value *ptr = CreateGEP(array, getInt32(i), elementName);
        CreateStore(el, ptr);
    }

    return array;
}