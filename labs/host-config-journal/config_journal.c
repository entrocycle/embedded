#include "config_journal.h"

#include <limits.h>
#include <string.h>

#define JOURNAL_MAGIC UINT32_C(0x43464731)
#define JOURNAL_COMMIT UINT32_C(0xC01117ED)
#define JOURNAL_KNOWN_FLAGS UINT32_C(0x00000003)
#define JOURNAL_RECORD_DATA_SIZE JOURNAL_CRC_OFFSET
#define JOURNAL_RECORD_SIZE (JOURNAL_COMMIT_OFFSET + sizeof(uint32_t))

static void write_u32_le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value >> 0u);
    destination[1] = (uint8_t)(value >> 8u);
    destination[2] = (uint8_t)(value >> 16u);
    destination[3] = (uint8_t)(value >> 24u);
}

static uint32_t read_u32_le(const uint8_t *source)
{
    return ((uint32_t)source[0] << 0u) |
           ((uint32_t)source[1] << 8u) |
           ((uint32_t)source[2] << 16u) |
           ((uint32_t)source[3] << 24u);
}

static int32_t decode_i32(uint32_t bits)
{
    const uint32_t sign_boundary = UINT32_MAX / 2u + 1u;

    if (bits < sign_boundary) {
        return (int32_t)bits;
    }

    return (int32_t)((int64_t)INT32_MIN +
                     (int64_t)(bits - sign_boundary));
}

static uint32_t crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_MAX;

    for (size_t i = 0u; i < length; ++i) {
        crc ^= data[i];
        for (unsigned int bit = 0u; bit < CHAR_BIT; ++bit) {
            const uint32_t low_bit = crc & UINT32_C(1);
            crc >>= 1u;
            if (low_bit != 0u) {
                crc ^= UINT32_C(0xEDB88320);
            }
        }
    }

    return ~crc;
}

static bool config_is_valid(const journal_config_t *config)
{
    if (config == NULL) {
        return false;
    }

    return config->report_interval_ms >= UINT32_C(100) &&
           config->report_interval_ms <= UINT32_C(86400000) &&
           config->alarm_threshold_milli >= INT32_C(-100000) &&
           config->alarm_threshold_milli <= INT32_C(200000) &&
           (config->flags & ~JOURNAL_KNOWN_FLAGS) == 0u;
}

static journal_status_t flash_checkpoint(journal_flash_t *flash)
{
    ++flash->primitive_steps;
    if (flash->cut_after_step != 0u &&
        flash->primitive_steps == flash->cut_after_step) {
        return JOURNAL_POWER_CUT;
    }
    return JOURNAL_OK;
}

static journal_status_t flash_erase_slot(journal_flash_t *flash, size_t slot)
{
    if (flash == NULL || slot >= JOURNAL_SLOT_COUNT) {
        return JOURNAL_INVALID_ARGUMENT;
    }

    memset(&flash->bytes[slot * JOURNAL_SLOT_SIZE], 0xFF, JOURNAL_SLOT_SIZE);
    return flash_checkpoint(flash);
}

static journal_status_t flash_program_byte(
    journal_flash_t *flash,
    size_t address,
    uint8_t value)
{
    uint8_t old_value;

    if (flash == NULL || address >= JOURNAL_FLASH_SIZE) {
        return JOURNAL_INVALID_ARGUMENT;
    }

    old_value = flash->bytes[address];
    if ((old_value & value) != value) {
        return JOURNAL_FLASH_ERROR;
    }

    flash->bytes[address] = (uint8_t)(old_value & value);
    return flash_checkpoint(flash);
}

static journal_status_t flash_program_range(
    journal_flash_t *flash,
    size_t address,
    const uint8_t *data,
    size_t length)
{
    if (data == NULL || length > JOURNAL_FLASH_SIZE ||
        address > JOURNAL_FLASH_SIZE - length) {
        return JOURNAL_INVALID_ARGUMENT;
    }

    for (size_t i = 0u; i < length; ++i) {
        journal_status_t status = flash_program_byte(flash, address + i, data[i]);
        if (status != JOURNAL_OK) {
            return status;
        }
    }

    return JOURNAL_OK;
}

void journal_flash_init(journal_flash_t *flash)
{
    if (flash == NULL) {
        return;
    }

    memset(flash->bytes, 0xFF, sizeof(flash->bytes));
    flash->primitive_steps = 0u;
    flash->cut_after_step = 0u;
}

void journal_flash_cut_after(journal_flash_t *flash, size_t primitive_step)
{
    if (flash == NULL) {
        return;
    }

    flash->primitive_steps = 0u;
    flash->cut_after_step = primitive_step;
}

size_t journal_flash_steps(const journal_flash_t *flash)
{
    return flash == NULL ? 0u : flash->primitive_steps;
}

journal_status_t journal_flash_force_clear_bits(
    journal_flash_t *flash,
    size_t slot,
    size_t record_offset,
    uint8_t mask)
{
    size_t address;

    if (flash == NULL || slot >= JOURNAL_SLOT_COUNT ||
        record_offset >= JOURNAL_SLOT_SIZE || mask == 0u) {
        return JOURNAL_INVALID_ARGUMENT;
    }

    address = slot * JOURNAL_SLOT_SIZE + record_offset;
    flash->bytes[address] = (uint8_t)(flash->bytes[address] & (uint8_t)~mask);
    return JOURNAL_OK;
}

bool journal_sequence_is_newer(uint32_t candidate, uint32_t reference)
{
    const uint32_t distance = candidate - reference;

    /* This modular comparison is unambiguous while live records differ by < 2^31. */
    return distance != 0u && distance < (UINT32_MAX / 2u + 1u);
}

