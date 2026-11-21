/* STUDENT FILE: Part 3 - Direct-mapped L1 plus victim cache.
 * Implement only this mode here; cache.c supplies initialization/dispatch.
 * Keep helpers static and in this file. Do not call another student module.
 * Record CPU and lookup events through stats_record(). Memory transfers
 * record their own events automatically; never record those twice.
 */
#include "cache_impl.h"
#include "memory.h"
#include "stats.h"
#include <string.h>

/* Reconstructs the aligned base address of a block stored in direct-mapped L1. */
static uint32_t l1_base_address(uint32_t tag, uint32_t index) {
    uint32_t block_number = tag * L1_LINES + index;
    uint32_t block_base = block_number * BLOCK_SIZE;
    return block_base;
}

/* Marks a victim entry as most recently used whenever a block enters it. */
static void victim_touch(Cache *cache, VictimEntry *entry) {
    entry->last_used = ++cache->clock;
}

/* Selects where to place a block evicted from L1 into the victim cache
 * (lowest-numbered invalid entry first, otherwise the least recently used entry)
 */
static VictimEntry *victim_select_slot(Cache *cache) {
    for (uint32_t i = 0; i < VICTIM_BLOCKS; ++i) {
        if (!cache->victim[i].valid) {
            return &cache->victim[i];
        }
    }

    VictimEntry *lru = &cache->victim[0];
    for (uint32_t i = 1; i < VICTIM_BLOCKS; ++i) {
        if (cache->victim[i].last_used < lru->last_used) {
            lru = &cache->victim[i];
        }
    }
    return lru;
}

/* Ensures the block for address is in L1, probing/updating the victim cache on L1 misses. */
static L1Line *victim_prepare_line(Cache *cache, uint32_t address) {
    uint32_t block_number = address / BLOCK_SIZE;
    uint32_t index = block_number % L1_LINES;
    uint32_t tag = block_number / L1_LINES;
    L1Line *line = &cache->l1[index];

    //If its a Hit
    if (line->valid && line->tag == tag) {
        stats_record(EVENT_L1_HIT);
        return line;
    } 

    //Its a L1 Miss
    stats_record(EVENT_L1_MISS);

    //If its a L1 Miss, we check all the victim cache entries
    VictimEntry *hit = NULL;
    for (uint32_t i = 0; i < VICTIM_BLOCKS; ++i) {
        if (cache->victim[i].valid && cache->victim[i].block_number == block_number) {
            hit = &cache->victim[i]; //Its a victim cache Hit
            break;
        }
    }

    //If its a victim Hit, we have to swap the blocks in the victim cache line and the L1 cache
    if (hit != NULL) {
        stats_record(EVENT_VICTIM_HIT);

        
        if (line->valid) {
            //That L1 line has a valid block so we swap the displaced L1 block and the matching victim block
            uint8_t temp_data[BLOCK_SIZE];
            memcpy(temp_data, hit->data, BLOCK_SIZE);
            bool temp_dirty = hit->dirty;

            hit->valid = true;
            hit->dirty = line->dirty;
            hit->block_number = line->tag * L1_LINES + index;
            memcpy(hit->data, line->data, BLOCK_SIZE);
            victim_touch(cache, hit); // A block entered victim, so update its recency

            line->valid = true;
            line->dirty = temp_dirty;
            line->tag = tag;
            memcpy(line->data, temp_data, BLOCK_SIZE);
        } else {
            // L1 line was empty so we move block from victim into L1 and invalidate the victim slot
            line->valid = true;
            line->dirty = hit->dirty;
            line->tag = tag;
            memcpy(line->data, hit->data, BLOCK_SIZE);
            
            hit->valid = false;
        }
        return line;
    }

    //Its a victim cache Miss
    stats_record(EVENT_VICTIM_MISS);

    //If its a victim Miss and L1 holds a valid block in that line
    if (line->valid) {
        VictimEntry *slot = victim_select_slot(cache);

        // Only write back to memory if a dirty block is evicted from the victim cache
        if (slot->valid && slot->dirty) {
            memory_write_block(slot->block_number * BLOCK_SIZE, slot->data);
        }

        //Evicts the L1 block to the Victim caches
        slot->valid = true;
        slot->dirty = line->dirty;
        slot->block_number = line->tag * L1_LINES + index;
        memcpy(slot->data, line->data, BLOCK_SIZE);
        victim_touch(cache, slot);
    }

    // Fetch the requested block from main memory into L1
    memory_read_block(block_number * BLOCK_SIZE, line->data);
    line->valid = true;
    line->dirty = false;
    line->tag = tag;

    return line;
}


uint8_t victim_read(Cache *cache, uint32_t address)
{
    stats_record(EVENT_CPU_READ);
    L1Line *line = victim_prepare_line(cache, address);
    uint8_t byte = line->data[address % BLOCK_SIZE];
    return byte;
}

void victim_write(Cache *cache, uint32_t address, uint8_t value)
{
    stats_record(EVENT_CPU_WRITE);
    L1Line *line = victim_prepare_line(cache, address);
    line->data[address % BLOCK_SIZE] = value;
    line->dirty = true;
}

void victim_flush(Cache *cache)
{
    // Flush L1 in increasing index order first
    for (uint32_t index = 0; index < L1_LINES; ++index) {
        L1Line *line = &cache->l1[index];
        if (line->valid && line->dirty) {
            memory_write_block(l1_base_address(line->tag, index), line->data);
            line->dirty = false;
        }
    }

    // Flush victim cache entries in increasing index order second
    for (uint32_t i = 0; i < VICTIM_BLOCKS; ++i) {
        VictimEntry *entry = &cache->victim[i];
        if (entry->valid && entry->dirty) {
            memory_write_block(entry->block_number * BLOCK_SIZE, entry->data);
            entry->dirty = false;
        }
    }

}
