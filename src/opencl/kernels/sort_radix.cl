#include "common_def.cl"

#ifdef RADIX_SORT
// Number of different values in a mask, equals 1 << bits count
    #define BITS_VALS 4
// Mask used to fetch bits, mask equals (1 << bits count) - 1
    #define BITS_MASK 0x3
#endif

__kernel void radix_sort_local(__global const uint* g_keys,
                               __global uint*       g_offsets,
                               __global uint*       g_blocks_size,
                               const uint           n,
                               const uint           shift) {
    const uint gid     = get_global_id(0);
    const uint gpid    = get_group_id(0);
    const uint ngroups = get_num_groups(0);
    const uint lid     = get_local_id(0);

    __local uint s_mask[BLOCK_SIZE];
    __local uint s_offsets[BLOCK_SIZE];

    s_mask[lid] = gid < n ? (g_keys[gid] >> shift) & BITS_MASK : 0;

    for (uint slot = 0; slot < BITS_VALS; slot += 1) {
        barrier(CLK_LOCAL_MEM_FENCE);

        uint process_bit = 0;

        if (gid < n) {
            process_bit = s_mask[lid] == slot ? 1 : 0;
        }

        s_offsets[lid] = process_bit;

        for (uint offset = 1; offset < BLOCK_SIZE; offset *= 2) {
            barrier(CLK_LOCAL_MEM_FENCE);
            uint value = s_offsets[lid];

            if (offset <= lid) {
                value += s_offsets[lid - offset];
            }

            barrier(CLK_LOCAL_MEM_FENCE);
            s_offsets[lid] = value;
        }

        barrier(CLK_LOCAL_MEM_FENCE);

        if (gid < n && process_bit) {
            g_offsets[gid] = s_offsets[lid] - 1;
        }

        if (lid == 0) {
            g_blocks_size[slot * ngroups + gpid] = s_offsets[BLOCK_SIZE - 1];
        }
    }
}

__kernel void radix_sort_scatter(__global const uint* g_in_keys,
                                 __global const TYPE* g_in_values,
                                 __global uint*       g_out_keys,
                                 __global TYPE*       g_out_values,
                                 __global const uint* g_offsets,
                                 __global const uint* g_blocks_offsets,
                                 const uint           n,
                                 const uint           shift) {
    const uint gid     = get_global_id(0);
    const uint gpid    = get_group_id(0);
    const uint ngroups = get_num_groups(0);


    if (gid < n) {
        const uint key    = g_in_keys[gid];
        const TYPE values = g_in_values[gid];

        uint slot   = (key >> shift) & BITS_MASK;
        uint offset = g_blocks_offsets[slot * ngroups + gpid] + g_offsets[gid];

        g_out_keys[offset]   = key;
        g_out_values[offset] = values;
    }
}