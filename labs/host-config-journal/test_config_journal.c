#include "config_journal.h"

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

static const journal_config_t OLD_CONFIG = {
    .report_interval_ms = UINT32_C(1000),
    .alarm_threshold_milli = INT32_C(42000),
    .flags = UINT32_C(1),
};

static const journal_config_t NEW_CONFIG = {
    .report_interval_ms = UINT32_C(5000),
    .alarm_threshold_milli = INT32_C(-12500),
    .flags = UINT32_C(3),
};

static const journal_config_t INTERMEDIATE_CONFIG = {
    .report_interval_ms = UINT32_C(2000),
    .alarm_threshold_milli = INT32_C(18000),
    .flags = UINT32_C(2),
};

static bool config_equal(
    const journal_config_t *left,
    const journal_config_t *right)
{
    return left->report_interval_ms == right->report_interval_ms &&
           left->alarm_threshold_milli == right->alarm_threshold_milli &&
           left->flags == right->flags;
}

static bool test_erased_flash_is_empty(void)
{
    journal_flash_t flash;
    journal_config_t loaded = {0};

    journal_flash_init(&flash);
    CHECK(journal_load(&flash, &loaded, NULL) == JOURNAL_NOT_FOUND);
    CHECK(!journal_inspect_slot(&flash, 0u).valid);
    CHECK(!journal_inspect_slot(&flash, 1u).valid);
    return true;
}

static bool test_round_trip_and_rotation(void)
{
    journal_flash_t flash;
    journal_config_t loaded = {0};
    journal_config_t third = {
        .report_interval_ms = UINT32_C(30000),
        .alarm_threshold_milli = INT32_C(80000),
        .flags = UINT32_C(0),
    };
    uint32_t sequence = 0u;

    journal_flash_init(&flash);
    CHECK(journal_store(&flash, &OLD_CONFIG) == JOURNAL_OK);
    CHECK(journal_load(&flash, &loaded, &sequence) == JOURNAL_OK);
    CHECK(config_equal(&loaded, &OLD_CONFIG));
    CHECK(sequence == 1u);

    CHECK(journal_store(&flash, &NEW_CONFIG) == JOURNAL_OK);
    CHECK(journal_load(&flash, &loaded, &sequence) == JOURNAL_OK);
    CHECK(config_equal(&loaded, &NEW_CONFIG));
    CHECK(sequence == 2u);
    CHECK(journal_inspect_slot(&flash, 0u).valid);
    CHECK(journal_inspect_slot(&flash, 1u).valid);

    CHECK(journal_store(&flash, &third) == JOURNAL_OK);
    CHECK(journal_load(&flash, &loaded, &sequence) == JOURNAL_OK);
    CHECK(config_equal(&loaded, &third));
    CHECK(sequence == 3u);
    return true;
}

static bool test_invalid_input_does_not_erase(void)
{
    journal_flash_t flash;
    uint8_t before[JOURNAL_FLASH_SIZE];
    journal_config_t invalid = OLD_CONFIG;

    journal_flash_init(&flash);
    CHECK(journal_store(&flash, &OLD_CONFIG) == JOURNAL_OK);
    memcpy(before, flash.bytes, sizeof(before));

    invalid.report_interval_ms = 0u;
    CHECK(journal_store(&flash, &invalid) == JOURNAL_INVALID_ARGUMENT);
    CHECK(memcmp(before, flash.bytes, sizeof(before)) == 0);
    return true;
}

static bool test_crc_rejects_corrupted_newest_slot(void)
{
    journal_flash_t flash;
    journal_config_t loaded = {0};
    uint32_t sequence = 0u;

    journal_flash_init(&flash);
    CHECK(journal_store(&flash, &OLD_CONFIG) == JOURNAL_OK);
    CHECK(journal_store(&flash, &NEW_CONFIG) == JOURNAL_OK);
    CHECK(journal_inspect_slot(&flash, 1u).valid);

    CHECK(journal_flash_force_clear_bits(
              &flash, 1u, JOURNAL_CONFIG_OFFSET, UINT8_C(0x08)) == JOURNAL_OK);
    CHECK(!journal_inspect_slot(&flash, 1u).valid);
    CHECK(journal_load(&flash, &loaded, &sequence) == JOURNAL_OK);
    CHECK(config_equal(&loaded, &OLD_CONFIG));
    CHECK(sequence == 1u);
    return true;
}

