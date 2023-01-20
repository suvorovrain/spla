/**********************************************************************************/
/* This file is part of spla project                                              */
/* https://github.com/SparseLinearAlgebra/spla                                    */
/**********************************************************************************/
/* MIT License                                                                    */
/*                                                                                */
/* Copyright (c) 2023 SparseLinearAlgebra                                         */
/*                                                                                */
/* Permission is hereby granted, free of charge, to any person obtaining a copy   */
/* of this software and associated documentation files (the "Software"), to deal  */
/* in the Software without restriction, including without limitation the rights   */
/* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell      */
/* copies of the Software, and to permit persons to whom the Software is          */
/* furnished to do so, subject to the following conditions:                       */
/*                                                                                */
/* The above copyright notice and this permission notice shall be included in all */
/* copies or substantial portions of the Software.                                */
/*                                                                                */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,       */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE    */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER         */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,  */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE  */
/* SOFTWARE.                                                                      */
/**********************************************************************************/

#ifndef SPLA_CL_DENSE_VEC_HPP
#define SPLA_CL_DENSE_VEC_HPP

#include <opencl/cl_formats.hpp>
#include <opencl/cl_utils.hpp>

namespace spla {

    /**
     * @addtogroup internal
     * @{
     */

    template<typename T>
    void cl_dense_vec_resize(const std::size_t n_rows,
                             CLDenseVec<T>&    storage) {
        const std::size_t buffer_size = n_rows * sizeof(T);
        const auto        flags       = CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS;

        cl::Buffer buffer(get_acc_cl()->get_context(), flags, buffer_size);
        storage.Ax = std::move(buffer);
    }

    template<typename T>
    void cl_dense_vec_init(const std::size_t n_rows,
                           const T*          values,
                           CLDenseVec<T>&    storage) {
        assert(values);

        const std::size_t buffer_size = n_rows * sizeof(T);
        const auto        flags       = CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS | CL_MEM_COPY_HOST_PTR;

        cl::Buffer buffer(get_acc_cl()->get_context(), flags, buffer_size, (void*) values);
        storage.Ax = std::move(buffer);
    }

    template<typename T>
    void cl_dense_vec_write(const std::size_t n_rows,
                            const T*          values,
                            CLDenseVec<T>&    storage,
                            cl::CommandQueue& queue,
                            bool              blocking = true) {
        queue.enqueueWriteBuffer(storage.Ax, blocking, 0, n_rows * sizeof(T), values);
    }

    template<typename T>
    void cl_dense_vec_read(const std::size_t n_rows,
                           T*                values,
                           CLDenseVec<T>&    storage,
                           cl::CommandQueue& queue,
                           bool              blocking = true) {
        const std::size_t buffer_size = n_rows * sizeof(T);
        cl::Buffer        staging(get_acc_cl()->get_context(), CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, buffer_size);

        queue.enqueueCopyBuffer(storage.Ax, staging, 0, 0, buffer_size);
        queue.enqueueReadBuffer(staging, blocking, 0, buffer_size, values);
    }

    template<typename T>
    void cl_dense_vec_to_coo(const std::size_t    n_rows,
                             const CLDenseVec<T>& in,
                             CLCooVec<T>&         out,
                             cl::CommandQueue&    queue) {
        auto* acc   = get_acc_cl();
        auto* utils = acc->get_utils();

        cl::Buffer temp_Ai(acc->get_context(), CL_MEM_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS, n_rows * sizeof(uint));
        cl::Buffer temp_Ax(acc->get_context(), CL_MEM_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS, n_rows * sizeof(T));

        uint count;
        utils->template vec_dense_to_coo<T>(in.Ax, temp_Ai, temp_Ax, n_rows, count, queue);

        out.values = count;
        out.Ai     = cl::Buffer(acc->get_context(), CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, count * sizeof(uint));
        out.Ax     = cl::Buffer(acc->get_context(), CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, count * sizeof(T));

        queue.enqueueCopyBuffer(temp_Ai, out.Ai, 0, 0, count * sizeof(uint));
        queue.enqueueCopyBuffer(temp_Ax, out.Ax, 0, 0, count * sizeof(T));
        queue.finish();
    }


    /**
     * @}
     */

}// namespace spla

#endif//SPLA_CL_DENSE_VEC_HPP
