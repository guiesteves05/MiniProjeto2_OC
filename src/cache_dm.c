/* STUDENT FILE: Part 1 - Direct-mapped cache.
 * Implement only this mode here; cache.c supplies initialization/dispatch.
 * Keep helpers static and in this file. Do not call another student module.
 * Record CPU and lookup events through stats_record(). Memory transfers
 * record their own events automatically; never record those twice.
 */
#include "cache_impl.h"
#include "memory.h"
#include "stats.h"

static uint32_t line_base_address(uint32_t tag, uint32_t index) {
    uint32_t block_number = tag * L1_LINES + index;
    uint32_t block_base = block_number * BLOCK_SIZE;
    return block_base;
}

static L1Line *dm_prepare_line(Cache *cache, uint32_t address) {
    uint32_t block_number = address / BLOCK_SIZE;
    uint32_t index = block_number % L1_LINES;
    uint32_t tag = block_number / L1_LINES;
    L1Line *line = &cache->l1[index];

    if (line->valid && line->tag == tag) {
        stats_record(EVENT_L1_HIT);
        return line;
    } else stats_record(EVENT_L1_MISS);

    if (line->valid && line->dirty) {
        memory_write_block(line_base_address(line->tag, index), line->data);
    }

    memory_read_block(block_number * BLOCK_SIZE, line->data);
    line->valid = true;
    line->dirty = false;
    line->tag = tag;

    return line;
} 


uint8_t dm_read(Cache *cache, uint32_t address)
{
    /* TODO: read one byte using this mode's mapping/replacement rules.
     * Emit one CPU_READ event and the appropriate lookup events.
     */

    stats_record(EVENT_CPU_READ);
    L1Line *line = dm_prepare_line(cache, address);
    return line->data[address % BLOCK_SIZE];
}

void dm_write(Cache *cache, uint32_t address, uint8_t value)
{
    /* TODO: write-back/write-allocate for this mode.
     * Emit one CPU_WRITE event and the appropriate lookup events.
     */

    stats_record(EVENT_CPU_WRITE);
    L1Line *line = dm_prepare_line(cache, address);
    line->data[address % BLOCK_SIZE] = value;
    line->dirty = true;
}

void dm_flush(Cache *cache)
{
    /* TODO: flush this mode without invalidation or recency changes.
     * Emit no CPU/lookup events. Memory writes record their own events.
     */

    for (uint32_t index = 0; index < L1_LINES; ++index) {
        L1Line *line = &cache->l1[index];
        if (line->valid && line->dirty) {
            memory_write_block(line_base_address(line->tag, index), line->data);
            line->dirty = false;
        }
    }

}
