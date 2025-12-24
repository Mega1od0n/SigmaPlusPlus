




#ifndef ASMJIT_ARM_A64BUILDER_H_INCLUDED
#define ASMJIT_ARM_A64BUILDER_H_INCLUDED

#include "../core/api-config.h"
#ifndef ASMJIT_NO_BUILDER

#include "../core/builder.h"
#include "../arm/a64emitter.h"

ASMJIT_BEGIN_SUB_NAMESPACE(a64)





class ASMJIT_VIRTAPI Builder
  : public BaseBuilder,
    public EmitterExplicitT<Builder> {
public:
  ASMJIT_NONCOPYABLE(Builder)
  using Base = BaseBuilder;




  ASMJIT_API explicit Builder(CodeHolder* code = nullptr) noexcept;
  ASMJIT_API ~Builder() noexcept override;






  ASMJIT_API Error on_attach(CodeHolder& code) noexcept override;
  ASMJIT_API Error on_detach(CodeHolder& code) noexcept override;






  ASMJIT_API Error finalize() override;


};



ASMJIT_END_SUB_NAMESPACE

#endif
#endif
