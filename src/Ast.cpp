#include "Ast.h"
#include "SymbolTable.h"
#include "Unit.h"
#include "Instruction.h"
#include "IRBuilder.h"
#include <string>
#include "Type.h"

extern FILE *yyout;
int Node::counter = 0;
IRBuilder* Node::builder = nullptr;
int yyget_lineno(void);

Node::Node()
{
    seq = counter++;
    lineno = yyget_lineno();
}

void Node::backPatch(std::vector<Instruction*> &list, BasicBlock*bb)
{
    for(auto &inst:list)
    {
        if(inst->isCond())
            dynamic_cast<CondBrInstruction*>(inst)->setTrueBranch(bb);
        else if(inst->isUncond())
            dynamic_cast<UncondBrInstruction*>(inst)->setBranch(bb);
    }
}

std::vector<Instruction*> Node::merge(std::vector<Instruction*> &list1, std::vector<Instruction*> &list2)
{
    std::vector<Instruction*> res(list1);
    res.insert(res.end(), list2.begin(), list2.end());
    return res;
}

void Ast::genCode(Unit *unit)
{
    IRBuilder *builder = new IRBuilder(unit);
    Node::setIRBuilder(builder);
    root->genCode();
}

void FunctionDef::genCode()
{
    Unit *unit = builder->getUnit();
    Function *func = new Function(unit, se);
    BasicBlock *entry = func->getEntry();
    // set the insert point to the entry basicblock of this function.
    builder->setInsertBB(entry);

    stmt->genCode();

    /**
     * Construct control flow graph. You need do set successors and predecessors for each basic block.
     * Todo
    */
   
}

void BinaryExpr::genCode()
{
    BasicBlock *bb = builder->getInsertBB();
    Function *func = bb->getParent();
    if (op == AND)
    {
        BasicBlock *trueBB = new BasicBlock(func);  // if the result of lhs is true, jump to the trueBB.
        expr1->genCode();
        backPatch(expr1->trueList(), trueBB);
        builder->setInsertBB(trueBB);               // set the insert point to the trueBB so that intructions generated by expr2 will be inserted into it.
        expr2->genCode();
        true_list = expr2->trueList();
        false_list = merge(expr1->falseList(), expr2->falseList());
    }
    else if(op == OR)
    {
        // Todo
    }
    else if(op >= LESS && op <= GREATER)
    {
        // Todo
    }
    else if(op >= ADD && op <= SUB)
    {
        expr1->genCode();
        expr2->genCode();
        Operand *src1 = expr1->getOperand();
        Operand *src2 = expr2->getOperand();
        int opcode;
        switch (op)
        {
        case ADD:
            opcode = BinaryInstruction::ADD;
            break;
        case SUB:
            opcode = BinaryInstruction::SUB;
            break;
        }
        new BinaryInstruction(opcode, dst, src1, src2, bb);
    }
}

void UnaryExpr::genCode()
{
    // Todo
}

void CallExpr::genCode()
{
    // Todo
}

void Constant::genCode()
{
    // we don't need to generate code.
}

void Id::genCode()
{
    BasicBlock *bb = builder->getInsertBB();
    Operand *addr = dynamic_cast<IdentifierSymbolEntry*>(symbolEntry)->getAddr();
    new LoadInstruction(dst, addr, bb);
}

void IfStmt::genCode()
{
    Function *func;
    BasicBlock *then_bb, *end_bb;

    func = builder->getInsertBB()->getParent();
    then_bb = new BasicBlock(func);
    end_bb = new BasicBlock(func);

    cond->genCode();
    backPatch(cond->trueList(), then_bb);
    backPatch(cond->falseList(), end_bb);

    builder->setInsertBB(then_bb);
    thenStmt->genCode();
    then_bb = builder->getInsertBB();
    new UncondBrInstruction(end_bb, then_bb);

    builder->setInsertBB(end_bb);
}

void IfElseStmt::genCode()
{
    // Todo
}

