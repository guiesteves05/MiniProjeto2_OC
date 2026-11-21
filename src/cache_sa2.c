/* STUDENT FILE: Part 2 - Two-way set-associative cache.
 * Implement only this mode here; cache.c supplies initialization/dispatch.
 * Keep helpers static and in this file. Do not call another student module.
 * Record CPU and lookup events through stats_record(). Memory transfers
 * record their own events automatically; never record those twice.
 */
#include "cache_impl.h"
#include "memory.h"
#include "stats.h"

/* Reconstructs the aligned memory base address of a block in a set
 * Since tag = block_number / num_sets and set = block_number % num_sets,
 * recombining them gives block_number, and multiplying by BLOCK_SIZE gives the byte address
 */
static uint32_t set_base_address(uint32_t tag, uint32_t set, uint32_t num_sets) {
    uint32_t block_number = tag * num_sets + set; 
    uint32_t block_base = block_number * BLOCK_SIZE; 
    return block_base;
}

/* Marks an L1 way as the most recently used by advancing the cache's clock */
static void sa2_touch(Cache *cache, L1Line *line) {
    line->last_used = ++cache->clock;
}

static L1Line *sa2_prepare_line(Cache *cache, uint32_t address) {
    uint32_t num_sets = L1_LINES / 2u; //number of sets in the cache
    uint32_t block_number = address / BLOCK_SIZE; //block number of the address
    uint32_t set = block_number % num_sets; //Set to which that block maps
    uint32_t tag = block_number / num_sets; //tag to distiguish blocks who map to the same set

    //Check both ways of the set to see if its a Hit 
    for (uint32_t way = 0; way < 2u; ++way) {
        L1Line *line = &cache->l1[2u * set + way];
        if (line->valid && line->tag == tag) {
            sa2_touch(cache, line);
            stats_record(EVENT_L1_HIT);
            return line;
        }
    }

    //Neither way matched, so its a Miss
    stats_record(EVENT_L1_MISS);

    L1Line *way0 = &cache->l1[2u * set];
    L1Line *way1 = &cache->l1[2u * set + 1u];
    L1Line *victim; //line to be evicted (victim)

    if (!way0->valid) {
        victim = way0;
    } else if (!way1->valid) {
        victim = way1;
    } else {
        victim = (way0->last_used <= way1->last_used) ? way0 : way1;
    }

    // Write-back to save modified data to memory before overwriting a valid dirty way
    if (victim->valid && victim->dirty) {
        memory_write_block(set_base_address(victim->tag, set, num_sets), victim->data);
    }

    //Fetch the new block into the the chosen L1 way (overwriting the victim) and mark it as most recently used
    memory_read_block(block_number * BLOCK_SIZE, victim->data);
    victim->valid = true;
    victim->dirty = false;
    victim->tag = tag;
    sa2_touch(cache, victim);

    return victim;
}

uint8_t sa2_read(Cache *cache, uint32_t address)
{
    stats_record(EVENT_CPU_READ);
    L1Line *line = sa2_prepare_line(cache, address);
    
    uint8_t byte = line->data[address % BLOCK_SIZE]; // Extract byte at the block offset
    return byte;
}

void sa2_write(Cache *cache, uint32_t address, uint8_t value)
{
    stats_record(EVENT_CPU_WRITE);
    L1Line *line = sa2_prepare_line(cache, address);  //(Already writes block back to memory if its a miss)

    line->data[address % BLOCK_SIZE] = value; // Update byte in cache and marks it dirty
    line->dirty = true;
}

void sa2_flush(Cache *cache)
{
    // Write back all dirty valid lines in increasing set order, then increasing way order
    // Preserves valid bits, tags, data, and LRU recency
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