static bool test_every_power_cut_during_first_store(void)
{
    journal_flash_t erased;
    journal_flash_t uninterrupted;
    size_t steps;
    size_t empty_count = 0u;
    size_t committed_count = 0u;

    journal_flash_init(&erased);
    uninterrupted = erased;
    journal_flash_cut_after(&uninterrupted, 0u);
    CHECK(journal_store(&uninterrupted, &NEW_CONFIG) == JOURNAL_OK);
    steps = journal_flash_steps(&uninterrupted);
    CHECK(steps > 0u);

    for (size_t cut = 1u; cut <= steps; ++cut) {
        journal_flash_t trial = erased;
        journal_config_t loaded = {0};
        journal_status_t load_status;

        journal_flash_cut_after(&trial, cut);
        CHECK(journal_store(&trial, &NEW_CONFIG) == JOURNAL_POWER_CUT);
        CHECK(journal_flash_steps(&trial) == cut);

        load_status = journal_load(&trial, &loaded, NULL);
        if (load_status == JOURNAL_NOT_FOUND) {
            ++empty_count;
        } else {
            CHECK(load_status == JOURNAL_OK);
            CHECK(config_equal(&loaded, &NEW_CONFIG));
            ++committed_count;
        }
    }

    CHECK(empty_count + committed_count == steps);
    CHECK(committed_count == 1u);
    (void)printf(
        "    evidence: %zu cut points, empty=%zu, committed=%zu\n",
        steps, empty_count, committed_count);
    return true;
}

static bool verify_every_power_cut_during_update(
    const journal_flash_t *base,
    const journal_config_t *old_config,
    const journal_config_t *new_config,
    const char *rotation)
{
    journal_flash_t uninterrupted = *base;
    size_t steps;
    size_t old_count = 0u;
    size_t new_count = 0u;

    journal_flash_cut_after(&uninterrupted, 0u);
    CHECK(journal_store(&uninterrupted, new_config) == JOURNAL_OK);
    steps = journal_flash_steps(&uninterrupted);
    CHECK(steps > 0u);

    for (size_t cut = 1u; cut <= steps; ++cut) {
        journal_flash_t trial = *base;
        journal_config_t loaded = {0};

        journal_flash_cut_after(&trial, cut);
        CHECK(journal_store(&trial, new_config) == JOURNAL_POWER_CUT);
        CHECK(journal_flash_steps(&trial) == cut);
        CHECK(journal_load(&trial, &loaded, NULL) == JOURNAL_OK);

        if (config_equal(&loaded, old_config)) {
            ++old_count;
        } else {
            CHECK(config_equal(&loaded, new_config));
            ++new_count;
        }
    }

    CHECK(old_count + new_count == steps);
    CHECK(new_count == 1u);
    (void)printf(
        "    evidence (%s): %zu cut points, old=%zu, new=%zu, invalid=0\n",
        rotation, steps, old_count, new_count);
    return true;
}

static bool test_every_power_cut_during_update(void)
{
    journal_flash_t base;

    journal_flash_init(&base);
    CHECK(journal_store(&base, &OLD_CONFIG) == JOURNAL_OK);
    CHECK(verify_every_power_cut_during_update(
        &base, &OLD_CONFIG, &NEW_CONFIG, "slot 0 -> slot 1"));

    CHECK(journal_store(&base, &INTERMEDIATE_CONFIG) == JOURNAL_OK);
    CHECK(verify_every_power_cut_during_update(
        &base, &INTERMEDIATE_CONFIG, &NEW_CONFIG, "slot 1 -> slot 0"));
    return true;
}

static bool test_sequence_wrap_comparison(void)
{
    CHECK(journal_sequence_is_newer(1u, 0u));
    CHECK(!journal_sequence_is_newer(0u, 1u));
    CHECK(journal_sequence_is_newer(0u, UINT32_MAX));
    CHECK(!journal_sequence_is_newer(UINT32_MAX, 0u));
    CHECK(!journal_sequence_is_newer(UINT32_C(7), UINT32_C(7)));
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
        {"erased flash is empty", test_erased_flash_is_empty},
        {"round trip and slot rotation", test_round_trip_and_rotation},
        {"invalid input preserves flash", test_invalid_input_does_not_erase},
        {"CRC falls back from corrupted newest slot",
         test_crc_rejects_corrupted_newest_slot},
        {"power cut after every first-store primitive",
         test_every_power_cut_during_first_store},
        {"power cut after every update primitive in both rotations",
         test_every_power_cut_during_update},
        {"sequence comparison handles wrap", test_sequence_wrap_comparison},
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
