#include "ring_buffer.h"

static bool ring_buffer_is_valid(const ring_buffer_t *buffer)
{
    return buffer != NULL && buffer->storage != NULL &&
           buffer->storage_size >= 2u && buffer->head < buffer->storage_size &&
           buffer->tail < buffer->storage_size;
}

static size_t next_index(const ring_buffer_t *buffer, size_t index)
{
    ++index;
    return index == buffer->storage_size ? 0u : index;
}

rb_status_t ring_buffer_init(
    ring_buffer_t *buffer,
    uint8_t *storage,
    size_t storage_size)
{
    if (buffer == NULL || storage == NULL || storage_size < 2u) {
        return RB_INVALID_ARGUMENT;
    }

    buffer->storage = storage;
    buffer->storage_size = storage_size;
    buffer->head = 0u;
    buffer->tail = 0u;
    return RB_OK;
}

void ring_buffer_clear(ring_buffer_t *buffer)
{
    if (!ring_buffer_is_valid(buffer)) {
        return;
    }

    buffer->head = 0u;
    buffer->tail = 0u;
}

size_t ring_buffer_size(const ring_buffer_t *buffer)
{
    if (!ring_buffer_is_valid(buffer)) {
        return 0u;
    }

    if (buffer->head >= buffer->tail) {
        return buffer->head - buffer->tail;
    }
    return buffer->storage_size - buffer->tail + buffer->head;
}

size_t ring_buffer_capacity(const ring_buffer_t *buffer)
{
    return ring_buffer_is_valid(buffer) ? buffer->storage_size - 1u : 0u;
}

bool ring_buffer_is_empty(const ring_buffer_t *buffer)
{
    return ring_buffer_is_valid(buffer) && buffer->head == buffer->tail;
}

bool ring_buffer_is_full(const ring_buffer_t *buffer)
{
    return ring_buffer_is_valid(buffer) &&
           next_index(buffer, buffer->head) == buffer->tail;
}

rb_status_t ring_buffer_push(ring_buffer_t *buffer, uint8_t value)
{
    size_t next;

    if (!ring_buffer_is_valid(buffer)) {
        return RB_INVALID_ARGUMENT;
    }

    next = next_index(buffer, buffer->head);
    if (next == buffer->tail) {
        return RB_FULL;
    }

    buffer->storage[buffer->head] = value;
    buffer->head = next;
    return RB_OK;
}

rb_status_t ring_buffer_pop(ring_buffer_t *buffer, uint8_t *value)
{
    if (!ring_buffer_is_valid(buffer) || value == NULL) {
        return RB_INVALID_ARGUMENT;
    }
    if (buffer->head == buffer->tail) {
        return RB_EMPTY;
    }

    *value = buffer->storage[buffer->tail];
    buffer->tail = next_index(buffer, buffer->tail);
    return RB_OK;
}

rb_status_t ring_buffer_peek(const ring_buffer_t *buffer, uint8_t *value)
{
    if (!ring_buffer_is_valid(buffer) || value == NULL) {
        return RB_INVALID_ARGUMENT;
    }
    if (buffer->head == buffer->tail) {
        return RB_EMPTY;
    }

    *value = buffer->storage[buffer->tail];
    return RB_OK;
}
