#include "common_def.cl"

__kernel void fill_zero(__global TYPE* values,
                        const uint     n) {
    const uint gid     = get_global_id(0);
    const uint gstride = get_global_size(0);
    const TYPE value   = 0;

    for (uint i = gid; i < n; i += gstride) {
        values[i] = value;
    }
}

__kernel void fill_value(__global TYPE* values,
                         const uint     n,
                         const TYPE     value) {
    const uint gid     = get_global_id(0);
    const uint gstride = get_global_size(0);

    for (uint i = gid; i < n; i += gstride) {
        values[i] = value;
    }
}