/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools; based on CheshirCa / WeAct Display Tools Contributors.
 * See LICENSE (MIT).
 */
#include "weact_memory_transport.h"
#include <string.h>

static void advance(wx_memory_transport_t *m, uint64_t amount)
{
    m->time_ms = amount > UINT64_MAX - m->time_ms ? UINT64_MAX : m->time_ms + amount;
}
static uint64_t now(void *context)
{
    return ((wx_memory_transport_t *)context)->time_ms;
}
static wx_status_t write_memory(void *context, const uint8_t *p, size_t length,
                                size_t *written)
{
    wx_memory_transport_t *m = context;
    *written = 0;
    ++m->writes;
    advance(m, m->tick_ms);
    if (m->disconnected) return WX_DISCONNECTED;
    size_t limit = m->write_limit ? m->write_limit : length;
    if (m->script_index < m->script_length) {
        if (!m->script) return WX_IO;
        wx_memory_action_t action = m->script[m->script_index++];
        if (action.status != WX_OK) {
            if (action.status == WX_DISCONNECTED) m->disconnected = true;
            return action.status;
        }
        limit = action.limit;
    }
    if (length > limit) length = limit;
    if (!length) return WX_OK;
    if (m->length > m->capacity) return WX_IO;
    size_t room = m->capacity - m->length;
    if (!room) return WX_IO;
    if (length > room) length = room;
    memcpy(m->bytes + m->length, p, length);
    m->length += length;
    *written = length;
    return WX_OK;
}
static wx_status_t wait_memory(void *context, uint64_t end)
{
    wx_memory_transport_t *m = context;
    ++m->waits;
    advance(m, m->wait_ms);
    if (m->disconnected) return WX_DISCONNECTED;
    if (m->time_ms >= end) return WX_TIMEOUT;
    return m->wait_status;
}
wx_status_t wx_memory_transport_init(wx_memory_transport_t *m,
                                     uint8_t *storage, size_t capacity)
{
    if (!m || (!storage && capacity)) return WX_INVALID;
    *m = (wx_memory_transport_t){0};
    m->bytes = storage;
    m->capacity = capacity;
    m->tick_ms = 1;
    m->wait_ms = 1;
    return WX_OK;
}
wx_transport_t wx_memory_transport_bind(wx_memory_transport_t *m)
{
    if (!m) return (wx_transport_t){0};
    return (wx_transport_t){m, write_memory, now, wait_memory};
}
