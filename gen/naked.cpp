#include "gen/llvm.h"

#include "expression.h"
#include "statement.h"
#include "declaration.h"

#include <cassert>

#include "gen/logger.h"
#include "gen/irstate.h"
#include "gen/llvmhelpers.h"
#include "gen/tollvm.h"

//////////////////////////////////////////////////////////////////////////////////////////

void Statement::toNakedIR(IRState *p)
{
    error("not allowed in naked function");
}

//////////////////////////////////////////////////////////////////////////////////////////

void CompoundStatement::toNakedIR(IRState *p)
{
    Logger::println("CompoundStatement::toNakedIR(): %s", loc.toChars());
    LOG_SCOPE;

    if (statements)
    for (unsigned i = 0; i < statements->dim; i++)
    {
        Statement* s = (Statement*)statements->data[i];
        if (s) s->toNakedIR(p);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

void ExpStatement::toNakedIR(IRState *p)
{
    Logger::println("ExpStatement::toNakedIR(): %s", loc.toChars());
    LOG_SCOPE;

    // only expstmt supported in declarations
    if (exp->op != TOKdeclaration)
    {
        Statement::toNakedIR(p);
        return;
    }

    DeclarationExp* d = (DeclarationExp*)exp;
    VarDeclaration* vd = d->declaration->isVarDeclaration();
    FuncDeclaration* fd = d->declaration->isFuncDeclaration();
    EnumDeclaration* ed = d->declaration->isEnumDeclaration();

    // and only static variable/function declaration
    // no locals or nested stuffies!
    if (!vd && !fd && !ed)
    {
        Statement::toNakedIR(p);
        return;
    }
    else if (vd && !vd->isDataseg())
    {
        error("non-static variable '%s' not allowed in naked function", vd->toChars());
        return;
    }
    else if (fd && !fd->isStatic())
    {
        error("non-static nested function '%s' not allowed in naked function", fd->toChars());
        return;
    }
    // enum decls should always be safe

    // make sure the symbols gets processed
    d->declaration->toObjFile(0);
}

//////////////////////////////////////////////////////////////////////////////////////////

void LabelStatement::toNakedIR(IRState *p)
{
    Logger::println("LabelStatement::toNakedIR(): %s", loc.toChars());
    LOG_SCOPE;

    p->nakedAsm << p->func()->decl->mangle() << "_" << ident->toChars() << ":";

    if (statement)
        statement->toNakedIR(p);
}

//////////////////////////////////////////////////////////////////////////////////////////

void DtoDefineNakedFunction(FuncDeclaration* fd)
{
    Logger::println("DtoDefineNakedFunction(%s)", fd->mangle());
    LOG_SCOPE;

    assert(fd->ir.irFunc);
    gIR->functions.push_back(fd->ir.irFunc);

    // we need to do special processing on the body, since we only want
    // to allow actual inline asm blocks to reach the final asm output

    std::ostringstream& asmstr = gIR->nakedAsm;

    // build function header

    // FIXME: could we perhaps use llvm asmwriter to give us these details ?

    const char* mangle = fd->mangle();
    std::ostringstream tmpstr;

    // osx is different
    // also mangling has an extra underscore prefixed
    if (global.params.os == OSMacOSX)
    {
        std::string section = "text";
        bool weak = false;
        if (DtoIsTemplateInstance(fd))
        {
            tmpstr << "section\t__TEXT,__textcoal_nt,coalesced,pure_instructions";
            section = tmpstr.str();
            weak = true;
        }
        asmstr << "\t." << section << std::endl;
        asmstr << "\t.align\t4,0x90" << std::endl;
        asmstr << "\t.globl\t_" << mangle << std::endl;
        if (weak)
        {
            asmstr << "\t.weak_definition\t_" << mangle << std::endl;
        }
        asmstr << "_" << mangle << ":" << std::endl;
    }
    // this works on linux x86 32 and 64 bit
    // assume it works everywhere else as well for now
    else
    {
        const char* linkage = "globl";
        std::string section = "text";
        if (DtoIsTemplateInstance(fd))
        {
            linkage = "weak";
            tmpstr << "section\t.gnu.linkonce.t." << mangle << ",\"ax\",@progbits";
            section = tmpstr.str();
        }
        asmstr << "\t." << section << std::endl;
        asmstr << "\t.align\t16" << std::endl;
        asmstr << "\t." << linkage << "\t" << mangle << std::endl;
        asmstr << "\t.type\t" << mangle << ",@function" << std::endl;
        asmstr << mangle << ":" << std::endl;
    }

    // emit body
    fd->fbody->toNakedIR(gIR);

    // emit size after body
    // llvm does this on linux, but not on osx
    if (global.params.os != OSMacOSX)
    {
        asmstr << "\t.size\t" << mangle << ", .-" << mangle << std::endl << std::endl;
    }

    gIR->module->appendModuleInlineAsm(asmstr.str());
    asmstr.str("");

    gIR->functions.pop_back();
}

//////////////////////////////////////////////////////////////////////////////////////////

void emitABIReturnAsmStmt(IRAsmBlock* asmblock, Loc loc, FuncDeclaration* fdecl)
{
    Logger::println("emitABIReturnAsmStmt(%s)", fdecl->mangle());
    LOG_SCOPE;

    IRAsmStmt* as = new IRAsmStmt;

    const LLType* llretTy = DtoType(fdecl->type->nextOf());
    asmblock->retty = llretTy;
    asmblock->retn = 1;

    // x86 or x86_64
    if (global.params.cpu == ARCHx86 || global.params.cpu == ARCHx86_64)
    {
        LINK l = fdecl->linkage;
        assert((l == LINKd || l == LINKc || l == LINKwindows) && "invalid linkage for asm implicit return");

        Type* rt = fdecl->type->nextOf()->toBasetype();
        if (rt->isintegral() || rt->ty == Tpointer || rt->ty == Tclass || rt->ty == Taarray)
        {
            if (rt->size() == 8) {
                as->out_c = "=A,";
            } else {
                as->out_c = "={ax},";
            }
        }
        else if (rt->isfloating())
        {
            if (rt->iscomplex()) {
                as->out_c = "={st},={st(1)},";
                asmblock->retn = 2;
            } else {
                as->out_c = "={st},";
            }
        }
        else if (rt->ty == Tarray || rt->ty == Tdelegate)
        {
            as->out_c = "={ax},={dx},";
            asmblock->retn = 2;
        }
        else
        {
            error(loc, "unimplemented return type '%s' for implicit abi return", rt->toChars());
            fatal();
        }
    }

    // unsupported
    else
    {
        error(loc, "this target (%s) does not implement inline asm falling off the end of the function", global.params.targetTriple);
        fatal();
    }

    // return values always go in the front
    asmblock->s.push_front(as);
}
