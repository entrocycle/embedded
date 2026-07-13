#ifndef STREAM_PARSER_H
#define STREAM_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STREAM_PARSER_MAX_PAYLOAD 64u

typedef enum {
    STREAM_OK = 0,
    STREAM_INVALID_ARGUMENT,
    STREAM_INVALID_LENGTH,
    STREAM_CALLBACK_REJECTED,
    STREAM_TRUNCATED,
    STREAM_FAILED_STATE,
} stream_status_t;

typedef bool (*stream_frame_callback_t)(
    const uint8_t *payload,
    size_t length,
    void *context);

typedef struct {
    uint8_t header[2];
    size_t header_used;
    uint8_t payload[STREAM_PARSER_MAX_PAYLOAD];
    size_t payload_used;
    size_t expected_payload;
    size_t frames_delivered;
    bool failed;
} stream_parser_t;

void stream_parser_init(stream_parser_t *parser);
stream_status_t stream_parser_feed(
    stream_parser_t *parser,
    const uint8_t *data,
    size_t length,
    stream_frame_callback_t callback,
    void *context);
stream_status_t stream_parser_finish(const stream_parser_t *parser);

#endif
