#include "ring_buffer.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            (void)fprintf(                                                      \
                stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,    \
                #condition);                                                    \
            return false;                                                       \
        }                                                                       \
    } while (false)

typedef struct {
    uint8_t values[32];
    size_t size;
} queue_model_t;

static bool test_invalid_arguments(void)
{
    ring_buffer_t buffer = {0};
    uint8_t storage[2] = {0};
    uint8_t value = 0u;

    CHECK(ring_buffer_init(NULL, storage, sizeof(storage)) ==
          RB_INVALID_ARGUMENT);
    CHECK(ring_buffer_init(&buffer, NULL, sizeof(storage)) ==
          RB_INVALID_ARGUMENT);
    CHECK(ring_buffer_init(&buffer, storage, 1u) == RB_INVALID_ARGUMENT);
    CHECK(ring_buffer_pop(&buffer, &value) == RB_INVALID_ARGUMENT);
    return true;
}

static bool test_empty_full_and_peek(void)
{
    ring_buffer_t buffer;
    uint8_t storage[4] = {0};
    uint8_t value = 0u;

    CHECK(ring_buffer_init(&buffer, storage, sizeof(storage)) == RB_OK);
    CHECK(ring_buffer_capacity(&buffer) == 3u);
    CHECK(ring_buffer_size(&buffer) == 0u);
    CHECK(ring_buffer_is_empty(&buffer));
    CHECK(ring_buffer_pop(&buffer, &value) == RB_EMPTY);

    CHECK(ring_buffer_push(&buffer, UINT8_C(10)) == RB_OK);
    CHECK(ring_buffer_push(&buffer, UINT8_C(20)) == RB_OK);
    CHECK(ring_buffer_push(&buffer, UINT8_C(30)) == RB_OK);
    CHECK(ring_buffer_is_full(&buffer));
    CHECK(ring_buffer_size(&buffer) == 3u);
    CHECK(ring_buffer_push(&buffer, UINT8_C(40)) == RB_FULL);
    CHECK(ring_buffer_peek(&buffer, &value) == RB_OK);
    CHECK(value == UINT8_C(10));
    CHECK(ring_buffer_size(&buffer) == 3u);
    return true;
}

static bool test_wrap_preserves_fifo_order(void)
{
    ring_buffer_t buffer;
    uint8_t storage[5] = {0};
    uint8_t value = 0u;

    CHECK(ring_buffer_init(&buffer, storage, sizeof(storage)) == RB_OK);
    for (uint8_t i = 1u; i <= 4u; ++i) {
        CHECK(ring_buffer_push(&buffer, i) == RB_OK);
    }
    CHECK(ring_buffer_pop(&buffer, &value) == RB_OK && value == 1u);
    CHECK(ring_buffer_pop(&buffer, &value) == RB_OK && value == 2u);
    CHECK(ring_buffer_push(&buffer, UINT8_C(5)) == RB_OK);
    CHECK(ring_buffer_push(&buffer, UINT8_C(6)) == RB_OK);

    for (uint8_t expected = 3u; expected <= 6u; ++expected) {
        CHECK(ring_buffer_pop(&buffer, &value) == RB_OK);
        CHECK(value == expected);
    }
    CHECK(ring_buffer_is_empty(&buffer));
    return true;
}

static uint32_t next_random(uint32_t *state)
{
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static bool test_against_queue_model(void)
{
    ring_buffer_t buffer;
    uint8_t storage[17] = {0};
    queue_model_t model = {{0}, 0u};
    uint32_t random_state = UINT32_C(0x5eed1234);
    size_t pushes = 0u;
    size_t pops = 0u;

    CHECK(ring_buffer_init(&buffer, storage, sizeof(storage)) == RB_OK);

    for (size_t step = 0u; step < 100000u; ++step) {
        const uint32_t random = next_random(&random_state);
        const bool choose_push = (random & 1u) != 0u;

        if (choose_push) {
            const uint8_t value = (uint8_t)(random >> 8u);
            const rb_status_t status = ring_buffer_push(&buffer, value);

            if (model.size == ring_buffer_capacity(&buffer)) {
                CHECK(status == RB_FULL);
            } else {
                CHECK(status == RB_OK);
                model.values[model.size++] = value;
                ++pushes;
            }
        } else {
            uint8_t actual = 0u;
            const rb_status_t status = ring_buffer_pop(&buffer, &actual);

            if (model.size == 0u) {
                CHECK(status == RB_EMPTY);
            } else {
                CHECK(status == RB_OK);
                CHECK(actual == model.values[0]);
                memmove(model.values, &model.values[1], model.size - 1u);
                --model.size;
                ++pops;
            }
        }

        CHECK(ring_buffer_size(&buffer) == model.size);
        CHECK(ring_buffer_is_empty(&buffer) == (model.size == 0u));
        CHECK(ring_buffer_is_full(&buffer) ==
              (model.size == ring_buffer_capacity(&buffer)));
    }

    (void)printf(
        "    evidence: 100000 operations, pushes=%zu, pops=%zu\n",
        pushes, pops);
    return true;
}

typedef bool (*test_function_t)(void);

typedef struct {
    const char *name;
    test_function_t function;
} test_case_t;

int main(void)
{
    static const test_case_t tests[] = {
        {"invalid arguments are rejected", test_invalid_arguments},
        {"empty, full and peek semantics", test_empty_full_and_peek},
        {"wrap preserves FIFO order", test_wrap_preserves_fifo_order},
        {"implementation matches queue model", test_against_queue_model},
    };
    size_t passed = 0u;

    for (size_t i = 0u; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (!tests[i].function()) {
            (void)fprintf(stderr, "[FAIL] %s\n", tests[i].name);
            return 1;
        }
        ++passed;
        (void)printf("[PASS] %s\n", tests[i].name);
    }

    (void)printf("\n%zu/%zu tests passed\n", passed,
                 sizeof(tests) / sizeof(tests[0]));
    return 0;
}
