




#ifndef ASMJIT_ARM_A64COMPILER_H_INCLUDED
#define ASMJIT_ARM_A64COMPILER_H_INCLUDED

#include "../core/api-config.h"
#ifndef ASMJIT_NO_COMPILER

#include "../core/compiler.h"
#include "../core/type.h"
#include "../arm/a64emitter.h"

ASMJIT_BEGIN_SUB_NAMESPACE(a64)





class ASMJIT_VIRTAPI Compiler
  : public BaseCompiler,
    public EmitterExplicitT<Compiler> {
public:
  ASMJIT_NONCOPYABLE(Compiler)
  using Base = BaseCompiler;




  ASMJIT_API explicit Compiler(CodeHolder* code = nullptr) noexcept;
  ASMJIT_API ~Compiler() noexcept override;










  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_gp(TypeId type_id, Args&&... args) { return new_reg<Gp>(type_id, std::forward<Args>(args)...); }





  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Vec new_vec(TypeId type_id, Args&&... args) { return new_reg<Vec>(type_id, std::forward<Args>(args)...); }


  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_gp32(Args&&... args) { return new_reg<Gp>(TypeId::kUInt32, std::forward<Args>(args)...); }


  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_gp64(Args&&... args) { return new_reg<Gp>(TypeId::kUInt64, std::forward<Args>(args)...); }




  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_gpw(Args&&... args) { return new_reg<Gp>(TypeId::kUIntPtr, std::forward<Args>(args)...); }




  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_gpx(Args&&... args) { return new_reg<Gp>(TypeId::kUIntPtr, std::forward<Args>(args)...); }




  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_gpz(Args&&... args) { return new_reg<Gp>(TypeId::kUIntPtr, std::forward<Args>(args)...); }




  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_gp_ptr(Args&&... args) { return new_reg<Gp>(TypeId::kUIntPtr, std::forward<Args>(args)...); }


  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Vec new_vec128(Args&&... args) { return new_reg<Vec>(TypeId::kInt32x4, std::forward<Args>(args)...); }


  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Vec new_vec128_f32x1(Args&&... args) { return new_reg<Vec>(TypeId::kFloat32x1, std::forward<Args>(args)...); }


  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Vec new_vec128_f64x1(Args&&... args) { return new_reg<Vec>(TypeId::kFloat64x1, std::forward<Args>(args)...); }


  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Vec new_vec128_f32x4(Args&&... args) { return new_reg<Vec>(TypeId::kFloat32x4, std::forward<Args>(args)...); }


  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Vec new_vec128_f64x2(Args&&... args) { return new_reg<Vec>(TypeId::kFloat64x2, std::forward<Args>(args)...); }






  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Vec new_vec_s(Args&&... args) { return new_reg<Vec>(TypeId::kFloat32, std::forward<Args>(args)...); }


  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Vec new_vec_d(Args&&... args) { return new_reg<Vec>(TypeId::kFloat64, std::forward<Args>(args)...); }


  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Vec new_vec_q(Args&&... args) { return new_reg<Vec>(TypeId::kInt32x4, std::forward<Args>(args)...); }







  ASMJIT_INLINE_NODEBUG Mem new_stack(uint32_t size, uint32_t alignment, const char* name = nullptr) {
    Mem m(Globals::NoInit);
    _new_stack(Out<BaseMem>(m), size, alignment, name);
    return m;
  }







  ASMJIT_INLINE_NODEBUG Mem new_const(ConstPoolScope scope, const void* data, size_t size) {
    Mem m(Globals::NoInit);
    _new_const(Out<BaseMem>(m), scope, data, size);
    return m;
  }


  ASMJIT_INLINE_NODEBUG Mem new_byte_const(ConstPoolScope scope, uint8_t val) noexcept { return new_const(scope, &val, 1); }

  ASMJIT_INLINE_NODEBUG Mem new_half_const(ConstPoolScope scope, uint16_t val) noexcept { return new_const(scope, &val, 2); }

  ASMJIT_INLINE_NODEBUG Mem new_word_const(ConstPoolScope scope, uint32_t val) noexcept { return new_const(scope, &val, 4); }

  ASMJIT_INLINE_NODEBUG Mem new_dword_const(ConstPoolScope scope, uint64_t val) noexcept { return new_const(scope, &val, 8); }


  ASMJIT_INLINE_NODEBUG Mem new_int16_const(ConstPoolScope scope, int16_t val) noexcept { return new_const(scope, &val, 2); }

  ASMJIT_INLINE_NODEBUG Mem new_uint16_const(ConstPoolScope scope, uint16_t val) noexcept { return new_const(scope, &val, 2); }

  ASMJIT_INLINE_NODEBUG Mem new_int32_const(ConstPoolScope scope, int32_t val) noexcept { return new_const(scope, &val, 4); }

  ASMJIT_INLINE_NODEBUG Mem new_uint32_const(ConstPoolScope scope, uint32_t val) noexcept { return new_const(scope, &val, 4); }

  ASMJIT_INLINE_NODEBUG Mem new_int64_const(ConstPoolScope scope, int64_t val) noexcept { return new_const(scope, &val, 8); }

  ASMJIT_INLINE_NODEBUG Mem new_uint64_const(ConstPoolScope scope, uint64_t val) noexcept { return new_const(scope, &val, 8); }


  ASMJIT_INLINE_NODEBUG Mem new_float_const(ConstPoolScope scope, float val) noexcept { return new_const(scope, &val, 4); }

  ASMJIT_INLINE_NODEBUG Mem new_double_const(ConstPoolScope scope, double val) noexcept { return new_const(scope, &val, 8); }







  ASMJIT_INLINE_NODEBUG Compiler& unfollow() noexcept { _inst_options |= InstOptions::kUnfollow; return *this; }











  ASMJIT_INLINE_NODEBUG Error load_address_of(const Gp& o0, const Mem& o1) { return _emitter()->_emitI(Inst::kIdAdr, o0, o1); }







  ASMJIT_INLINE_NODEBUG Error invoke_(Out<InvokeNode*> out, const Operand_& target, const FuncSignature& signature) {
    return add_invoke_node(out, Inst::kIdBlr, target, signature);
  }


  //!
  //! Creates a new \ref InvokeNode, initializes all the necessary members to match the given function `signature`,
  //! adds the node to the compiler, and stores its pointer to `out`. The operation is atomic, if anything fails
  //! nullptr is stored in `out` and error code is returned.
  ASMJIT_INLINE_NODEBUG Error invoke(Out<InvokeNode*> out, const Gp& target, const FuncSignature& signature) { return invoke_(out, target, signature); }
  //! \overload
  ASMJIT_INLINE_NODEBUG Error invoke(Out<InvokeNode*> out, const Mem& target, const FuncSignature& signature) { return invoke_(out, target, signature); }
  //! \overload
  ASMJIT_INLINE_NODEBUG Error invoke(Out<InvokeNode*> out, const Label& target, const FuncSignature& signature) { return invoke_(out, target, signature); }
  //! \overload
  ASMJIT_INLINE_NODEBUG Error invoke(Out<InvokeNode*> out, const Imm& target, const FuncSignature& signature) { return invoke_(out, target, signature); }
  //! \overload
  ASMJIT_INLINE_NODEBUG Error invoke(Out<InvokeNode*> out, uint64_t target, const FuncSignature& signature) { return invoke_(out, Imm(int64_t(target)), signature); }

  //! Return from function.
  //!
  //! \note This doesn't end the function - it just emits a return.
  ASMJIT_INLINE_NODEBUG Error ret() { return add_ret(Operand(), Operand()); }

  //! Return from function - one value.
  //!
  //! \note This doesn't end the function - it just emits a return.
  ASMJIT_INLINE_NODEBUG Error ret(const Reg& o0) { return add_ret(o0, Operand()); }

  //! Return from function - two values / register pair.
  //!
  //! \note This doesn't end the function - it just emits a return.
  ASMJIT_INLINE_NODEBUG Error ret(const Reg& o0, const Reg& o1) { return add_ret(o0, o1); }

  //! \}

  //! \name Jump Tables Support
  //! \{

  using EmitterExplicitT<Compiler>::br;

  //! Adds a jump to the given `target` with the provided jump `annotation`.
  ASMJIT_INLINE_NODEBUG Error br(const Reg& target, JumpAnnotation* annotation) { return emit_annotated_jump(Inst::kIdBr, target, annotation); }

  //! \}

  //! \name Events
  //! \{

  ASMJIT_API Error on_attach(CodeHolder& code) noexcept override;
  ASMJIT_API Error on_detach(CodeHolder& code) noexcept override;
  ASMJIT_API Error on_reinit(CodeHolder& code) noexcept override;

  //! \}

  //! \name Finalize
  //! \{

  ASMJIT_API Error finalize() override;

  //! \}
};

//! \}

ASMJIT_END_SUB_NAMESPACE

#endif // !ASMJIT_NO_COMPILER
#endif // ASMJIT_ARM_A64COMPILER_H_INCLUDED
