




#ifndef ASMJIT_ARM_A64INSTDB_H_INCLUDED
#define ASMJIT_ARM_A64INSTDB_H_INCLUDED

#include "../arm/a64globals.h"

ASMJIT_BEGIN_SUB_NAMESPACE(a64)





namespace InstDB {


enum InstFlags : uint32_t {

  kInstFlagCond = 0x00000001u,

  kInstFlagPair = 0x00000002u,

  kInstFlagLong = 0x00000004u,

  kInstFlagNarrow = 0x00000008u,

  kInstFlagVH0_15 = 0x00000010u,


  kInstFlagConsecutive = 0x00000080u
};


struct InstInfo {

  uint32_t _encoding : 8;

  uint32_t _encoding_data_index : 8;
  uint32_t _reserved : 16;

  uint16_t _rw_info_index;
  uint16_t _flags;




  [[nodiscard]]
  ASMJIT_INLINE_NODEBUG uint32_t rw_info_index() const noexcept { return _rw_info_index; }

  [[nodiscard]]
  ASMJIT_INLINE_NODEBUG uint32_t flags() const noexcept { return _flags; }

  [[nodiscard]]
  ASMJIT_INLINE_NODEBUG bool has_flag(uint32_t flag) const { return (_flags & flag) != 0; }


};

ASMJIT_VARAPI const InstInfo _inst_info_table[];

[[nodiscard]]
static ASMJIT_INLINE const InstInfo& inst_info_by_id(InstId inst_id) noexcept {
  inst_id &= uint32_t(InstIdParts::kRealId);
  ASMJIT_ASSERT(Inst::is_defined_id(inst_id));
  return _inst_info_table[inst_id];
}

}



ASMJIT_END_SUB_NAMESPACE

#endif
