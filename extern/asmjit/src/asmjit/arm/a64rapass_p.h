




#ifndef ASMJIT_ARM_A64RAPASS_P_H_INCLUDED
#define ASMJIT_ARM_A64RAPASS_P_H_INCLUDED

#include "../core/api-config.h"
#ifndef ASMJIT_NO_COMPILER

#include "../core/compiler.h"
#include "../core/racfgblock_p.h"
#include "../core/racfgbuilder_p.h"
#include "../core/rapass_p.h"
#include "../arm/a64assembler.h"
#include "../arm/a64compiler.h"
#include "../arm/a64emithelper_p.h"

ASMJIT_BEGIN_SUB_NAMESPACE(a64)









class ARMRAPass : public BaseRAPass {
public:
  ASMJIT_NONCOPYABLE(ARMRAPass)
  using Base = BaseRAPass;




  EmitHelper _emit_helper;






  ARMRAPass(BaseCompiler& cc) noexcept;
  ~ARMRAPass() noexcept override;







  [[nodiscard]]
  ASMJIT_INLINE_NODEBUG Compiler& cc() const noexcept { return static_cast<Compiler&>(_cb); }


  [[nodiscard]]
  ASMJIT_INLINE_NODEBUG EmitHelper* emit_helper() noexcept { return &_emit_helper; }






  void on_init() noexcept override;
  void on_done() noexcept override;






  Error build_cfg_nodes() noexcept override;






  Error rewrite() noexcept override;






  Error update_stack_frame() noexcept override;






  Error emit_move(RAWorkReg* work_reg, uint32_t dst_phys_id, uint32_t src_phys_id) noexcept override;
  Error emit_swap(RAWorkReg* a_reg, uint32_t a_phys_id, RAWorkReg* b_reg, uint32_t b_phys_id) noexcept override;

  Error emit_load(RAWorkReg* work_reg, uint32_t dst_phys_id) noexcept override;
  Error emit_save(RAWorkReg* work_reg, uint32_t src_phys_id) noexcept override;

  Error emit_jump(const Label& label) noexcept override;
  Error emit_pre_call(InvokeNode* invoke_node) noexcept override;


};




ASMJIT_END_SUB_NAMESPACE

#endif
#endif
