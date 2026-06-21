#include <libdcc/tacky.hpp>

namespace tacky {


    TackyVal TackyGen::emit_tacky(dcc::Expr * expr) {
    if (expr->getExprType() == dcc::ExprType::CONSTANT) {
        return TackyConstant{static_cast<dcc::Constant*>(expr)->_value};
    } else if (expr->getExprType() == dcc::ExprType::UNARY) {
        auto unary = static_cast<dcc::UnaryOperator*>(expr);
        auto src = emit_tacky(unary->get_inner_expr());
        auto dst_name = make_temporary_name();
        auto dst = TackyVar{dst_name};
        auto tacky_op = unary->getUnaryOp();

        TackyUnaryOp unary_op = convert_unop(unary->getUnaryOp());
        _instructions.push_back(std::make_unique<TackyUnary>(unary_op, src, dst));
        return dst;
    } else if (expr->getExprType() == dcc::ExprType::BINARY) {
        auto binary = static_cast<dcc::BinaryOperator*>(expr);
        auto src1 = emit_tacky(binary->getLeftExpr());
        auto src2 = emit_tacky(binary->getRightExpr());
        auto dst_name = make_temporary_name();
        auto dst = TackyVar{dst_name};
        auto tacky_op = binary->getBinOp();

        TackyBinaryOp binary_op = convert_binop(binary->getBinOp());
        _instructions.push_back(std::make_unique<TackyBinary>(binary_op, src1, src2, dst));
        return dst;
    } 
    return std::monostate {};
}
}