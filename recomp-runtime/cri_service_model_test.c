#include "cri_service_model.h"

#include <stdio.h>

typedef struct CriTestContext {
    RecompCriServiceModel *model;
    uint32_t calls;
    uint32_t nested_calls;
    RecompCriServiceResult nested_result;
} CriTestContext;

static int expect_u32(const char *field, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "CRI service model: %s was %u, expected %u\n",
        field,
        actual,
        expected);
    return 0;
}

static void count_step(void *context)
{
    CriTestContext *test = context;

    ++test->calls;
}

static void reenter_lane2(void *context)
{
    CriTestContext *test = context;

    ++test->calls;
    test->nested_result = recomp_cri_service_run_lane2(
        test->model, count_step, test);
}

static void reenter_lane5(void *context)
{
    CriTestContext *test = context;

    ++test->nested_calls;
    test->nested_result = recomp_cri_service_run_lane5(
        test->model, count_step, test);
}

static void nest_lane5(void *context)
{
    CriTestContext *test = context;

    ++test->calls;
    test->nested_result = recomp_cri_service_run_lane5(
        test->model, reenter_lane5, test);
}

int recomp_cri_service_model_test(void)
{
    RecompCriServiceModel model;
    CriTestContext test = {.model = &model};
    int passed = 1;

    /* A late worker must deliver every elapsed refresh, without rounding
       60 Hz to 16 ms or counting a second poll as another refresh. */
    uint64_t tick = recomp_cri_vblank_tick(1000000000u);
    passed &= expect_u32("early refresh", (uint32_t)(recomp_cri_vblank_tick(1016000000u) - tick), 0u);
    passed &= expect_u32("late refreshes", (uint32_t)(recomp_cri_vblank_tick(1050000000u) - tick), 3u);
    tick = recomp_cri_vblank_tick(1050000000u);
    passed &= expect_u32("same refresh", (uint32_t)(recomp_cri_vblank_tick(1050000001u) - tick), 0u);
    passed &= expect_u32("one second", (uint32_t)(recomp_cri_vblank_tick(2050000000u) - tick), 60u);

    recomp_cri_service_reset(&model);
    passed &= expect_u32("reset lane 2", model.lane2_active, 0u);
    passed &= expect_u32("reset lane 5", model.lane5_active, 0u);
    passed &= expect_u32("reset batches", model.lane2_batches, 0u);

    passed &= expect_u32(
        "lane 2 result",
        recomp_cri_service_run_lane2(&model, count_step, &test),
        RECOMP_CRI_SERVICE_OK);
    passed &= expect_u32("lane 2 calls", test.calls, 1u);
    passed &= expect_u32("lane 2 batches", model.lane2_batches, 1u);
    passed &= expect_u32("lane 2 cleared", model.lane2_active, 0u);

    test.calls = 0u;
    passed &= expect_u32(
        "lane 2 outer result",
        recomp_cri_service_run_lane2(&model, reenter_lane2, &test),
        RECOMP_CRI_SERVICE_OK);
    passed &= expect_u32(
        "lane 2 nested result", test.nested_result, RECOMP_CRI_SERVICE_BUSY);
    passed &= expect_u32("lane 2 reentry calls", test.calls, 1u);
    passed &= expect_u32("lane 2 busy skips", model.lane2_busy_skips, 1u);

    test.calls = 0u;
    test.nested_calls = 0u;
    passed &= expect_u32(
        "nested lane 5 outer result",
        recomp_cri_service_run_lane2(&model, nest_lane5, &test),
        RECOMP_CRI_SERVICE_OK);
    passed &= expect_u32(
        "nested lane 5 result", test.nested_result, RECOMP_CRI_SERVICE_OK);
    passed &= expect_u32("nested lane 2 calls", test.calls, 1u);
    passed &= expect_u32("nested lane 5 calls", test.nested_calls, 1u);
    passed &= expect_u32("lane 5 handoffs", model.lane5_handoffs, 1u);
    passed &= expect_u32("lane 5 busy skips", model.lane5_busy_skips, 1u);
    passed &= expect_u32("lane 5 cleared", model.lane5_active, 0u);

    passed &= expect_u32(
        "null model",
        recomp_cri_service_run_lane2(NULL, count_step, &test),
        RECOMP_CRI_SERVICE_INVALID);
    passed &= expect_u32(
        "null callback",
        recomp_cri_service_run_lane5(&model, NULL, &test),
        RECOMP_CRI_SERVICE_INVALID);
    return passed;
}
