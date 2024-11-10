/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>

#define AMBIENT_TEMP_MIN        ( 0)
#define AMBIENT_TEMP_MAX       (100)
#define AMBIENT_HUMIDITY_MIN    ( 0)
#define AMBIENT_HUMIDITY_MAX   (100)

#define AMBIENT_HOT_THREASHOLD  (25)
#define AMBIENT_COLD_THREASHOLD (18)
#define AMBIENT_WET_THREASHOLD  (90)
#define AMBIENT_DRY_THREASHOLD  (70)

typedef enum {
    AMBIENT_HOT_WET,
    AMBIENT_HOT_NORMAL,
    AMBIENT_HOT_DRY,
    AMBIENT_NORMAL_WET,
    AMBIENT_NORMAL_NORMAL,
    AMBIENT_NORMAL_DRY,
    AMBIENT_COLD_WET,
    AMBIENT_COLD_NORMAL,
    AMBIENT_COLD_DRY,

    NUM_AMBIENT_STATES
} AMBIENT_STATE;

// MOCK VERSION OF THE FUNCTION UNDER UNIT TESTING
AMBIENT_STATE mock_calc_ambient_state(int32_t t, int32_t h) {
    if        (t >= AMBIENT_HOT_THREASHOLD  && h >= AMBIENT_WET_THREASHOLD) {
        return AMBIENT_HOT_WET;
    } else if (t >= AMBIENT_HOT_THREASHOLD  && h >= AMBIENT_DRY_THREASHOLD) {
        return AMBIENT_HOT_NORMAL;
    } else if (t >= AMBIENT_HOT_THREASHOLD  && h <  AMBIENT_DRY_THREASHOLD) {
        return AMBIENT_HOT_DRY;
    } else if (t >= AMBIENT_COLD_THREASHOLD && h >= AMBIENT_WET_THREASHOLD) {
        return AMBIENT_NORMAL_WET;
    } else if (t >= AMBIENT_COLD_THREASHOLD && h >= AMBIENT_DRY_THREASHOLD) {
        return AMBIENT_NORMAL_NORMAL;
    } else if (t >= AMBIENT_COLD_THREASHOLD && h <  AMBIENT_DRY_THREASHOLD) {
        return AMBIENT_NORMAL_DRY;
    } else if (t <  AMBIENT_COLD_THREASHOLD && h >= AMBIENT_WET_THREASHOLD) {
        return AMBIENT_COLD_WET;
    } else if (t <  AMBIENT_COLD_THREASHOLD && h >= AMBIENT_DRY_THREASHOLD) {
        return AMBIENT_COLD_NORMAL;
    } else if (t <  AMBIENT_COLD_THREASHOLD && h <  AMBIENT_DRY_THREASHOLD) {
        return AMBIENT_COLD_DRY;
    } else {
        return AMBIENT_NORMAL_NORMAL;
    }
}

ZTEST_SUITE(framework_tests, NULL, NULL, NULL, NULL, NULL);

/**
 * @brief Test Asserts
 *
 * This test verifies various assert macros provided by ztest.
 *
 */
ZTEST(framework_tests, test_assert)
{
    int t, h;
    AMBIENT_STATE state;
    // DRY
    // and COLD
    for(t=AMBIENT_TEMP_MIN; t<AMBIENT_COLD_THREASHOLD; t++) {
        for(h=AMBIENT_HUMIDITY_MIN; h<AMBIENT_DRY_THREASHOLD; h++) {
            state = mock_calc_ambient_state(t, h);
            zassert_true(state == AMBIENT_COLD_DRY, "With %d T and %d H the state is not AMBIENT_COLD_DRY as aspected", t, h);
        }
    }
    // and NORMAL
    for(t=AMBIENT_COLD_THREASHOLD; t<AMBIENT_HOT_THREASHOLD; t++) {
        for(h=AMBIENT_HUMIDITY_MIN; h<AMBIENT_DRY_THREASHOLD; h++) {
            state = mock_calc_ambient_state(t, h);
            zassert_true(state == AMBIENT_NORMAL_DRY, "With %d T and %d H the state is not AMBIENT_NORMAL_DRY as aspected", t, h);
        }
    }
    // and HOT
    for(t=AMBIENT_HOT_THREASHOLD; t<AMBIENT_TEMP_MAX; t++) {
        for(h=AMBIENT_HUMIDITY_MIN; h<AMBIENT_DRY_THREASHOLD; h++) {
            state = mock_calc_ambient_state(t, h);
            zassert_true(state == AMBIENT_HOT_DRY, "With %d T and %d H the state is not AMBIENT_HOT_DRY as aspected", t, h);
        }
    }

    // NORMAL
    // and COLD
    for(t=AMBIENT_TEMP_MIN; t<AMBIENT_COLD_THREASHOLD; t++) {
        for(h=AMBIENT_DRY_THREASHOLD; h<AMBIENT_WET_THREASHOLD; h++) {
            state = mock_calc_ambient_state(t, h);
            zassert_true(state == AMBIENT_COLD_NORMAL, "With %d T and %d H the state is not AMBIENT_COLD_NORMAL as aspected", t, h);
        }
    }
    // and NORMAL
    for(t=AMBIENT_COLD_THREASHOLD; t<AMBIENT_HOT_THREASHOLD; t++) {
        for(h=AMBIENT_DRY_THREASHOLD; h<AMBIENT_WET_THREASHOLD; h++) {
            state = mock_calc_ambient_state(t, h);
            zassert_true(state == AMBIENT_NORMAL_NORMAL, "With %d T and %d H the state is not AMBIENT_NORMAL_NORMAL as aspected", t, h);
        }
    }
    // and HOT
    for(t=AMBIENT_HOT_THREASHOLD; t<AMBIENT_TEMP_MAX; t++) {
        for(h=AMBIENT_DRY_THREASHOLD; h<AMBIENT_WET_THREASHOLD; h++) {
            state = mock_calc_ambient_state(t, h);
            zassert_true(state == AMBIENT_HOT_NORMAL, "With %d T and %d H the state is not AMBIENT_HOT_NORMAL as aspected", t, h);
        }
    }
    
    // WET
    // and COLD
    for(t=AMBIENT_TEMP_MIN; t<AMBIENT_COLD_THREASHOLD; t++) {
        for(h=AMBIENT_WET_THREASHOLD; h<AMBIENT_HUMIDITY_MAX; h++) {
            state = mock_calc_ambient_state(t, h);
            zassert_true(state == AMBIENT_COLD_WET, "With %d T and %d H the state is not AMBIENT_COLD_WET as aspected", t, h);
        }
    }
    // and NORMAL
    for(t=AMBIENT_COLD_THREASHOLD; t<AMBIENT_HOT_THREASHOLD; t++) {
        for(h=AMBIENT_WET_THREASHOLD; h<AMBIENT_HUMIDITY_MAX; h++) {
            state = mock_calc_ambient_state(t, h);
            zassert_true(state == AMBIENT_NORMAL_WET, "With %d T and %d H the state is not AMBIENT_NORMAL_WET as aspected", t, h);
        }
    }
    // and HOT
    for(t=AMBIENT_HOT_THREASHOLD; t<AMBIENT_TEMP_MAX; t++) {
        for(h=AMBIENT_WET_THREASHOLD; h<AMBIENT_HUMIDITY_MAX; h++) {
            state = mock_calc_ambient_state(t, h);
            zassert_true(state == AMBIENT_HOT_WET, "With %d T and %d H the state is not AMBIENT_HOT_WET as aspected", t, h);
        }
    }
}
