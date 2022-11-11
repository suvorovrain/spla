#include "common.cl"

__kernel void vxm_prepare(__global TYPE* g_rx,
                          const TYPE     init,
                          const uint     n) {
    const uint gid = get_global_id(0);

    if (gid < n) {
        g_rx[gid] = init;
    }
}

__kernel void vxm_atomic_vector(__global const TYPE* g_vx,
                                __global const uint* g_Ap,
                                __global const uint* g_Aj,
                                __global const TYPE* g_Ax,
                                __global const TYPE* g_mask,
                                __global TYPE*       g_rx,
                                const uint           n) {
    const uint lid     = get_local_id(1);   // thread id in a row
    const uint lsize   = get_local_size(1); // num threads to process row
    const uint gid     = get_global_id(0);  // id of row to touch
    const uint gstride = get_global_size(0);// step between row ids

    for (int row_id = gid; row_id < n; row_id += gstride) {
        const uint start = g_Ap[row_id];
        const uint end   = g_Ap[row_id + 1];

        const TYPE vx = g_vx[row_id];

        if (vx) {
            for (uint i = start + lid; i < end; i += lsize) {
                const uint col_id = g_Aj[i];
                const TYPE prod   = OP_BINARY1(vx, g_Ax[i]);

                if (OP_SELECT(g_mask[col_id])) {
                    bool success = false;
                    TYPE old     = g_rx[col_id];

                    while (!success) {
                        const TYPE val = OP_BINARY2(old, prod);

                        if (val == old) break;

                        const TYPE res = atomic_cmpxchg(g_rx + col_id, old, val);

                        success = old == res;
                        old     = res;
                    }
                }
            }
        }
    }
}

__kernel void vxm_atomic_scalar(__global const TYPE* g_vx,
                                __global const uint* g_Ap,
                                __global const uint* g_Aj,
                                __global const TYPE* g_Ax,
                                __global const TYPE* g_mask,
                                __global TYPE*       g_rx,
                                const uint           n,
                                const uint           early_exit) {
    const uint gid     = get_global_id(0);  // id of row to touch
    const uint gstride = get_global_size(0);// step between row ids

    for (int row_id = gid; row_id < n; row_id += gstride) {
        const uint start = g_Ap[row_id];
        const uint end   = g_Ap[row_id + 1];

        const TYPE vx = g_vx[row_id];

        if (vx) {
            for (uint i = start; i < end; i += 1) {
                const uint col_id = g_Aj[i];
                const TYPE prod   = OP_BINARY1(vx, g_Ax[i]);

                if (OP_SELECT(g_mask[col_id])) {
                    bool success = false;
                    TYPE old     = g_rx[col_id];

                    while (!success) {
                        if (old && early_exit) break;

                        const TYPE val = OP_BINARY2(old, prod);

                        if (val == old) break;

                        const TYPE res = atomic_cmpxchg(g_rx + col_id, old, val);

                        success = old == res;
                        old     = res;
                    }
                }
            }
        }
    }
}