void CompoundStmt::genCode()
{
    // Todo
}

void SeqNode::genCode()
{
    // Todo
}

void DeclStmt::genCode()
{
    IdentifierSymbolEntry *se = dynamic_cast<IdentifierSymbolEntry *>(id->getSymPtr());
    if(se->isGlobal())
    {
        Operand *addr;
        SymbolEntry *addr_se;
        addr_se = new IdentifierSymbolEntry(*se);
        addr_se->setType(new PointerType(se->getType()));
        addr = new Operand(addr_se);
        se->setAddr(addr);
    }
    else if(se->isLocal())
    {
        Function *func = builder->getInsertBB()->getParent();
        BasicBlock *entry = func->getEntry();
        Instruction *alloca;
        Operand *addr;
        SymbolEntry *addr_se;
        Type *type;
        type = new PointerType(se->getType());
        addr_se = new TemporarySymbolEntry(type, SymbolTable::getLabel());
        addr = new Operand(addr_se);
        alloca = new AllocaInstruction(addr, se);                   // allocate space for local id in function stack.
        entry->insertFront(alloca);                                 // allocate instructions should be inserted into the begin of the entry block.
        se->setAddr(addr);                                          // set the addr operand in symbol entry so that we can use it in subsequent code generation.
    }
}

void ReturnStmt::genCode()
{
    // Todo
}

void AssignStmt::genCode()
{
    BasicBlock *bb = builder->getInsertBB();
    expr->genCode();
    Operand *addr = dynamic_cast<IdentifierSymbolEntry*>(lval->getSymPtr())->getAddr();
    Operand *src = expr->getOperand();
    /***
     * We haven't implemented array yet, the lval can only be ID. So we just store the result of the `expr` to the addr of the id.
     * If you want to implement array, you have to caculate the address first and then store the result into it.
     */
    new StoreInstruction(addr, src, bb);
}

void ExprStmt::genCode()
{
    expr->genCode();
}

void WhileStmt::genCode()
{
    // Todo
}

void ConvExpr::genCode()
{
    // Todo
}

Type* Ast::typeCheck(Type* retType)
{
    if(root != nullptr)
        return root->typeCheck();
    return nullptr;
}

Type* FunctionDef::typeCheck(Type* retType)
{
    Type* _retType = ((FunctionType *)(se->getType()))->getRetType();
    int beg_ret_cnt = ReturnStmt::getCheckCnt();
    stmt->typeCheck(_retType);
    int end_ret_cnt = ReturnStmt::getCheckCnt(); // 计算return语句的个数
    if(beg_ret_cnt == end_ret_cnt && _retType != TypeSystem::voidType)
    {
        BEG_RED;
        fprintf(stderr, "line:%d %s should return a value of type %s\n", 
            lineno, se->toStr().c_str(), _retType->toStr().c_str());
        END_COLOR;
    }
    return nullptr;
}

Type* BinaryExpr::typeCheck(Type* retType)
{
    Type* t1 = expr1->typeCheck(retType);
    Type* t2 = expr2->typeCheck(retType);
    if(t1 == nullptr || t2 == nullptr)
        return TypeSystem::errorType;
    if(t1 != t2)
    {
        BEG_RED;
        fprintf(stderr, "line:%d binary op - type mismatched between %s and %s\n", 
            lineno, t1->toStr().c_str(), t2->toStr().c_str());
        END_COLOR;
        return TypeSystem::errorType;
    }
    return t1;
}

Type* UnaryExpr::typeCheck(Type* retType)
{
    return expr->typeCheck(retType);
}

