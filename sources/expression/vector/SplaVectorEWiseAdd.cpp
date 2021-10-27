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

#include <boost/compute/algorithm.hpp>
#include <boost/compute/iterator.hpp>
#include <compute/SplaGather.hpp>
#include <compute/SplaMaskByKey.hpp>
#include <compute/SplaMergeByKey.hpp>
#include <core/SplaError.hpp>
#include <core/SplaLibraryPrivate.hpp>
#include <core/SplaMath.hpp>
#include <expression/vector/SplaVectorEWiseAdd.hpp>
#include <storage/SplaVectorStorage.hpp>
#include <storage/block/SplaVectorCOO.hpp>

bool spla::VectorEWiseAdd::Select(std::size_t nodeIdx, const spla::Expression &expression) {
    return true;
}

void spla::VectorEWiseAdd::Process(std::size_t nodeIdx, const spla::Expression &expression, spla::TaskBuilder &builder) {
    auto &nodes = expression.GetNodes();
    auto node = nodes[nodeIdx];
    auto library = expression.GetLibrary().GetPrivatePtr();
    auto logger = library->GetLogger();

    auto w = node->GetArg(0).Cast<Vector>();
    auto mask = node->GetArg(1).Cast<Vector>();
    auto op = node->GetArg(2).Cast<FunctionBinary>();
    auto a = node->GetArg(3).Cast<Vector>();
    auto b = node->GetArg(4).Cast<Vector>();
    auto desc = node->GetDescriptor();

    // NOTE: mask allowed to be null
    assert(w.IsNotNull());
    assert(op.IsNotNull());
    assert(a.IsNotNull());
    assert(b.IsNotNull());
    assert(desc.IsNotNull());

    std::size_t requiredDeviceCount = w->GetStorage()->GetNblockRows();
    auto deviceIds = library->GetDeviceManager().FetchDevices(requiredDeviceCount, node);

    for (std::size_t i = 0; i < w->GetStorage()->GetNblockRows(); i++) {
        auto deviceId = deviceIds[i];
        builder.Emplace([=]() {
            using namespace boost;

            auto device = library->GetDeviceManager().GetDevice(deviceId);
            compute::context ctx = library->GetContext();
            compute::command_queue queue(ctx, device);

            auto type = w->GetType();
            auto byteSize = type->GetByteSize();
            auto typeHasValues = byteSize != 0;
            assert(typeHasValues);

            auto blockA = a->GetStorage()->GetBlock(i).Cast<VectorCOO>();
            const compute::vector<unsigned int> *rowsA = nullptr;
            compute::vector<unsigned int> permA(ctx);

            // Fill a values permutation indices
            if (blockA.IsNotNull() /* todo: && typeHasValues */) {
                auto nnz = blockA->GetNvals();
                permA.resize(nnz, queue);
                compute::copy_n(compute::make_counting_iterator(0), nnz, permA.begin(), queue);
            }

            auto blockB = b->GetStorage()->GetBlock(i).Cast<VectorCOO>();
            const compute::vector<unsigned int> *rowsB = nullptr;
            compute::vector<unsigned int> permB(ctx);

            // Fill b values permutation indices
            if (blockA.IsNotNull() /* todo: && typeHasValues */) {
                auto nnz = blockB->GetNvals();
                permB.resize(nnz, queue);
                compute::copy_n(compute::make_counting_iterator(0), nnz, permB.begin(), queue);
            }

            // If mask is present, apply it to a and b blocks
            compute::vector<unsigned int> tmpRowsA(ctx);
            compute::vector<unsigned int> tmpRowsB(ctx);

            auto maskBlock = mask.IsNotNull() ? mask->GetStorage()->GetBlock(i).Cast<VectorCOO>() : RefPtr<VectorCOO>();
            auto applyMask = [&](RefPtr<VectorCOO> &block, compute::vector<unsigned int> &tmpRows, compute::vector<unsigned int> &perm, const compute::vector<unsigned int> *&out) {
                if (block.IsNull())
                    return;

                if (maskBlock.IsNull()) {
                    out = &block->GetRows();
                    return;
                }

                auto maxResultCount = std::min(maskBlock->GetNvals(), block->GetNvals());
                tmpRows.resize(maxResultCount, queue);

                using ::boost::compute::lambda::_1;
                using ::boost::compute::lambda::_2;

                auto &maskRows = maskBlock->GetRows();
                auto &blockRows = block->GetRows();

                auto count = MaskByKey(maskRows.begin(), maskRows.end(),
                                       blockRows.begin(), blockRows.end(),
                                       perm.begin(),
                                       tmpRows.begin(),
                                       perm.begin(),
                                       _1 < _2,
                                       _1 == _2,
                                       queue);

                tmpRows.resize(count, queue);
                out = &tmpRows;
            };

            applyMask(blockA, tmpRowsA, permA, rowsA);
            applyMask(blockB, tmpRowsB, permB, rowsB);

            // Is some block is empty (or both, save result as is and finish without merge)
            if (!rowsA || !rowsB) {
                auto &storage = w->GetStorage();

                // Remove old block in case if both a and b are null
                storage->RemoveBlock(i);

                auto setResult = [&](RefPtr<VectorCOO> &block, const compute::vector<unsigned int> *rows, compute::vector<unsigned int> &perm) {
                    auto nnz = rows->size();
                    compute::vector<unsigned int> newRows = *rows;
                    compute::vector<unsigned char> newVals(ctx);

                    // Copy masked values if presented
                    if (typeHasValues) {
                        auto &vals = block->GetVals();
                        newVals.resize(nnz * byteSize, queue);
                        Gather(perm.begin(), perm.end(), vals.begin(), newVals.begin(), byteSize, queue).wait();
                    }

                    auto result = VectorCOO::Make(block->GetNrows(), nnz, std::move(newRows), std::move(newVals));
                    storage->SetBlock(i, result.As<VectorBlock>());
                };

                // Copy result of masking a block
                if (rowsA)
                    setResult(blockA, rowsA, permA);

                // Copy result of masking b block
                if (rowsB)
                    setResult(blockB, rowsB, permB);

                return;
            }

            // todo: merge + reduce
        });
    }
}

spla::ExpressionNode::Operation spla::VectorEWiseAdd::GetOperationType() const {
    return ExpressionNode::Operation::VectorEWiseAdd;
}
