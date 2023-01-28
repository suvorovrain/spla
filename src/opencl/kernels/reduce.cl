#include "common_def.cl"

// wave-wide reduction in local memory
void reduction_group(uint                   block_size,
                     uint                   lid,
                     volatile __local TYPE* s_sum) {
    if (BLOCK_SIZE >= block_size) {
        if (lid < (block_size / 2)) {
            s_sum[lid] = OP_BINARY(s_sum[lid], s_sum[lid + (block_size / 2)]);
        }
        if (block_size > WARP_SIZE) {
            barrier(CLK_LOCAL_MEM_FENCE);
        }
    }
}

__kernel void reduce(__global const TYPE* g_vec,
                     __global const TYPE* g_init,
                     __global TYPE*       g_sum,
                     const uint           stride,
                     const uint           n) {
    const uint gid   = get_group_id(0);
    const uint lsize = get_local_size(0);
    const uint lid   = get_local_id(0);

    __local TYPE s_sum[BLOCK_SIZE];
    TYPE         sum = g_init[0];

    const uint gstart = gid * stride;
    const uint gend   = gstart + stride;

    for (uint i = gstart + lid; i < gend && i < n; i += lsize) {
        sum = OP_BINARY(sum, g_vec[i]);
    }

    s_sum[lid] = sum;
    barrier(CLK_LOCAL_MEM_FENCE);

    reduction_group(1024, lid, s_sum);
    reduction_group(512, lid, s_sum);
    reduction_group(256, lid, s_sum);
    reduction_group(128, lid, s_sum);
    reduction_group(64, lid, s_sum);
    reduction_group(32, lid, s_sum);
    reduction_group(16, lid, s_sum);
    reduction_group(8, lid, s_sum);
    reduction_group(4, lid, s_sum);
    reduction_group(2, lid, s_sum);

    if (lid == 0) {
        g_sum[gid] = s_sum[0];
    }
}