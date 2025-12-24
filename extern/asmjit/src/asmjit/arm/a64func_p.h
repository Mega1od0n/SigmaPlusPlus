




#ifndef ASMJIT_ARM_A64FUNC_P_H_INCLUDED
#define ASMJIT_ARM_A64FUNC_P_H_INCLUDED

#include "../core/func.h"

ASMJIT_BEGIN_SUB_NAMESPACE(a64)






namespace FuncInternal {


Error init_call_conv(CallConv& cc, CallConvId call_conv_id, const Environment& environment) noexcept;


Error init_func_detail(FuncDetail& func, const FuncSignature& signature) noexcept;

}




ASMJIT_END_SUB_NAMESPACE

#endif
