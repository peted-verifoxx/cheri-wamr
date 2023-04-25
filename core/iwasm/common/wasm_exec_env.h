/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef _WASM_EXEC_ENV_H
#define _WASM_EXEC_ENV_H

#include "bh_assert.h"
#if WASM_ENABLE_INTERP != 0
#include "../interpreter/wasm.h"
#endif

#ifdef __CHERI__
#include <cheriintrin.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct WASMModuleInstanceCommon;
struct WASMInterpFrame;

#if WASM_ENABLE_THREAD_MGR != 0
typedef struct WASMCluster WASMCluster;
#if WASM_ENABLE_DEBUG_INTERP != 0
typedef struct WASMCurrentEnvStatus WASMCurrentEnvStatus;
#endif
#endif

#ifdef OS_ENABLE_HW_BOUND_CHECK
typedef struct WASMJmpBuf {
    struct WASMJmpBuf *prev;
    korp_jmpbuf jmpbuf;
} WASMJmpBuf;
#endif

#ifdef __CHERI__
// @see : cheri_mem_mgmt.h

/* Our stack structure:
|.................<allocated space>......................|xx|
^           ^                                             ^
|           |                                             |
bottom    -->  top grows -->                        top_boundary
*/

typedef struct
{
    uint8* __capability top_boundary;
    uint8* __capability top;
    uint8* __capability bottom;
}  WASMCheriStack_t __attribute__((aligned(__BIGGEST_ALIGNMENT__)));
#endif


/* Execution environment */
typedef struct WASMExecEnv {
    /* Next thread's exec env of a WASM module instance. */
    struct WASMExecEnv *next;

    /* Previous thread's exec env of a WASM module instance. */
    struct WASMExecEnv *prev;

    /* Note: field module_inst, argv_buf, native_stack_boundary,
       suspend_flags, aux_stack_boundary, aux_stack_bottom, and
       native_symbol are used by AOTed code, don't change the
       places of them */

    /* The WASM module instance of current thread */
    struct WASMModuleInstanceCommon *module_inst;

#if WASM_ENABLE_AOT != 0
    uint32 *argv_buf;
#endif

    /* The boundary of native stack. When runtime detects that native
       frame may overrun this boundary, it throws stack overflow
       exception. */
    uint8 *native_stack_boundary;

    /* Used to terminate or suspend current thread
        bit 0: need to terminate
        bit 1: need to suspend
        bit 2: need to go into breakpoint
        bit 3: return from pthread_exit */
    union {
        uint32 flags;
        uintptr_t __padding__;
    } suspend_flags;

    /* Auxiliary stack boundary */
    union {
        uint32 boundary;
        uintptr_t __padding__;
    } aux_stack_boundary;

    /* Auxiliary stack bottom */
    union {
        uint32 bottom;
        uintptr_t __padding__;
    } aux_stack_bottom;

#if WASM_ENABLE_AOT != 0
    /* Native symbol list, reserved */
    void **native_symbol;
#endif

    /*
     * The lowest stack pointer value observed.
     * Assumption: native stack grows to the lower address.
     */
    uint8 *native_stack_top_min;

#if WASM_ENABLE_FAST_JIT != 0
    /**
     * Cache for
     * - jit native operations in 32-bit target which hasn't 64-bit
     *   int/float registers, mainly for the operations of double and int64,
     *   such as F64TOI64, F32TOI64, I64 MUL/REM, and so on.
     * - SSE instructions.
     **/
    uint64 jit_cache[2];
#endif

#if WASM_ENABLE_THREAD_MGR != 0
    /* thread return value */
    void *thread_ret_value;

    /* Must be provided by thread library */
    void *(*thread_start_routine)(void *);
    void *thread_arg;

    /* pointer to the cluster */
    WASMCluster *cluster;

    /* used to support debugger */
    korp_mutex wait_lock;
    korp_cond wait_cond;
    /* the count of threads which are joining current thread */
    uint32 wait_count;

    /* whether current thread is detached */
    bool thread_is_detached;
#endif

#if WASM_ENABLE_DEBUG_INTERP != 0
    WASMCurrentEnvStatus *current_status;
#endif

    /* attachment for native function */
#ifdef __CHERI__
    void* attachment __attribute__((aligned(__BIGGEST_ALIGNMENT__)));
#else
    void* attachment;
#endif

    void *user_data;

    /* Current interpreter frame of current thread */
    struct WASMInterpFrame *cur_frame;

    /* The native thread handle of current thread */
    korp_tid handle;

#if WASM_ENABLE_INTERP != 0 && WASM_ENABLE_FAST_INTERP == 0
#ifdef __CHERI__
    BlockAddr block_addr_cache[BLOCK_ADDR_CACHE_SIZE][BLOCK_ADDR_CONFLICT_SIZE] __attribute__((aligned(__BIGGEST_ALIGNMENT__)));
#else
    BlockAddr block_addr_cache[BLOCK_ADDR_CACHE_SIZE][BLOCK_ADDR_CONFLICT_SIZE];
#endif
#endif

#ifdef OS_ENABLE_HW_BOUND_CHECK
#ifdef __CHERI__
    WASMJmpBuf *jmpbuf_stack_top __attribute__((aligned(__BIGGEST_ALIGNMENT__)));
#else
    WASMJmpBuf* jmpbuf_stack_top;
#endif
    /* One guard page for the exception check */
    uint8 *exce_check_guard_page;
#endif

#if WASM_ENABLE_MEMORY_PROFILING != 0
    uint32 max_wasm_stack_used;
#endif

    /* The WASM stack size */
    uint32 wasm_stack_size;

#ifdef __CHERI__
    // On CHERI, we will manage the WASM stack ourselves
    WASMCheriStack_t* __capability wasm_stack_p __attribute__((aligned(__BIGGEST_ALIGNMENT__)));

#else
    /* The WASM stack of current thread */
    union {
        uint64 __make_it_8_byte_aligned_;

        struct {
            /* The top boundary of the stack. */
            uint8 *top_boundary;

            /* Top cell index which is free. */
            uint8 *top;

            /* The WASM stack. */
            uint8 bottom[1];
        } s;
    } wasm_stack;
#endif

#ifdef __CHERI__
} WASMExecEnv __attribute__((aligned(__BIGGEST_ALIGNMENT__)));
#else
} WASMExecEnv;
#endif

