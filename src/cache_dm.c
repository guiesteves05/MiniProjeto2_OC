/* STUDENT FILE: Part 1 - Direct-mapped cache.
 * Implement only this mode here; cache.c supplies initialization/dispatch.
 * Keep helpers static and in this file. Do not call another student module.
 * Record CPU and lookup events through stats_record(). Memory transfers
 * record their own events automatically; never record those twice.
 */
#include "cache_impl.h"
#include "memory.h"
#include "stats.h"

/* Reconstructs the aligned physical memory base address of a block stored in L1
 * Because L1 only stores the upper tag bits, we recombine tag and index
 * to recover the full block_number, then multiply by BLOCK_SIZE to get a byte address for the base
 */
static uint32_t line_base_address(uint32_t tag, uint32_t index) {
    uint32_t block_number = tag * L1_LINES + index; 
    uint32_t block_base = block_number * BLOCK_SIZE;
    return block_base;
}

/* Ensures the block containing address is loaded into its mapped L1 line,
 * handling hit detection, dirty eviction write-back, and memory fetch on a miss
 * Shared by both dm_read and dm_write (since writes use write-allocate)
 */
static L1Line *dm_prepare_line(Cache *cache, uint32_t address) {
    uint32_t block_number = address / BLOCK_SIZE; //Block where the line is
    uint32_t index = block_number % L1_LINES; //The only line in cache where this block can be
    uint32_t tag = block_number / L1_LINES; //Identifies the block that is currently in that line (from all the possible options)
    L1Line *line = &cache->l1[index];

    //If its a hit or a miss
    if (line->valid && line->tag == tag) {
        stats_record(EVENT_L1_HIT);
        return line;
    } else {
        stats_record(EVENT_L1_MISS);

        //If the its the miss, and the line is dirty, we write it back to memory
        if (line->valid && line->dirty) {
            memory_write_block(line_base_address(line->tag, index), line->data);
        }

        //Read the block from memory
        memory_read_block(block_number * BLOCK_SIZE, line->data);
        line->valid = true;
        line->dirty = false;
        line->tag = tag;

        return line;
    }
} 


uint8_t dm_read(Cache *cache, uint32_t address)
{
    stats_record(EVENT_CPU_READ);
    L1Line *line = dm_prepare_line(cache, address);

    // Extract the single requested byte at that offset (address % BLOCK_SIZE = byte offset)
    uint8_t byte = line->data[address % BLOCK_SIZE];
    return byte;
}

void dm_write(Cache *cache, uint32_t address, uint8_t value)
{
    stats_record(EVENT_CPU_WRITE);
    L1Line *line = dm_prepare_line(cache, address);

    // Update only the target byte inside the cached block and mark the line dirty
    // main memory is not updated yet
    line->data[address % BLOCK_SIZE] = value;
    line->dirty = true;
}

void dm_flush(Cache *cache)
{
    // Visit lines and write back any valid dirty blocks
    // Clears dirty bits so repeated flushes do nothing, while keeping valid, tag, and data intact
    for (uint32_t index = 0; index < L1_LINES; ++index) {
        L1Line *line = &cache->l1[index];
        if (line->valid && line->dirty) {
            memory_write_block(line_base_address(line->tag, index), line->data);
            line->dirty = false;
        }
    }

}
