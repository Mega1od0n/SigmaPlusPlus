




#include "../core/api-build_p.h"
#if !defined(ASMJIT_NO_AARCH64) && !defined(ASMJIT_NO_COMPILER)

#include "../arm/a64assembler.h"
#include "../arm/a64compiler.h"
#include "../arm/a64emithelper_p.h"
#include "../arm/a64rapass_p.h"

ASMJIT_BEGIN_SUB_NAMESPACE(a64)




Compiler::Compiler(CodeHolder* code) noexcept : BaseCompiler() {
  _arch_mask = uint64_t(1) << uint32_t(Arch::kAArch64);
  init_emitter_funcs(this);

  if (code) {
    code->attach(this);
  }
}
Compiler::~Compiler() noexcept {}




Error Compiler::on_attach(CodeHolder& code) noexcept {
  ASMJIT_PROPAGATE(Base::on_attach(code));
  Error err = add_pass<ARMRAPass>();

  if (ASMJIT_UNLIKELY(err != Error::kOk)) {
    on_detach(code);
    return err;
  }

  _instruction_alignment = uint8_t(4);
  update_emitter_funcs(this);

  return Error::kOk;
}

Error Compiler::on_detach(CodeHolder& code) noexcept {
  return Base::on_detach(code);
}

Error Compiler::on_reinit(CodeHolder& code) noexcept {
  Error err = Base::on_reinit(code);
  if (err == Error::kOk) {
    err = add_pass<ARMRAPass>();
  }
  return err;
}




Error Compiler::finalize() {
  ASMJIT_PROPAGATE(run_passes());
  Assembler a(_code);
  a.add_encoding_options(encoding_options());
  a.add_diagnostic_options(diagnostic_options());
  return serialize_to(&a);
}

ASMJIT_END_SUB_NAMESPACE

#endif