Type* CallExpr::typeCheck(Type* retType)
{
    if(symbolEntry->getType() == TypeSystem::errorType)
        return TypeSystem::errorType;
    // 读取vector<ExprNode*> params中的内容
    std::vector<ExprNode*> params_vec = params->params; // 实参
    // 找到函数的参数列表
    std::vector<Type*> formal_type; // 形参
    
    if(((IdentifierSymbolEntry *)symbolEntry)->reverse_func == nullptr)
    {
        fprintf(stderr, "[DEBUG] line:%d function call %s - function se has no reverse\n", 
            lineno, symbolEntry->toStr().c_str());
        return TypeSystem::errorType;
    }
    else
    { // 存在多个函数，遍历其形参表，找到匹配的函数
        FunctionDef *now = ((IdentifierSymbolEntry *)symbolEntry)->reverse_func;
        bool flag = false;
        while(now != nullptr)
        {
            formal_type = now->getFormalTypes();
            if(params_vec.size() == formal_type.size())
            { // 参数个数相同，检查类型是否匹配
                flag = true;
                for(int i = 0; i < (int)(params_vec.size()); i++)
                { // 检查每个参数的类型 
                    Type *real_type = params_vec[i]->typeCheck(retType);
                    if(real_type != formal_type[i])
                    {
                        flag = false;
                        break;
                    }
                }
                if(flag)
                { // 找到匹配的函数
                    symbolEntry = now->getSymPtr();
                    BEG_BLUE;
                    fprintf(stderr, "line:%d function call %s - matched type %s\n", 
                        lineno, symbolEntry->toStr().c_str() , symbolEntry->getType()->toStr().c_str());
                    END_COLOR;
                    break;
                }
            }
            now = now->next;
        }
        if(!flag)
        { // 没有找到匹配的函数
            BEG_RED;
            fprintf(stderr, "line:%d function call %s - no matching function\n", 
                lineno, symbolEntry->toStr().c_str());
            END_COLOR;
            return TypeSystem::errorType;
        }
    }
    return ((FunctionType *)(symbolEntry->getType()))->getRetType();
}

Type* Constant::typeCheck(Type* retType)
{
    return symbolEntry->getType();
}

Type* Id::typeCheck(Type* retType)
{
    return symbolEntry->getType();
}

Type * ConvExpr::typeCheck(Type* retType)
{
    return type;
}

Type* IfStmt::typeCheck(Type* retType)
{
    Type *condType = cond->typeCheck();
    if(condType != TypeSystem::boolType)
    {
        cond = new ConvExpr(cond->getSymPtr(), TypeSystem::boolType, cond);
        BEG_BLUE;
        fprintf(stderr, "line:%d if - type %s mismatched, convert to bool\n", 
            lineno, condType->toStr().c_str());
        END_COLOR;
    }
    thenStmt->typeCheck(retType);
    return nullptr;
}

Type* IfElseStmt::typeCheck(Type* retType)
{
    Type *condType = cond->typeCheck();
    if(condType != TypeSystem::boolType)
    {
        cond = new ConvExpr(cond->getSymPtr(), TypeSystem::boolType, cond);
        BEG_BLUE;
        fprintf(stderr, "line:%d ifelse - type %s mismatched, convert to bool\n", 
            lineno, condType->toStr().c_str());
        END_COLOR;
    }
    thenStmt->typeCheck(retType);
    elseStmt->typeCheck(retType);
    return nullptr;
}

Type* CompoundStmt::typeCheck(Type* retType)
{
    stmt->typeCheck(retType);
    return nullptr;
}

Type* SeqNode::typeCheck(Type* retType)
{
    stmt1->typeCheck(retType);
    stmt2->typeCheck(retType);
    return nullptr;
}

Type* DeclStmt::typeCheck(Type* retType)
{
    if(expr == nullptr)
        return nullptr;
    Type* rType = expr->typeCheck(retType);
    Type* lType = id->typeCheck(retType);
    // printf("rType:%s, lType:%s", rType->toStr().c_str(), lType->toStr().c_str());
    if(rType != lType && lType != TypeSystem::errorType && rType != TypeSystem::errorType)
    {
        BEG_RED;
        fprintf(stderr, "line:%d declare - type mismatched between %s and %s\n", 
            lineno, lType->toStr().c_str(), rType->toStr().c_str());
        END_COLOR;
    }
    return nullptr;
}

