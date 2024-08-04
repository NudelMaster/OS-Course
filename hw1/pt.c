# include "os.h"

uint64_t getentry_index(uint64_t vpn, int level){
    /* since the vpn is 45 bits we need to take 9 bits at each level */
    int res = vpn >> (36 - level * 9) & 0x1ff;
    return res;
}
void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn){
    uint64_t root = pt << 12;
    uint64_t* table = phys_to_virt(root);
    uint64_t entry_index = 0;
    for (int i = 0; i < 4; i++){
        entry_index  = getentry_index(vpn, i);
        if ((table[entry_index] & 1) == 0){
            /* if current pte is invalid need to allocate page and mark as valid */
            table[entry_index] = (alloc_page_frame() << 12) | 0x1;
        }
        /* setting the 12 least significant bits to zero in order to get the page frame number in correct format */
        uint64_t next_level = table[entry_index] & ~0xFFF;
        table = phys_to_virt(next_level);
    }
    /* if program got to this point, then we are in the last level and can access the last entry_index */
    entry_index = vpn & 0x1ff;
    if (ppn == NO_MAPPING){
        // destroying the mapping by invalidating the entry
        table[entry_index] = 0;

    }
    else{
        /* next PTE set to ppn and mark as valid */
        table[entry_index] = (ppn << 12) | 0x1;
    }

}

uint64_t page_table_query(uint64_t pt, uint64_t vpn){
    uint64_t root = pt << 12; /* adding offset to the frame number */
    uint64_t* table = phys_to_virt(root);
    uint64_t entry_index = 0;
    for (int i = 0; i < 4; i++){
        /* shifting to the right to get the i'th part of the vpn + masking in order to get the value of the 9 bits */
        entry_index = getentry_index(vpn, i);
        /* if the least significant bit is 0 then there is no mapping */
        if ((table[entry_index] & 1) == 0){
            return NO_MAPPING;
        }
        /* setting the 12 least significant bits to zero in order to get the page frame number in correct format */
        uint64_t next_level = table[entry_index] & ~0xFFF;
        table = phys_to_virt(next_level);

    }
    /* if program got to this point, then we are in the last level and can access the last entry_index */
    entry_index = vpn & 0x1ff;

    if ((table[entry_index] & 1) == 0){
        return NO_MAPPING;
    }

    /* getting the final PTE which is PPN of the final level, and shifting 12 bits to the right to get the number itself */
    uint64_t PPN = table[entry_index] >> 12;
    return PPN;

}
