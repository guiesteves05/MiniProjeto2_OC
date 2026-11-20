/* STUDENT FILE: Part 2 - Two-way set-associative cache.
 * Implement only this mode here; cache.c supplies initialization/dispatch.
 * Keep helpers static and in this file. Do not call another student module.
 * Record CPU and lookup events through stats_record(). Memory transfers
 * record their own events automatically; never record those twice.
 */
#include "cache_impl.h"
#include "memory.h"
#include "stats.h"

static uint32_t set_base_address(uint32_t tag, uint32_t set, uint32_t num_sets) {
    uint32_t block_number = tag * num_sets + set;
    uint32_t block_base = block_number * BLOCK_SIZE;
    return block_base;
}

static void sa2_touch(Cache *cache, L1Line *line) {
    line->last_used = ++cache->clock;
}

static uint32_t *sa2_prepare_line(Cache *cache, uint32_t address) {
    uint32_t num_sets = L1_LINES / 2u;
    uint32_t block_number = address / BLOCK_SIZE;
    uint32_t set = block_number % num_sets;
    uint32_t tag = block_number / num_sets;

    //Check both ways to see of its a Hit 
    for (uint32_t way = 0; way < 2u; ++way) {
        L1Line *line = &cache->l1[2u * set + way];
        if (line->valid && line->tag == tag) {
            sa2_touch(cache, line);
            stats_record(EVENT_L1_HIT);
            return line;
        }
    }

    //Neither way matched
    stats_record(EVENT_L1_MISS);

    L1Line *way0 = &cache->l1[2u * set];
    L1Line *way1 = &cache->l1[2u * set + 1u];
    L1Line *victim; //line to be evicted

    if (!way0->valid) {
        victim = way0;
    } else if (!way1->valid) {
        victim = way1;
    } else {
        victim = (way0->last_used <= way1->last_used) ? way0 : way1;
    }

    if (victim->valid && victim->dirty) {
        memory_write_block(set_base_address(victim->tag, set, num_sets), victim->data);
    }

    memory_read_block(block_number * BLOCK_SIZE, victim->data);
    victim->valid = true;
    victim->dirty = false;
    victim->tag = tag;
    sa2_touch(cache, victim);

    return victim;
}

uint8_t sa2_read(Cache *cache, uint32_t address)
{
    /* TODO: read one byte using this mode's mapping/replacement rules.
     * Emit one CPU_READ event and the appropriate lookup events.
     */

    stats_record(EVENT_CPU_READ);
    L1Line *line = sa2_prepare_line(cache, address);
    uint8_t offset = line->data[address % BLOCK_SIZE];
    return offset;
}

void sa2_write(Cache *cache, uint32_t address, uint8_t value)
{
    /* TODO: write-back/write-allocate for this mode.
     * Emit one CPU_WRITE event and the appropriate lookup events.
     */

    stats_record(EVENT_CPU_WRITE);
    L1Line *line = sa2_prepare_line(cache, address);
    line->data[address % BLOCK_SIZE] = value;
    line->dirty = true;
}

void sa2_flush(Cache *cache)
{
    /* TODO: flush this mode without invalidation or recency changes.
     * Emit no CPU/lookup events. Memory writes record their own events.
     */

    uint32_t num_sets = L1_LINES / 2u;
    for (uint32_t set = 0; set < num_sets; ++set) {
        for (uint32_t way = 0; way < 2u; ++way) {
            L1Line *line = &cache->l1[2u * set + way];
            if (line->valid && line->dirty) {
                memory_write_block(set_base_address(line->tag, set, num_sets), line->data);
                line->dirty = false;
            }
        }
    }
}
