#include "stream_parser.h"

void stream_parser_init(stream_parser_t *parser)
{
    if (parser == NULL) {
        return;
    }

    parser->header[0] = 0u;
    parser->header[1] = 0u;
    parser->header_used = 0u;
    parser->payload_used = 0u;
    parser->expected_payload = 0u;
    parser->frames_delivered = 0u;
    parser->failed = false;
}

static stream_status_t fail(
    stream_parser_t *parser,
    stream_status_t status)
{
    parser->failed = true;
    return status;
}

stream_status_t stream_parser_feed(
    stream_parser_t *parser,
    const uint8_t *data,
    size_t length,
    stream_frame_callback_t callback,
    void *context)
{
    size_t offset = 0u;

    if (parser == NULL || callback == NULL ||
        (data == NULL && length != 0u)) {
        return STREAM_INVALID_ARGUMENT;
    }
    if (parser->failed) {
        return STREAM_FAILED_STATE;
    }

    while (offset < length) {
        if (parser->header_used < sizeof(parser->header)) {
            parser->header[parser->header_used++] = data[offset++];
            if (parser->header_used < sizeof(parser->header)) {
                continue;
            }

            parser->expected_payload =
                ((size_t)parser->header[0] << 8u) | parser->header[1];
            if (parser->expected_payload == 0u ||
                parser->expected_payload > STREAM_PARSER_MAX_PAYLOAD) {
                return fail(parser, STREAM_INVALID_LENGTH);
            }
        }

        while (offset < length &&
               parser->payload_used < parser->expected_payload) {
            parser->payload[parser->payload_used++] = data[offset++];
        }

        if (parser->payload_used == parser->expected_payload) {
            if (!callback(
                    parser->payload, parser->expected_payload, context)) {
                return fail(parser, STREAM_CALLBACK_REJECTED);
            }
            ++parser->frames_delivered;
            parser->header_used = 0u;
            parser->payload_used = 0u;
            parser->expected_payload = 0u;
        }
    }

    return STREAM_OK;
}

stream_status_t stream_parser_finish(const stream_parser_t *parser)
{
    if (parser == NULL) {
        return STREAM_INVALID_ARGUMENT;
    }
    if (parser->failed) {
        return STREAM_FAILED_STATE;
    }
    if (parser->header_used != 0u || parser->payload_used != 0u ||
        parser->expected_payload != 0u) {
        return STREAM_TRUNCATED;
    }
    return STREAM_OK;
}
