/**********************************************************************************/
/* This file is part of spla project                                              */
/* https://github.com/JetBrains-Research/spla                                     */
/**********************************************************************************/
/* MIT License                                                                    */
/*                                                                                */
/* Copyright (c) 2021 JetBrains-Research                                          */
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

#include "test_common.hpp"

#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_TARGET_OPENCL_VERSION 120
#include <CL/opencl.hpp>

#include <iostream>
#include <vector>

TEST(opencl, basic_gpu) {
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    cl::Platform platform = platforms.front();

    std::vector<cl::Device> devices;
    platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);
    cl::Device device = devices.front();

    cl::Context context(device);
    cl::CommandQueue queue(context);

    std::vector<int> vec_a = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<int> vec_b = {0, -1, 2, -3, 4, -5, 6, -7, 8, -9};
    std::vector<int> vec_c(10);

    cl::Buffer a(queue, vec_a.begin(), vec_a.end(), true, false);
    cl::Buffer b(queue, vec_b.begin(), vec_b.end(), true, false);
    cl::Buffer c(context, CL_MEM_READ_WRITE, sizeof(int) * vec_c.size());

    std::string kernel_code =
            "__kernel void add(__global const int* a, __global const int* b, __global int* c, int count) { "
            "   size_t idx = get_global_id(0); "
            "   if (idx < count) { c[idx] = a[idx] + b[idx]; } "
            "}";

    cl::Program program(context, kernel_code);
    program.build(device, "-cl-std=CL1.2");

    cl::Kernel kernel(program, "add");
    kernel.setArg(0, a);
    kernel.setArg(1, b);
    kernel.setArg(2, c);
    kernel.setArg(3, 10);

    cl::NDRange global(32);
    cl::NDRange local(32);
    queue.enqueueNDRangeKernel(kernel, cl::NDRange(), global, local);

    cl::copy(queue, c, vec_c.begin(), vec_c.end());

    for (auto value : vec_c)
        std::cout << value << std::endl;
}

SPLA_GTEST_MAIN