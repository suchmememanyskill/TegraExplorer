#pragma once
#include <stdint.h>

typedef struct {
    uint16_t offset;
    uint16_t width;
} RegMask;

typedef struct {
    uint16_t offset;
    uint16_t width;
    uint32_t value;
} RegField;

static inline uint32_t reg_mask_encode(RegMask m) {
    return ((~0u >> (32 - m.width)) << m.offset);
}

static inline uint32_t reg_field_encode_mask(RegField f) {
    return ((~0u >> (32 - f.width)) << f.offset);
}

static inline uint32_t reg_field_encode_value(RegField f) {
    return ((f.value << f.offset) & reg_field_encode_mask(f));
}

#define REG_BITS_MASK(OFFSET, WIDTH)         ((RegMask){(uint16_t)(OFFSET),(uint16_t)(WIDTH)})
#define REG_BITS_VALUE(OFFSET, WIDTH, VALUE) ((RegField){(uint16_t)(OFFSET),(uint16_t)(WIDTH),(uint32_t)(VALUE)})

#define REG_NAMED_BITS_MASK(P,N)             REG_BITS_MASK(P##_##N##_OFFSET, P##_##N##_WIDTH)
#define REG_NAMED_BITS_VALUE(P,N,V)          REG_BITS_VALUE(P##_##N##_OFFSET, P##_##N##_WIDTH, (V))
#define REG_NAMED_BITS_ENUM(P,N,E)           REG_BITS_VALUE(P##_##N##_OFFSET, P##_##N##_WIDTH, P##_##N##_##E)
#define REG_NAMED_BITS_ENUM_SEL(P,N,C,T,F)  REG_BITS_VALUE(P##_##N##_OFFSET, P##_##N##_WIDTH, (C) ? (uint32_t)P##_##N##_##T : (uint32_t)P##_##N##_##F)

#define REG_DEFINE_NAMED_REG(P,N,OFF,W)  enum { P##_##N##_OFFSET=(OFF), P##_##N##_WIDTH=(W) }

#define REG_DEFINE_NAMED_BIT_ENUM(P,N,OFF,Z,O) \
    REG_DEFINE_NAMED_REG(P,N,OFF,1); typedef enum P##_##N { P##_##N##_##Z=0, P##_##N##_##O=1 } P##_##N##_t

#define REG_DEFINE_NAMED_TWO_BIT_ENUM(P,N,OFF,A,B,C,D) \
    REG_DEFINE_NAMED_REG(P,N,OFF,2); typedef enum P##_##N { P##_##N##_##A=0,P##_##N##_##B=1,P##_##N##_##C=2,P##_##N##_##D=3 } P##_##N##_t

#define REG_DEFINE_NAMED_THREE_BIT_ENUM(P,N,OFF,A,B,C,D,E,F,G,H) \
    REG_DEFINE_NAMED_REG(P,N,OFF,3); typedef enum P##_##N { P##_##N##_##A=0,P##_##N##_##B=1,P##_##N##_##C=2,P##_##N##_##D=3,P##_##N##_##E=4,P##_##N##_##F=5,P##_##N##_##G=6,P##_##N##_##H=7 } P##_##N##_t

#define REG_DEFINE_NAMED_FOUR_BIT_ENUM(P,N,OFF,A,B,C,D,E,F,G,H,I,J,K,L,M,NN,O,PP) \
    REG_DEFINE_NAMED_REG(P,N,OFF,4); typedef enum P##_##N { P##_##N##_##A=0,P##_##N##_##B=1,P##_##N##_##C=2,P##_##N##_##D=3,P##_##N##_##E=4,P##_##N##_##F=5,P##_##N##_##G=6,P##_##N##_##H=7,P##_##N##_##I=8,P##_##N##_##J=9,P##_##N##_##K=10,P##_##N##_##L=11,P##_##N##_##M=12,P##_##N##_##NN=13,P##_##N##_##O=14,P##_##N##_##PP=15 } P##_##N##_t

static inline void RegWrite(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}

static inline uint32_t RegRead(uintptr_t addr) {
    return *(volatile uint32_t *)addr;
}

static inline void RegReadWrite(uintptr_t addr, uint32_t val, uint32_t mask) {
    uint32_t *reg = (uint32_t *)addr;
    *reg = (*reg & ~mask) | (val & mask);
}

static inline void RegSetBits(uintptr_t addr, uint32_t bits) {
    *(volatile uint32_t *)addr |= bits;
}

static inline void RegClearBits(uintptr_t addr, uint32_t bits) {
    *(volatile uint32_t *)addr &= ~bits;
}

static inline uint32_t RegGetValue(uintptr_t addr, RegMask mask) {
    return (RegRead(addr) & reg_mask_encode(mask)) >> mask.offset;
}

static inline uint32_t RegGetField(uint32_t val, RegMask mask) {
    return (val & reg_mask_encode(mask)) >> mask.offset;
}

static inline uint32_t RegSetField(uint32_t val, RegField field) {
    return (val & ~reg_field_encode_mask(field)) | reg_field_encode_value(field);
}

static inline void RegReadWriteField(uintptr_t addr, RegField field) {
    RegReadWrite(addr, reg_field_encode_value(field), reg_field_encode_mask(field));
}
