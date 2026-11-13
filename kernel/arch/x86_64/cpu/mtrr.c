
/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: mtrr.c
 *    Description: Implementação da gestão dos Memory Type Range Registers
 *                 (MTRR) em x86_64 com suporte a inicialização BSP e
 *                 sincronização AP (SMP).
 *
 *         Author: Nelson Cole
 *   Created Date: 12/11/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 12/11/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <kernel/klib.h>
#include <kernel/arch/x86_64/cpu/mtrr.h>

static mtrr_info_t mtrr_state = {0};
static mtrr_saved_entry_t saved_var_mtrrs[MAX_VARIABLE_MTRRS] = {0};
static uint64_t saved_def_type = 0;

static inline void cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx)
{
    __asm__ __volatile__(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
    );
}

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;

    __asm__ __volatile__(
        "rdmsr"
        : "=a"(lo), "=d"(hi)
        : "c"(msr)
    );

    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);

    __asm__ __volatile__(
        "wrmsr"
        :
        : "c"(msr), "a"(lo), "d"(hi)
        : "memory"
    );
}

static inline uint64_t mtrr_begin_update(void)
{
    uint64_t cr0, cr3;

    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));

    cr0 |= (1ULL << 30);
    cr0 &= ~(1ULL << 29);

    __asm__ __volatile__(
        "mov %0, %%cr0"
        :
        : "r"(cr0)
        : "memory"
    );

    __asm__ __volatile__("wbinvd" ::: "memory");

    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));

    __asm__ __volatile__(
        "mov %0, %%cr3"
        :
        : "r"(cr3)
        : "memory"
    );

    uint64_t def = rdmsr(IA32_MTRR_DEF_TYPE);

    wrmsr(IA32_MTRR_DEF_TYPE, def & ~MTRR_ENABLE);

    return def;
}

static inline void mtrr_end_update(uint64_t saved_def)
{
    uint64_t cr0, cr3;

    __asm__ __volatile__("wbinvd" ::: "memory");

    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));

    __asm__ __volatile__(
        "mov %0, %%cr3"
        :
        : "r"(cr3)
        : "memory"
    );

    wrmsr(IA32_MTRR_DEF_TYPE, saved_def);

    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));

    cr0 &= ~(1ULL << 30);
    cr0 &= ~(1ULL << 29);

    __asm__ __volatile__(
        "mov %0, %%cr0"
        :
        : "r"(cr0)
        : "memory"
    );
}

static uint8_t cpu_get_phys_addr_bits(void)
{
    uint32_t eax, ebx, ecx, edx;

    cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);

    if (eax >= 0x80000008) {
        cpuid(0x80000008, 0, &eax, &ebx, &ecx, &edx);

        uint8_t bits = (uint8_t)(eax & 0xFF);

        if (bits >= 32 && bits <= 52)
            return bits;
    }

    return 36;
}

int cpu_supports_mtrr(void)
{
    uint32_t eax, ebx, ecx, edx;

    cpuid(1, 0, &eax, &ebx, &ecx, &edx);

    int has_msr  = (edx & (1U << 5)) != 0;
    int has_mtrr = (edx & (1U << 12)) != 0;

    return has_msr && has_mtrr;
}

void mtrr_init(void)
{
    if (!cpu_supports_mtrr()) {
        mtrr_state.supported = 0;
        return;
    }

    mtrr_state.supported = 1;

    uint64_t cap = rdmsr(IA32_MTRR_CAP);
    uint64_t def = rdmsr(IA32_MTRR_DEF_TYPE);

    mtrr_state.variable_count =
        (uint32_t)(cap & MTRR_CAP_VCNT_MASK);

    mtrr_state.fixed_supported =
        (cap & MTRR_CAP_FIX_SUPPORTED) != 0;

    mtrr_state.wc_supported =
        (cap & MTRR_CAP_WC_SUPPORTED) != 0;

    uint8_t phys_bits = cpu_get_phys_addr_bits();

    mtrr_state.phys_addr_mask =
        ((1ULL << phys_bits) - 1) & ~0xFFFULL;

    if (!(def & MTRR_ENABLE)) {
        uint64_t saved = mtrr_begin_update();

        saved &= (0xFFULL | MTRR_FIXED_ENABLE | MTRR_ENABLE);
        saved |= MTRR_ENABLE;

        if (mtrr_state.fixed_supported)
            saved |= MTRR_FIXED_ENABLE;

        mtrr_end_update(saved);
    }
}

void mtrr_init_bsp(void)
{
    mtrr_init();

    if (!mtrr_state.supported)
        return;

    saved_def_type =
        rdmsr(IA32_MTRR_DEF_TYPE) &
        (0xFFULL | MTRR_FIXED_ENABLE | MTRR_ENABLE);

    for (uint32_t i = 0;
         i < mtrr_state.variable_count && i < MAX_VARIABLE_MTRRS;
         i++) {

        uint64_t base = rdmsr(IA32_MTRR_PHYSBASE(i));
        uint64_t mask = rdmsr(IA32_MTRR_PHYSMASK(i));

        if (mask & MTRR_PHYS_MASK_VALID) {
            saved_var_mtrrs[i].base =
                (base & mtrr_state.phys_addr_mask) |
                (base & 0xFFULL);

            saved_var_mtrrs[i].mask =
                (mask & mtrr_state.phys_addr_mask) |
                MTRR_PHYS_MASK_VALID;

            saved_var_mtrrs[i].active = 1;
        } else {
            saved_var_mtrrs[i].base = 0;
            saved_var_mtrrs[i].mask = 0;
            saved_var_mtrrs[i].active = 0;
        }
    }
}