journal_slot_info_t journal_inspect_slot(
    const journal_flash_t *flash,
    size_t slot)
{
    journal_slot_info_t info = {0};
    const uint8_t *record;
    uint32_t stored_crc;
    uint32_t calculated_crc;

    if (flash == NULL || slot >= JOURNAL_SLOT_COUNT) {
        return info;
    }

    record = &flash->bytes[slot * JOURNAL_SLOT_SIZE];
    if (read_u32_le(&record[JOURNAL_MAGIC_OFFSET]) != JOURNAL_MAGIC ||
        read_u32_le(&record[JOURNAL_COMMIT_OFFSET]) != JOURNAL_COMMIT) {
        return info;
    }

    stored_crc = read_u32_le(&record[JOURNAL_CRC_OFFSET]);
    calculated_crc = crc32(record, JOURNAL_RECORD_DATA_SIZE);
    if (stored_crc != calculated_crc) {
        return info;
    }

    info.sequence = read_u32_le(&record[JOURNAL_SEQUENCE_OFFSET]);
    info.config.report_interval_ms = read_u32_le(&record[JOURNAL_CONFIG_OFFSET]);
    info.config.alarm_threshold_milli =
        decode_i32(read_u32_le(&record[JOURNAL_CONFIG_OFFSET + 4u]));
    info.config.flags = read_u32_le(&record[JOURNAL_CONFIG_OFFSET + 8u]);
    info.valid = config_is_valid(&info.config);
    return info;
}

static bool find_latest_slot(
    const journal_flash_t *flash,
    size_t *slot,
    journal_slot_info_t *info)
{
    journal_slot_info_t slots[JOURNAL_SLOT_COUNT];

    slots[0] = journal_inspect_slot(flash, 0u);
    slots[1] = journal_inspect_slot(flash, 1u);

    if (!slots[0].valid && !slots[1].valid) {
        return false;
    }

    if (slots[0].valid &&
        (!slots[1].valid ||
         journal_sequence_is_newer(slots[0].sequence, slots[1].sequence))) {
        *slot = 0u;
        *info = slots[0];
    } else {
        *slot = 1u;
        *info = slots[1];
    }
    return true;
}

journal_status_t journal_load(
    const journal_flash_t *flash,
    journal_config_t *config,
    uint32_t *sequence)
{
    size_t latest_slot = 0u;
    journal_slot_info_t latest = {0};

    if (flash == NULL || config == NULL) {
        return JOURNAL_INVALID_ARGUMENT;
    }

    if (!find_latest_slot(flash, &latest_slot, &latest)) {
        return JOURNAL_NOT_FOUND;
    }

    (void)latest_slot;
    *config = latest.config;
    if (sequence != NULL) {
        *sequence = latest.sequence;
    }
    return JOURNAL_OK;
}

journal_status_t journal_store(
    journal_flash_t *flash,
    const journal_config_t *config)
{
    uint8_t record[JOURNAL_RECORD_SIZE];
    uint8_t commit[sizeof(uint32_t)];
    size_t latest_slot = 0u;
    journal_slot_info_t latest = {0};
    bool has_latest;
    size_t target_slot;
    size_t target_address;
    uint32_t next_sequence;
    uint32_t record_crc;
    journal_status_t status;

    if (flash == NULL || !config_is_valid(config)) {
        return JOURNAL_INVALID_ARGUMENT;
    }

    has_latest = find_latest_slot(flash, &latest_slot, &latest);
    target_slot = has_latest ? 1u - latest_slot : 0u;
    next_sequence = has_latest ? latest.sequence + 1u : 1u;
    target_address = target_slot * JOURNAL_SLOT_SIZE;

    memset(record, 0xFF, sizeof(record));
    write_u32_le(&record[JOURNAL_MAGIC_OFFSET], JOURNAL_MAGIC);
    write_u32_le(&record[JOURNAL_SEQUENCE_OFFSET], next_sequence);
    write_u32_le(&record[JOURNAL_CONFIG_OFFSET], config->report_interval_ms);
    write_u32_le(
        &record[JOURNAL_CONFIG_OFFSET + 4u],
        (uint32_t)config->alarm_threshold_milli);
    write_u32_le(&record[JOURNAL_CONFIG_OFFSET + 8u], config->flags);
    record_crc = crc32(record, JOURNAL_RECORD_DATA_SIZE);
    write_u32_le(&record[JOURNAL_CRC_OFFSET], record_crc);
    write_u32_le(commit, JOURNAL_COMMIT);

    status = flash_erase_slot(flash, target_slot);
    if (status != JOURNAL_OK) {
        return status;
    }

    /* Data and CRC become durable before the commit marker is touched. */
    status = flash_program_range(
        flash, target_address, record, JOURNAL_COMMIT_OFFSET);
    if (status != JOURNAL_OK) {
        return status;
    }

    return flash_program_range(
        flash,
        target_address + JOURNAL_COMMIT_OFFSET,
        commit,
        sizeof(commit));
}

const char *journal_status_string(journal_status_t status)
{
    switch (status) {
    case JOURNAL_OK:
        return "ok";
    case JOURNAL_NOT_FOUND:
        return "not_found";
    case JOURNAL_INVALID_ARGUMENT:
        return "invalid_argument";
    case JOURNAL_FLASH_ERROR:
        return "flash_error";
    case JOURNAL_POWER_CUT:
        return "power_cut";
    }
    return "unknown";
}
