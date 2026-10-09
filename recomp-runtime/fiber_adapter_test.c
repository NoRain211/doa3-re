#include "fiber_adapter.h"
#include "xbox_memory_layout.h"

#include <stdio.h>
#include <string.h>

enum {
    TEST_HEAP_BASE = 0x27000000u,
    TEST_HEAP_SIZE = 0x00200000u,
    TEST_STATE_BASE = 0x28000000u,
    TEST_STATE_SIZE = 0x00002000u,
    TEST_TLS_BLOCK = TEST_STATE_BASE + 0x100u,
    TEST_ENTRY_ESP = TEST_STATE_BASE + 0x1100u,
    TEST_STACK_SIZE = 0x3000u,
    TEST_FIBER_ENTRY = 0x000b5570u,
};

void recomp_test_heap_reset(uint32_t cursor, int fail_after);

static int expect_u32(const char *field, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "fiber adapter: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

static void prepare_call(uint32_t argument_count, const uint32_t *arguments)
{
    uint32_t *stack = recomp_memory_u32(TEST_ENTRY_ESP);

    stack[0] = 0x0010abcdu;
    for (uint32_t i = 0u; i < argument_count; ++i) {
        stack[i + 1u] = arguments[i];
    }
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
}

static const RecompFiber *find_fiber(uint32_t guest_handle)
{
    const RecompFiberModel *model = recomp_fiber_adapter_model();

    for (size_t i = 0u; i < RECOMP_FIBER_MAX_COUNT; ++i) {
        const RecompFiber *fiber = &model->fibers[i];

        if (fiber->active && fiber->guest_handle == guest_handle) {
            return fiber;
        }
    }
    return NULL;
}

static const RecompFiberBindings *test_bindings;
static int switch_passed;
static uint32_t entry_count;

static void test_fiber_entry(void)
{
    uint32_t esp = recomp_runtime.registers.esp;
    uint32_t parameter = *recomp_memory_u32(esp + 4u);
    uint32_t handle = recomp_fiber_adapter_model()->current_handle;

    switch_passed &= expect_u32("entry parameter", parameter, 0x22222222u);
    for (;;) {
        ++entry_count;
        switch_passed &= expect_u32(
            "current TLS fiber", *recomp_memory_u32(TEST_TLS_BLOCK + 4u), handle);
        recomp_runtime.registers.ebx = 0x12345678u;
        *recomp_memory_u32(0u) = 0x87654321u;
        recomp_runtime.registers.esp = esp - 8u;
        *recomp_memory_u32(esp - 4u) = TEST_TLS_BLOCK + 8u;
        recomp_fiber_lookup_manual(test_bindings->switch_to, test_bindings)();
        switch_passed &= expect_u32(
            "resumed ESP",
            recomp_runtime.registers.esp,
            esp);
        switch_passed &= expect_u32(
            "resumed EBX",
            recomp_runtime.registers.ebx,
            0x12345678u);
        switch_passed &= expect_u32(
            "resumed SEH",
            *recomp_memory_u32(0u),
            0x87654321u);
    }
}

static int test_bindings_round_trip(const RecompFiberBindings *bindings)
{
    static uint8_t low_memory[8];
    static uint8_t tls_index_memory[4];
    static uint8_t heap_memory[TEST_HEAP_SIZE];
    static uint8_t state_memory[TEST_STATE_SIZE];
    const RecompMemoryRegion regions[] = {
        {
            .address = 0u,
            .size = sizeof low_memory,
            .data = low_memory,
        },
        {
            .address = bindings->tls_index,
            .size = sizeof tls_index_memory,
            .data = tls_index_memory,
        },
        {
            .address = TEST_HEAP_BASE,
            .size = sizeof heap_memory,
            .data = heap_memory,
        },
        {
            .address = TEST_STATE_BASE,
            .size = sizeof state_memory,
            .data = state_memory,
        },
    };
    const RecompFunctionEntry functions[] = {
        {TEST_FIBER_ENTRY, test_fiber_entry},
    };
    const uint32_t first_parameter = 0x11111111u;
    const uint32_t second_parameter = 0x22222222u;
    uint32_t arguments[3];
    uint32_t first_handle;
    uint32_t first_heap_checkpoint;
    uint32_t second_handle;
    RecompFunction adapter;
    const RecompFiber *fiber;
    int passed = 1;

    memset(low_memory, 0, sizeof low_memory);
    memset(tls_index_memory, 0, sizeof tls_index_memory);
    memset(heap_memory, 0xa5, sizeof heap_memory);
    memset(state_memory, 0, sizeof state_memory);
    recomp_runtime_init(regions, 4u, NULL, 0u, functions, 1u);
    recomp_test_heap_reset(TEST_HEAP_BASE, -1);
    recomp_fiber_adapter_reset();

    *recomp_memory_u32(0u) = 0xffffffffu;
    *recomp_memory_u32(4u) = TEST_STATE_BASE;
    *recomp_memory_u32(bindings->tls_index) = 1u;
    *recomp_memory_u32(TEST_STATE_BASE + 4u) = TEST_TLS_BLOCK;

    arguments[0] = 0xabcdef01u;
    prepare_call(1u, arguments);
    adapter = recomp_fiber_lookup_manual(bindings->convert_thread, bindings);
    adapter();

    arguments[0] = TEST_STACK_SIZE;
    arguments[1] = TEST_FIBER_ENTRY;
    arguments[2] = first_parameter;
    prepare_call(3u, arguments);
    adapter = recomp_fiber_lookup_manual(bindings->create, bindings);
    adapter();
    first_handle = recomp_runtime.registers.eax;
    first_heap_checkpoint = xbox_HeapCheckpoint();

    arguments[0] = first_handle;
    prepare_call(1u, arguments);
    adapter = recomp_fiber_lookup_manual(bindings->delete_fiber, bindings);
    adapter();
    passed &= expect_u32(
        "deleted fiber absent",
        find_fiber(first_handle) != NULL,
        0u);
    *recomp_memory_u32(TEST_HEAP_BASE + 0x100u) = 0xdeadbeefu;

    arguments[0] = TEST_STACK_SIZE;
    arguments[1] = TEST_FIBER_ENTRY;
    arguments[2] = second_parameter;
    prepare_call(3u, arguments);
    adapter = recomp_fiber_lookup_manual(bindings->create, bindings);
    adapter();
    second_handle = recomp_runtime.registers.eax;

    passed &= expect_u32(
        "guest stack handle reused", second_handle, first_handle);
    passed &= expect_u32(
        "guest heap unchanged",
        xbox_HeapCheckpoint(),
        first_heap_checkpoint);
    passed &= expect_u32(
        "guest stack base",
        *recomp_memory_u32(second_handle + 8u),
        TEST_HEAP_BASE);
    passed &= expect_u32(
        "guest stack parameter",
        *recomp_memory_u32(second_handle),
        second_parameter);
    passed &= expect_u32(
        "guest stack cleared",
        *recomp_memory_u32(TEST_HEAP_BASE + 0x100u),
        0u);
    fiber = find_fiber(second_handle);
    passed &= expect_u32("recreated fiber present", fiber != NULL, 1u);
    if (fiber != NULL) {
        passed &= expect_u32(
            "recreated fiber parameter", fiber->parameter, second_parameter);
    }

    test_bindings = bindings;
    switch_passed = 1;
    entry_count = 0u;
    for (uint32_t i = 1u; i <= 2u; ++i) {
        arguments[0] = second_handle;
        prepare_call(1u, arguments);
        recomp_runtime.registers.ebx = 0xabcdef12u;
        adapter = recomp_fiber_lookup_manual(bindings->switch_to, bindings);
        adapter();
        passed &= expect_u32("fiber ran and yielded", entry_count, i);
        passed &= expect_u32(
            "main ESP",
            recomp_runtime.registers.esp,
            TEST_ENTRY_ESP + 8u);
        passed &= expect_u32(
            "main EBX",
            recomp_runtime.registers.ebx,
            0xabcdef12u);
        passed &= expect_u32("main SEH", *recomp_memory_u32(0u), 0xffffffffu);
        passed &= expect_u32(
            "main TLS fiber",
            *recomp_memory_u32(TEST_TLS_BLOCK + 4u),
            TEST_TLS_BLOCK + 8u);
    }
    passed &= switch_passed;

    arguments[0] = second_handle;
    prepare_call(1u, arguments);
    adapter = recomp_fiber_lookup_manual(bindings->delete_fiber, bindings);
    adapter();
    recomp_fiber_adapter_reset();
    return passed;
}

int recomp_fiber_adapter_test(void)
{
    int passed = test_bindings_round_trip(&recomp_fiber_doaxbv_bindings);
    passed &= test_bindings_round_trip(&recomp_fiber_doa3_bindings);
    return passed;
}