static int cpu_is_hypervisor(void)
{
    uint32_t eax, ebx, ecx, edx;

    cpuid(1, 0, &eax, &ebx, &ecx, &edx);

    return (ecx & (1U << 31)) != 0;
}

void mtrr_init_ap(void)
{
    if (!mtrr_state.supported || !cpu_supports_mtrr())
        return;

    /*
     * Em ambientes virtualizados, evitar alterações dos MTRRs
     * nos APs sem confirmação de suporte do hypervisor.
     */
    if (cpu_is_hypervisor())
        return;

    uint32_t ap_var_count =
        (uint32_t)(rdmsr(IA32_MTRR_CAP) & MTRR_CAP_VCNT_MASK);

    uint32_t count =
        (ap_var_count < mtrr_state.variable_count)
        ? ap_var_count
        : mtrr_state.variable_count;

    uint64_t dummy = mtrr_begin_update();
    (void)dummy;

    for (uint32_t i = 0;
         i < count && i < MAX_VARIABLE_MTRRS;
         i++) {

        if (saved_var_mtrrs[i].active) {
            uint64_t clean_base =
                (saved_var_mtrrs[i].base &
                 mtrr_state.phys_addr_mask) |
                (saved_var_mtrrs[i].base & 0xFFULL);

            uint64_t clean_mask =
                (saved_var_mtrrs[i].mask &
                 mtrr_state.phys_addr_mask) |
                MTRR_PHYS_MASK_VALID;

            wrmsr(IA32_MTRR_PHYSBASE(i), clean_base);
            wrmsr(IA32_MTRR_PHYSMASK(i), clean_mask);
        } else {
            wrmsr(IA32_MTRR_PHYSMASK(i), 0);
            wrmsr(IA32_MTRR_PHYSBASE(i), 0);
        }
    }

    uint64_t clean_def =
        saved_def_type &
        (0xFFULL | MTRR_FIXED_ENABLE | MTRR_ENABLE);

    mtrr_end_update(clean_def);
}

int mtrr_set_variable(uint32_t index, uint64_t base,
                      uint64_t size, uint8_t type)
{
    if (!mtrr_state.supported ||
        index >= mtrr_state.variable_count)
        return MTRR_ERR_NOT_SUPPORTED;

    if (size < 0x1000 || (size & (size - 1)) != 0)
        return MTRR_ERR_INVALID_SIZE;

    if ((base & (size - 1)) != 0)
        return MTRR_ERR_UNALIGNED_BASE;

    if (type == MTRR_TYPE_WRITECOMBINING &&
        !mtrr_state.wc_supported)
        return MTRR_ERR_WC_NOT_SUPPORTED;

    uint64_t phys_base =
        (base & mtrr_state.phys_addr_mask) |
        (type & 0xFF);

    uint64_t phys_mask =
        ((~(size - 1)) & mtrr_state.phys_addr_mask) |
        MTRR_PHYS_MASK_VALID;

    uint64_t saved_def = mtrr_begin_update();

    wrmsr(IA32_MTRR_PHYSBASE(index), phys_base);
    wrmsr(IA32_MTRR_PHYSMASK(index), phys_mask);

    mtrr_end_update(saved_def);

    if (index < MAX_VARIABLE_MTRRS) {
        saved_var_mtrrs[index].base = phys_base;
        saved_var_mtrrs[index].mask = phys_mask;
        saved_var_mtrrs[index].active = 1;
    }

    return MTRR_SUCCESS;
}

static uint64_t mtrr_next_pow2(uint64_t size)
{
    uint64_t pow2 = 0x1000;

    while (pow2 < size) {
        if (pow2 > (1ULL << 62))
            return 0;

        pow2 <<= 1;
    }

    return pow2;
}

int mtrr_add_region(uint64_t base, uint64_t size, uint8_t type)
{
    if (!mtrr_state.supported)
        return MTRR_ERR_NOT_SUPPORTED;

    if (size == 0)
        return MTRR_ERR_INVALID_SIZE;

    uint64_t aligned_size = mtrr_next_pow2(size);

    if (aligned_size == 0)
        return MTRR_ERR_INVALID_SIZE;

    if ((base & (aligned_size - 1)) != 0)
        return MTRR_ERR_UNALIGNED_BASE;

    for (uint32_t i = 0;
         i < mtrr_state.variable_count;
         i++) {

        uint64_t mask = rdmsr(IA32_MTRR_PHYSMASK(i));

        if (!(mask & MTRR_PHYS_MASK_VALID)) {
            int res =
                mtrr_set_variable(i, base, aligned_size, type);

            return (res == MTRR_SUCCESS)
                ? (int)i
                : res;
        }
    }

    return MTRR_ERR_NO_FREE_SLOT;
}

void mtrr_clear_variable(uint32_t index)
{
    if (!mtrr_state.supported ||
        index >= mtrr_state.variable_count)
        return;

    uint64_t saved_def = mtrr_begin_update();

    wrmsr(IA32_MTRR_PHYSMASK(index), 0);
    wrmsr(IA32_MTRR_PHYSBASE(index), 0);

    mtrr_end_update(saved_def);

    if (index < MAX_VARIABLE_MTRRS) {
        saved_var_mtrrs[index].base = 0;
        saved_var_mtrrs[index].mask = 0;
        saved_var_mtrrs[index].active = 0;
    }
}

const mtrr_info_t *mtrr_get_info(void)
{
    return &mtrr_state;
}