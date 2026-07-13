#include "stream_parser.h"

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
    uint8_t payloads[8][STREAM_PARSER_MAX_PAYLOAD];
    size_t lengths[8];
    size_t count;
} collector_t;

static bool collect_frame(
    const uint8_t *payload,
    size_t length,
    void *context)
{
    collector_t *collector = context;

    if (collector == NULL || collector->count >= 8u ||
        length > STREAM_PARSER_MAX_PAYLOAD) {
        return false;
    }
    memcpy(collector->payloads[collector->count], payload, length);
    collector->lengths[collector->count] = length;
    ++collector->count;
    return true;
}

static size_t encode_frame(
    uint8_t *output,
    const uint8_t *payload,
    size_t length)
{
    output[0] = (uint8_t)(length >> 8u);
    output[1] = (uint8_t)length;
    memcpy(&output[2], payload, length);
    return length + 2u;
}

static bool collector_has(
    const collector_t *collector,
    size_t index,
    const uint8_t *payload,
    size_t length)
{
    return index < collector->count && collector->lengths[index] == length &&
           memcmp(collector->payloads[index], payload, length) == 0;
}

static bool test_frame_survives_every_split(void)
{
    static const uint8_t payload[] = {
        UINT8_C(0x10), UINT8_C(0x20), UINT8_C(0x30),
        UINT8_C(0x40), UINT8_C(0x50),
    };
    uint8_t frame[2u + sizeof(payload)];
    const size_t frame_length =
        encode_frame(frame, payload, sizeof(payload));

    for (size_t split = 0u; split <= frame_length; ++split) {
        stream_parser_t parser;
        collector_t collector = {{{0}}, {0}, 0u};

        stream_parser_init(&parser);
        CHECK(stream_parser_feed(
                  &parser, frame, split, collect_frame, &collector) ==
              STREAM_OK);
        CHECK(stream_parser_feed(
                  &parser, &frame[split], frame_length - split,
                  collect_frame, &collector) == STREAM_OK);
        CHECK(stream_parser_finish(&parser) == STREAM_OK);
        CHECK(collector.count == 1u);
        CHECK(collector_has(&collector, 0u, payload, sizeof(payload)));
    }

    (void)printf("    evidence: %zu two-chunk split points\n",
                 frame_length + 1u);
    return true;
}

static bool test_one_byte_chunks(void)
{
    static const uint8_t payload[] = "fragmented";
    uint8_t frame[2u + sizeof(payload) - 1u];
    const size_t frame_length =
        encode_frame(frame, payload, sizeof(payload) - 1u);
    stream_parser_t parser;
    collector_t collector = {{{0}}, {0}, 0u};

    stream_parser_init(&parser);
    for (size_t i = 0u; i < frame_length; ++i) {
        CHECK(stream_parser_feed(
                  &parser, &frame[i], 1u, collect_frame, &collector) ==
              STREAM_OK);
    }
    CHECK(stream_parser_finish(&parser) == STREAM_OK);
    CHECK(collector_has(
        &collector, 0u, payload, sizeof(payload) - 1u));
    return true;
}

static bool test_coalesced_frames(void)
{
    static const uint8_t first[] = "first";
    static const uint8_t second[] = {0u, 1u, 2u, 3u};
    uint8_t stream[2u + sizeof(first) - 1u + 2u + sizeof(second)];
    size_t length = 0u;
    stream_parser_t parser;
    collector_t collector = {{{0}}, {0}, 0u};

    length += encode_frame(
        &stream[length], first, sizeof(first) - 1u);
    length += encode_frame(&stream[length], second, sizeof(second));

    stream_parser_init(&parser);
    CHECK(stream_parser_feed(
              &parser, stream, length, collect_frame, &collector) ==
          STREAM_OK);
    CHECK(stream_parser_finish(&parser) == STREAM_OK);
    CHECK(collector.count == 2u);
    CHECK(collector_has(&collector, 0u, first, sizeof(first) - 1u));
    CHECK(collector_has(&collector, 1u, second, sizeof(second)));
    return true;
}

static bool test_invalid_lengths_fail_closed(void)
{
    const uint8_t zero_length[] = {0u, 0u};
    const uint8_t oversized[] = {
        0u, (uint8_t)(STREAM_PARSER_MAX_PAYLOAD + 1u), 0u,
    };
    stream_parser_t parser;
    collector_t collector = {{{0}}, {0}, 0u};

    stream_parser_init(&parser);
    CHECK(stream_parser_feed(
              &parser, zero_length, sizeof(zero_length), collect_frame,
              &collector) == STREAM_INVALID_LENGTH);
    CHECK(stream_parser_feed(
              &parser, zero_length, sizeof(zero_length), collect_frame,
              &collector) == STREAM_FAILED_STATE);

    stream_parser_init(&parser);
    CHECK(stream_parser_feed(
              &parser, oversized, sizeof(oversized), collect_frame,
              &collector) == STREAM_INVALID_LENGTH);
    CHECK(collector.count == 0u);
    return true;
}

static bool test_finish_detects_truncation(void)
{
    static const uint8_t incomplete_header[] = {0u};
    static const uint8_t incomplete_payload[] = {0u, 3u, 1u, 2u};
    stream_parser_t parser;
    collector_t collector = {{{0}}, {0}, 0u};

    stream_parser_init(&parser);
    CHECK(stream_parser_feed(
              &parser, incomplete_header, sizeof(incomplete_header),
              collect_frame, &collector) == STREAM_OK);
    CHECK(stream_parser_finish(&parser) == STREAM_TRUNCATED);

    stream_parser_init(&parser);
    CHECK(stream_parser_feed(
              &parser, incomplete_payload, sizeof(incomplete_payload),
              collect_frame, &collector) == STREAM_OK);
    CHECK(stream_parser_finish(&parser) == STREAM_TRUNCATED);
    CHECK(collector.count == 0u);
    return true;
}

static bool reject_frame(
    const uint8_t *payload,
    size_t length,
    void *context)
{
    (void)payload;
    (void)length;
    (void)context;
    return false;
}

static bool test_callback_failure_is_sticky(void)
{
    static const uint8_t frame[] = {0u, 1u, UINT8_C(0xaa)};
    stream_parser_t parser;

    stream_parser_init(&parser);
    CHECK(stream_parser_feed(
              &parser, frame, sizeof(frame), reject_frame, NULL) ==
          STREAM_CALLBACK_REJECTED);
    CHECK(stream_parser_feed(
              &parser, frame, sizeof(frame), reject_frame, NULL) ==
          STREAM_FAILED_STATE);
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
        {"frame survives every split", test_frame_survives_every_split},
        {"one-byte chunks are accepted", test_one_byte_chunks},
        {"coalesced frames remain separate", test_coalesced_frames},
        {"invalid lengths fail closed", test_invalid_lengths_fail_closed},
        {"finish detects truncation", test_finish_detects_truncation},
        {"callback failure is sticky", test_callback_failure_is_sticky},
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
