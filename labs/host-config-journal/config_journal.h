#ifndef CONFIG_JOURNAL_H
#define CONFIG_JOURNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define JOURNAL_SLOT_COUNT 2u
#define JOURNAL_SLOT_SIZE 32u
#define JOURNAL_FLASH_SIZE (JOURNAL_SLOT_COUNT * JOURNAL_SLOT_SIZE)

/* Byte offsets are public so tests and learners can inspect the wire format. */
#define JOURNAL_MAGIC_OFFSET 0u
#define JOURNAL_SEQUENCE_OFFSET 4u
#define JOURNAL_CONFIG_OFFSET 8u
#define JOURNAL_CRC_OFFSET 20u
#define JOURNAL_COMMIT_OFFSET 24u

typedef struct {
    uint32_t report_interval_ms;
    int32_t alarm_threshold_milli;
    uint32_t flags;
} journal_config_t;

typedef enum {
    JOURNAL_OK = 0,
    JOURNAL_NOT_FOUND,
    JOURNAL_INVALID_ARGUMENT,
    JOURNAL_FLASH_ERROR,
    JOURNAL_POWER_CUT
} journal_status_t;

typedef struct {
    uint8_t bytes[JOURNAL_FLASH_SIZE];
    size_t primitive_steps;
    size_t cut_after_step;
} journal_flash_t;

typedef struct {
    bool valid;
    uint32_t sequence;
    journal_config_t config;
} journal_slot_info_t;

void journal_flash_init(journal_flash_t *flash);
void journal_flash_cut_after(journal_flash_t *flash, size_t primitive_step);
size_t journal_flash_steps(const journal_flash_t *flash);

/* Test-only corruption helper: a real flash fault can also clear a stored bit. */
journal_status_t journal_flash_force_clear_bits(
    journal_flash_t *flash,
    size_t slot,
    size_t record_offset,
    uint8_t mask);

journal_status_t journal_load(
    const journal_flash_t *flash,
    journal_config_t *config,
    uint32_t *sequence);

journal_status_t journal_store(
    journal_flash_t *flash,
    const journal_config_t *config);

journal_slot_info_t journal_inspect_slot(
    const journal_flash_t *flash,
    size_t slot);

bool journal_sequence_is_newer(uint32_t candidate, uint32_t reference);
const char *journal_status_string(journal_status_t status);

#endif
