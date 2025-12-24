




#ifndef ASMJIT_ARM_A64ASSEMBLER_H_INCLUDED
#define ASMJIT_ARM_A64ASSEMBLER_H_INCLUDED

#include "../core/assembler.h"
#include "../arm/a64emitter.h"
#include "../arm/a64operand.h"

ASMJIT_BEGIN_SUB_NAMESPACE(a64)





class ASMJIT_VIRTAPI Assembler
  : public BaseAssembler,
    public EmitterExplicitT<Assembler> {

public:
  using Base = BaseAssembler;




  ASMJIT_API Assembler(CodeHolder* code = nullptr) noexcept;
  ASMJIT_API ~Assembler() noexcept override;






  ASMJIT_API Error _emit(InstId inst_id, const Operand_& o0, const Operand_& o1, const Operand_& o2, const Operand_* op_ext) override;






  ASMJIT_API Error align(AlignMode align_mode, uint32_t alignment) override;






  ASMJIT_API Error on_attach(CodeHolder& code) noexcept override;
  ASMJIT_API Error on_detach(CodeHolder& code) noexcept override;


};



ASMJIT_END_SUB_NAMESPACE

#endif
