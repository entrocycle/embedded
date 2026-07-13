#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    RB_OK = 0,
    RB_EMPTY,
    RB_FULL,
    RB_INVALID_ARGUMENT,
} rb_status_t;

typedef struct {
    uint8_t *storage;
    size_t storage_size;
    size_t head;
    size_t tail;
} ring_buffer_t;

rb_status_t ring_buffer_init(
    ring_buffer_t *buffer,
    uint8_t *storage,
    size_t storage_size);
void ring_buffer_clear(ring_buffer_t *buffer);
size_t ring_buffer_size(const ring_buffer_t *buffer);
size_t ring_buffer_capacity(const ring_buffer_t *buffer);
bool ring_buffer_is_empty(const ring_buffer_t *buffer);
bool ring_buffer_is_full(const ring_buffer_t *buffer);
rb_status_t ring_buffer_push(ring_buffer_t *buffer, uint8_t value);
rb_status_t ring_buffer_pop(ring_buffer_t *buffer, uint8_t *value);
rb_status_t ring_buffer_peek(const ring_buffer_t *buffer, uint8_t *value);

#endif