#if WASM_ENABLE_MEMORY_PROFILING != 0
#define RECORD_STACK_USAGE(e, p)               \
    do {                                       \
        if ((e)->native_stack_top_min > (p)) { \
            (e)->native_stack_top_min = (p);   \
        }                                      \
    } while (0)
#else
#define RECORD_STACK_USAGE(e, p) (void)0
#endif

WASMExecEnv *
wasm_exec_env_create_internal(struct WASMModuleInstanceCommon *module_inst,
                              uint32 stack_size);

void
wasm_exec_env_destroy_internal(WASMExecEnv *exec_env);

WASMExecEnv *
wasm_exec_env_create(struct WASMModuleInstanceCommon *module_inst,
                     uint32 stack_size);

void
wasm_exec_env_destroy(WASMExecEnv *exec_env);

/**
 * Allocate a WASM frame from the WASM stack.
 *
 * @param exec_env the current execution environment
 * @param size size of the WASM frame, it must be a multiple of 4
 *
 * @return the WASM frame if there is enough space in the stack area
 * with a protection area, NULL otherwise
 */
#ifdef __CHERI__
static inline void * __capability
wasm_exec_env_alloc_wasm_frame(WASMExecEnv* exec_env, size_t size)
{
    // Size must be aligned
    bh_assert(cheri_is_aligned(size, __BIGGEST_ALIGNMENT__));

    LOG_VERBOSE("Alloc frame++: Stack top addr=0x%" PRIx64
        ", Boundary = 0x%" PRIx64
        ", size = %d\n", exec_env->wasm_stack_p->top, exec_env->wasm_stack_p->top_boundary, size);

    // On CHERI, we want the frame to start on an aligned address and be an aligned size
    exec_env->wasm_stack_p->top = cheri_align_up(exec_env->wasm_stack_p->top, __BIGGEST_ALIGNMENT__);

    void* __capability addr = exec_env->wasm_stack_p->top;

    if (size * 2 > ((uintptr_t)exec_env->wasm_stack_p->top_boundary - (uintptr_t)addr))
    {
        /* WASM stack overflow. */
        return NULL;
    }

    exec_env->wasm_stack_p->top += size;

#if WASM_ENABLE_MEMORY_PROFILING != 0
    {
        size_t wasm_stack_used =
            (uintptr_t)exec_env->wasm_stack_p->top - (uintptr_t)exec_env->wasm_stack_p->bottom;
        if (wasm_stack_used > exec_env->max_wasm_stack_used)
            exec_env->max_wasm_stack_used = wasm_stack_used;
    }
#endif

    LOG_VERBOSE("Alloc frame--: Returned addr=0x%" PRIx64 "\n", addr);

    return addr;
}

