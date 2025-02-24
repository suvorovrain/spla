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

#ifndef SPLA_CPU_V_ASSIGN_HPP
#define SPLA_CPU_V_ASSIGN_HPP

#include <schedule/schedule_tasks.hpp>

#include <core/dispatcher.hpp>
#include <core/registry.hpp>
#include <core/top.hpp>
#include <core/tscalar.hpp>
#include <core/ttype.hpp>
#include <core/tvector.hpp>

namespace spla {

    template<typename T>
    class Algo_v_assign_masked_cpu final : public RegistryAlgo {
    public:
        ~Algo_v_assign_masked_cpu() override = default;

        std::string get_name() override {
            return "v_assign_masked";
        }

        std::string get_description() override {
            return "sequential masked vector assignment";
        }

        Status execute(const DispatchContext& ctx) override {
            auto                t    = ctx.task.template cast_safe<ScheduleTask_v_assign_masked>();
            ref_ptr<TVector<T>> mask = t->mask.template cast_safe<TVector<T>>();

            if (mask->is_valid(FormatVector::CpuCoo))
                return execute_sp2dn(ctx);
            if (mask->is_valid(FormatVector::CpuDense))
                return execute_dn2dn(ctx);

            return execute_sp2dn(ctx);
        }

    private:
        Status execute_sp2dn(const DispatchContext& ctx) {
            TIME_PROFILE_SCOPE("cpu/vector_assign_sparse2dense");

            auto t = ctx.task.template cast_safe<ScheduleTask_v_assign_masked>();

            auto r         = t->r.template cast_safe<TVector<T>>();
            auto mask      = t->mask.template cast_safe<TVector<T>>();
            auto value     = t->value.template cast_safe<TScalar<T>>();
            auto op_assign = t->op_assign.template cast_safe<TOpBinary<T, T, T>>();
            auto op_select = t->op_select.template cast_safe<TOpSelect<T>>();

            auto assign_value = value->get_value();

            r->validate_rwd(FormatVector::CpuDense);
            mask->validate_rw(FormatVector::CpuCoo);

            auto*       p_r_dense     = r->template get<CpuDenseVec<T>>();
            const auto* p_mask_sparse = mask->template get<CpuCooVec<T>>();
            const auto& func_assign   = op_assign->function;
            const auto& func_select   = op_select->function;

            uint N = p_mask_sparse->values;

            for (uint idx = 0; idx < N; ++idx) {
                uint i = p_mask_sparse->Ai[idx];
                auto x = p_mask_sparse->Ax[idx];
                if (func_select(x)) {
                    p_r_dense->Ax[i] = func_assign(p_r_dense->Ax[i], assign_value);
                }
            }

            return Status::Ok;
        }

        Status execute_dn2dn(const DispatchContext& ctx) {
            TIME_PROFILE_SCOPE("cpu/vector_assign_dense2dense");

            auto t = ctx.task.template cast_safe<ScheduleTask_v_assign_masked>();

            auto r         = t->r.template cast_safe<TVector<T>>();
            auto mask      = t->mask.template cast_safe<TVector<T>>();
            auto value     = t->value.template cast_safe<TScalar<T>>();
            auto op_assign = t->op_assign.template cast_safe<TOpBinary<T, T, T>>();
            auto op_select = t->op_select.template cast_safe<TOpSelect<T>>();

            auto assign_value = value->get_value();

            r->validate_rwd(FormatVector::CpuDense);
            mask->validate_rw(FormatVector::CpuDense);

            auto*       p_r_dense    = r->template get<CpuDenseVec<T>>();
            const auto* p_mask_dense = mask->template get<CpuDenseVec<T>>();
            const auto& func_assign  = op_assign->function;
            const auto& func_select  = op_select->function;

            uint N = r->get_n_rows();

            for (uint i = 0; i < N; ++i) {
                if (func_select(p_mask_dense->Ax[i])) {
                    p_r_dense->Ax[i] = func_assign(p_r_dense->Ax[i], assign_value);
                }
            }

            return Status::Ok;
        }
    };

}// namespace spla

#endif//SPLA_CPU_V_ASSIGN_HPP