int ReturnStmt::check_cnt = 0; // 类型检查计数器

Type* ReturnStmt::typeCheck(Type* retType)
{
    check_cnt++;
    Type *_retType; // 返回值类型
    if(retValue == nullptr)
        _retType = TypeSystem::voidType;
    else
        _retType = retValue->getSymPtr()->getType(); 
    CallExpr *callExpr = dynamic_cast<CallExpr *>(retValue); // 判断表达式是否为函数调用
    if(callExpr == nullptr && _retType != retType && retType != nullptr)
    {
        BEG_RED;
        fprintf(stderr, "line:%d ret type error, should %s, but %s\n", 
            lineno, retType->toStr().c_str(), _retType->toStr().c_str());
        END_COLOR;
    }
    else if(callExpr != nullptr && retType != nullptr &&
        ((FunctionType *)(callExpr->getSymPtr()->getType()))->getRetType() != retType)
    {
        BEG_RED;
        fprintf(stderr, "line:%d ret type error, should %s, but %s (CallExpr)\n", 
            lineno, retType->toStr().c_str(), callExpr->getSymPtr()->getType()->toStr().c_str());
        END_COLOR;
    }
    if(retValue != nullptr)
        retValue->typeCheck(retType);
    return nullptr;
}

Type* AssignStmt::typeCheck(Type* retType)
{
    Type* lType = lval->typeCheck(retType);
    Type* rType = expr->typeCheck(retType);
    if(lType != rType && lType != TypeSystem::errorType && rType != TypeSystem::errorType)
    {
        BEG_RED;
        fprintf(stderr, "line:%d assign - type mismatched between %s and %s\n", 
            lineno, lType->toStr().c_str(), rType->toStr().c_str());
        END_COLOR;
    }
    return nullptr;
}

Type* ExprStmt::typeCheck(Type* retType)
{
    return nullptr;
}

Type* WhileStmt::typeCheck(Type* retType)
{
    Type *condType = cond->typeCheck();
    if(condType != TypeSystem::boolType)
    {
        cond = new ConvExpr(cond->getSymPtr(), TypeSystem::boolType, cond);
        BEG_BLUE;
        fprintf(stderr, "line:%d while - type %s mismatched, convert to bool\n", 
            lineno, condType->toStr().c_str());
        END_COLOR;
    }
    body->typeCheck(retType);
    return nullptr;
}

void BinaryExpr::output(int level)
{
    std::string op_str;
    // ADD, SUB, AND, MUL, DIV, OR, LESS, GREATER, EQ, NEQ, LEQ, GEQ, LE, GE
    switch(op)
    {
        case ADD:
            op_str = "add";
            break;
        case SUB:
            op_str = "sub";
            break;
        case MUL:
            op_str = "mul";
            break;
        case DIV:
            op_str = "div";
            break;
        case AND:
            op_str = "and";
            break;
        case OR:
            op_str = "or";
            break;
        case LESS:
            op_str = "less";
            break;
        case GREATER:
            op_str = "greater";
            break;
        case EQ:
            op_str = "equal";
            break;
        case NEQ:
            op_str = "not_equal";
            break;
        case LEQ:
            op_str = "less_equal";
            break;
        case GEQ:
            op_str = "greater_equal";
            break;
    }
    fprintf(yyout, "%*cBinaryExpr\top: %s\n", level, ' ', op_str.c_str());
    expr1->output(level + 4);
    expr2->output(level + 4);
}

void UnaryExpr::output(int level)
{
    std::string op_str;
    switch(op)
    {
        case NOT:
            op_str = "not";
            break;
        case SUB:
            op_str = "sub";
            break;
    }
    fprintf(yyout, "%*cUnaryExpr\top: %s\n", level, ' ', op_str.c_str());
    expr->output(level + 4);
}

void ConvExpr::output(int level)
{
    fprintf(yyout, "%*cConvExpr\ttype: %s\n", level, ' ', type->toStr().c_str());
    expr->output(level + 4);
}

