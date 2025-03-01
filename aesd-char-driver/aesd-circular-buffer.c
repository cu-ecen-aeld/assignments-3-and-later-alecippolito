/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
	    // Check if the buffer has any entries; if not, return NULL indicating failure.
    if (buffer->out_offs == buffer->in_offs && !buffer->full)
        return NULL;

    uint8_t offs = buffer->out_offs;
    struct aesd_buffer_entry *entry = &buffer->entry[offs];
    
    // Loop through the entries until the requested char_offset is found within an entry.
    while (char_offset >= entry->size) {
        
        // Reduce the char_offset by the size of this entry, and move to the next entry.
        char_offset -= entry->size;
        offs = (offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

        // If we've wrapped around to the in_offs, return NULL to indicate the buffer is empty or we've exhausted entries.
        if (offs == buffer->in_offs)
            return NULL;

        // Move to the next entry in the buffer.
        entry = &buffer->entry[offs];
    }
    
    // If the requested char_offset is beyond the current entry size, return NULL as we can't find the position.
    if (char_offset >= entry->size)
        return NULL;

    // The char_offset has been found within the current entry, return it along with the byte offset.
    *entry_offset_byte_rtn = char_offset;
    return entry;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
	    // Copy the new entry into the buffer at the current "in" position.
    memcpy(&buffer->entry[buffer->in_offs], add_entry, sizeof(*add_entry));

    // Update the "in" pointer in a circular manner, advancing to the next position.
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    
    // If the buffer was already full, adjust the "out" pointer to the same position as "in".
    if (buffer->full){
        buffer->out_offs = buffer->in_offs;
    }
    // If the buffer wasn't full, check if it is now full after adding the new entry.
    else if (buffer->in_offs == buffer->out_offs) {
        buffer->full = true;
	}
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