#else   /* __CHERI__ */
static inline void*
wasm_exec_env_alloc_wasm_frame(WASMExecEnv * exec_env, unsigned size)
{
    uint8* addr = exec_env->wasm_stack.s.top;

    bh_assert(!(size & 3));

    /* For classic interpreter, the outs area doesn't contain the const cells,
       its size cannot be larger than the frame size, so here checking stack
       overflow with multiplying by 2 is enough. For fast interpreter, since
       the outs area contains const cells, its size may be larger than current
       frame size, we should check again before putting the function arguments
       into the outs area. */
    if (size * 2
        > (uint32)(uintptr_t)(exec_env->wasm_stack.s.top_boundary - addr)) {
        /* WASM stack overflow. */
        return NULL;
    }

    exec_env->wasm_stack.s.top += size;

#if WASM_ENABLE_MEMORY_PROFILING != 0
    {
        uint32 wasm_stack_used =
            exec_env->wasm_stack.s.top - exec_env->wasm_stack.s.bottom;
        if (wasm_stack_used > exec_env->max_wasm_stack_used)
            exec_env->max_wasm_stack_used = wasm_stack_used;
    }
#endif

    return (void*)addr;
}
#endif /* __CHERI__ */


static inline void
wasm_exec_env_free_wasm_frame(WASMExecEnv *exec_env, void *__capability prev_top)
{
#ifdef __CHERI__
    bh_assert((uintptr_t)prev_top >= (uintptr_t)exec_env->wasm_stack_p->bottom);
    exec_env->wasm_stack_p->top = (uint8*__capability)prev_top;
#else
    bh_assert((uint8*)prev_top >= exec_env->wasm_stack.s.bottom);
    exec_env->wasm_stack.s.top = (uint8*)prev_top;
#endif
}

/**
 * Get the current WASM stack top pointer.
 *
 * @param exec_env the current execution environment
 *
 * @return the current WASM stack top pointer
 */
static inline void *
wasm_exec_env_wasm_stack_top(WASMExecEnv *exec_env)
{
#ifdef __CHERI__
    return (void*)exec_env->wasm_stack_p->top;
#else
    return exec_env->wasm_stack.s.top;
#endif
}

/**
 * Set the current frame pointer.
 *
 * @param exec_env the current execution environment
 * @param frame the WASM frame to be set for the current exec env
 */
static inline void
wasm_exec_env_set_cur_frame(WASMExecEnv *exec_env,
                            struct WASMInterpFrame *frame)
{
    exec_env->cur_frame = frame;
}

/**
 * Get the current frame pointer.
 *
 * @param exec_env the current execution environment
 *
 * @return the current frame pointer
 */
static inline struct WASMInterpFrame *
wasm_exec_env_get_cur_frame(WASMExecEnv *exec_env)
{
    return exec_env->cur_frame;
}

struct WASMModuleInstanceCommon *
wasm_exec_env_get_module_inst(WASMExecEnv *exec_env);

void
wasm_exec_env_set_module_inst(
    WASMExecEnv *exec_env, struct WASMModuleInstanceCommon *const module_inst);

void
wasm_exec_env_set_thread_info(WASMExecEnv *exec_env);

#if WASM_ENABLE_THREAD_MGR != 0
void *
wasm_exec_env_get_thread_arg(WASMExecEnv *exec_env);

void
wasm_exec_env_set_thread_arg(WASMExecEnv *exec_env, void *thread_arg);
#endif

#ifdef OS_ENABLE_HW_BOUND_CHECK
void
wasm_exec_env_push_jmpbuf(WASMExecEnv *exec_env, WASMJmpBuf *jmpbuf);

WASMJmpBuf *
wasm_exec_env_pop_jmpbuf(WASMExecEnv *exec_env);
#endif

#ifdef __cplusplus
}
#endif

#endif /* end of _WASM_EXEC_ENV_H */