void Ast::output()
{
    fprintf(yyout, "program\n");
    if(root != nullptr)
        root->output(4);
}

void Constant::output(int level)
{
    std::string type, value;
    type = symbolEntry->getType()->toStr();
    value = symbolEntry->toStr();
    fprintf(yyout, "%*cIntegerLiteral\tvalue: %s\ttype: %s\n", level, ' ',
            value.c_str(), type.c_str());
}

void Id::output(int level)
{
    std::string name, type;
    int scope;
    name = symbolEntry->toStr();
    type = symbolEntry->getType()->toStr();
    scope = dynamic_cast<IdentifierSymbolEntry*>(symbolEntry)->getScope();
    fprintf(yyout, "%*cId\tname: %s\tscope: %d\ttype: %s\tpointer_entry: %p\n", level, ' ',
            name.c_str(), scope, type.c_str(), symbolEntry);
}

void CompoundStmt::output(int level)
{
    fprintf(yyout, "%*cCompoundStmt\n", level, ' ');
    if(stmt != nullptr)
        stmt->output(level + 4);
}

void SeqNode::output(int level)
{
    // fprintf(yyout, "%*cSequence\n", level, ' ');
    stmt1->output(level);
    stmt2->output(level);
}

void DeclStmt::output(int level)
{
    fprintf(yyout, "%*cDeclStmt\n", level, ' ');
    id->output(level + 4);
    if(expr != nullptr)
        expr->output(level + 4);
}

void IfStmt::output(int level)
{
    fprintf(yyout, "%*cIfStmt\n", level, ' ');
    cond->output(level + 4);
    thenStmt->output(level + 4);
}

void WhileStmt::output(int level)
{
    fprintf(yyout, "%*cWhileStmt\n", level, ' ');
    cond->output(level + 4);
    body->output(level + 4);
}

void IfElseStmt::output(int level)
{
    fprintf(yyout, "%*cIfElseStmt\n", level, ' ');
    cond->output(level + 4);
    thenStmt->output(level + 4);
    elseStmt->output(level + 4);
}

void ReturnStmt::output(int level)
{
    fprintf(yyout, "%*cReturnStmt\n", level, ' ');
    if(retValue != nullptr)
        retValue->output(level + 4);
}

void AssignStmt::output(int level)
{
    fprintf(yyout, "%*cAssignStmt\n", level, ' ');
    if(lval!=nullptr)
        lval->output(level + 4);
    expr->output(level + 4);
}

void ExprStmt::output(int level)
{
    fprintf(yyout, "%*cExprStmt\n", level, ' ');
    expr->output(level + 4);
}

void FunctionDef::output(int level)
{
    std::string name, type;
    name = se->toStr();
    type = se->getType()->toStr();
    fprintf(yyout, "%*cFunctionDefine function name: %s, type: %s\n", level, ' ', 
            name.c_str(), type.c_str());
    params->output(level + 4);
    if(stmt!=nullptr)
        stmt->output(level + 4);
}

void CallExpr::output(int level)
{
    std::string name, type;
    name = symbolEntry->toStr();
    type = symbolEntry->getType()->toStr();
    fprintf(yyout, "%*cCallExpr function name: %s, type: %s\n", level, ' ', 
            name.c_str(), type.c_str());
    params->output(level + 4);
}

void CallParams::append(ExprNode* expr)
{
    params.push_back(expr);
}

void CallParams::output(int level)
{
    fprintf(yyout, "%*cCallParams params_n: %d\n", level, ' ', (int)params.size());
    for(auto it: params)
        it->output(level + 4);
}

void FuncParams::append(Type* type, DeclStmt* decl)
{
    types.push_back(type);
    decls.push_back(decl);
}

void FuncParams::output(int level)
{
    fprintf(yyout, "%*cFuncParams params_n: %d\n", level, ' ', (int)decls.size());
    for(auto it: decls)
        it->output(level + 4);
}