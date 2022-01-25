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

#ifndef SPLA_SPLAUTILS_HPP
#define SPLA_SPLAUTILS_HPP

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <istream>
#include <sstream>
#include <type_traits>
#include <vector>

#include <spla-cpp/SplaConfig.hpp>

namespace spla {

    /**
     * @addtogroup API
     * @{
     */

    /**
     * @class CpuTimer
     * @brief Simple timer for measuring time on cpu side
     */
    class CpuTimer {
    public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;
        using Duration = std::chrono::nanoseconds;
        using Value = double;

    public:
        void Start() {
            mStart = mEnd = Clock::now();
        }

        void Stop() {
            mEnd = Clock::now();
            mElapsedMs += GetDurationMs();
        }

        Value Mark() {
            Stop();
            auto duration = GetDurationMs();
            Start();
            return duration;
        }

        void Reset() {
            mElapsedMs = 0.0;
            mStart = mEnd = TimePoint{};
        }

        [[nodiscard]] Duration GetDuration() const { return GetEnd() - GetStart(); }
        [[nodiscard]] Value GetDurationMs() const { return static_cast<double>(GetDuration().count()) * 1e-6; }
        [[nodiscard]] Value GetElapsedMs() const { return mElapsedMs; }

        [[nodiscard]] TimePoint GetStart() const noexcept { return mStart; }
        [[nodiscard]] TimePoint GetEnd() const noexcept { return mEnd; }

    private:
        TimePoint mStart{};
        TimePoint mEnd{};

        double mElapsedMs = 0.0;
    };

    /**
     * @class MatrixLoader
     * @brief A matrix loader of .mtx format
     *
     * @tparam Value A type of target matrix value
     */
    template<typename Value>
    class MatrixLoader {
    public:
        static constexpr bool HasValue = !std::is_same_v<Value, void>;
        using ValueCollectionType = std::conditional_t<std::is_same_v<Value, void>, nullptr_t, std::vector<Value>>;

    public:
        MatrixLoader() = default;

        /**
         * @brief Loads matrix from @p is
         *
         * @tparam FileValue Type of value which is written in the file
         * @param is An input stream with .mtx matrix inside
         * @param makeUndirected Add backward edges for directed edges
         * @param removeSelfLoops Remove loops (edges of type i -> i)
         * @param ignoreValues Ignore values inside file
         * @param verbose Verbose std output info
         * @param source Source name to display
         * @return Reference at created matrix
         */
        template<typename FileValue>
        MatrixLoader &Load(std::istream &is, bool makeUndirected, bool removeSelfLoops, bool ignoreValues, bool verbose = true, const std::string &source = "") {
            if (verbose) {
                std::cout << "Loading Matrix-market coordinate format graph...\n";
                std::cout << " Reading from " << source << "\n";

                if (removeSelfLoops)
                    std::cout << " Removing self-loops\n";
            }

            CpuTimer timer;
            CpuTimer total;
            total.Start();
            timer.Start();

            static_assert(!(std::is_same_v<FileValue, void> && !std::is_same_v<Value, void>) );
            std::string line;
            std::size_t lineN = 0;
            while (std::getline(is, line)) {
                ++lineN;
                if (line[0] != '%') break;
            }
            Size nnz;
            std::stringstream headerLineStream(line);
            headerLineStream >> mNrows >> mNcols >> nnz;

            if constexpr (HasValue) {
                mVals.reserve(nnz);
            }

            mRows.reserve(nnz);
            mCols.reserve(nnz);

            timer.Mark();

            while (std::getline(is, line)) {
                ++lineN;
                std::stringstream lineStream(line);
                Index i, j;
                lineStream >> i >> j;

                if (!(1 <= i && i <= mNrows)) {
                    throw std::logic_error("Row index is out of bounds on the line " + std::to_string(lineN));
                }

                if (!(1 <= j && j <= mNcols)) {
                    throw std::logic_error("Column index is out of bounds on the line " + std::to_string(lineN));
                }

                if (removeSelfLoops) {
                    if (i == j) {
                        nnz -= 1;
                        continue;
                    }
                }

                mRows.push_back(i);
                mCols.push_back(j);

                if constexpr (!std::is_same_v<FileValue, void>) {
                    FileValue value;
                    lineStream >> value;
                    if (!ignoreValues) {
                        if constexpr (HasValue) {
                            mVals.push_back(static_cast<Value>(value));
                        }
                    }
                }
            }

            if (ignoreValues) {
                if constexpr (HasValue) {
                    mVals.resize(mRows.size());
                }
            }

            timer.Stop();

            if (mRows.size() != nnz) {
                throw std::logic_error("Number of non zero values is not valid");
            }

            if (verbose)
                std::cout << " Parsing MTX file ("
                          << mNrows << " rows, "
                          << mNcols << " cols, "
                          << nnz << " directed edges)"
                          << " in " << timer.GetElapsedMs() << " ms\n";

            timer.Start();

            // Offset indices
            for (std::size_t k = 0; k < mRows.size(); k++) {
                mRows[k] -= 1;
                mCols[k] -= 1;
            }

            timer.Stop();

            if (verbose)
                std::cout << " Offset indices by -1 in "
                          << timer.GetDurationMs() << " ms\n";

            if (makeUndirected) {
                timer.Start();
                DoubleEdges();
                timer.Stop();

                if (verbose)
                    std::cout << " Doubling edges: " << nnz
                              << " to " << GetNvals()
                              << " in " << timer.GetDurationMs() << " ms\n";
            }

            double averageDegree;
            std::size_t maxDegree;
            std::size_t minDegree;

            if (GetNrows() == GetNcols())
                ComputeStats(minDegree, maxDegree, averageDegree);

            total.Stop();

            if (verbose) {
                std::cout << " Stats: min.deg " << minDegree
                          << ", max.deg " << maxDegree
                          << ", avg.deg " << averageDegree << "\n";
                std::cout << " Loaded in " << total.GetElapsedMs() << " ms\n";
            }

            return *this;
        }

        template<typename FileValue>
        MatrixLoader &Load(const std::string &filename, bool makeUndirected, bool removeSelfLoops, bool ignoreValues, bool verbose = true) {
            std::ifstream file(filename);
            if (!file.is_open()) {
                throw std::invalid_argument("Could not open '" + filename + "' to read matrix");
            }
            return Load<FileValue>(file, makeUndirected, removeSelfLoops, ignoreValues, verbose, filename);
        }

        template<class FillValue>
        void Fill(FillValue value) {
            for (auto &v : mVals) {
                v = value;
            }
        }

        template<typename Generator>
        void Generate(Generator &&generator) {
            for (auto &v : mVals) {
                v = generator();
            }
        }

        [[nodiscard]] Size GetNrows() const { return mNrows; }
        [[nodiscard]] Size GetNcols() const { return mNcols; }
        [[nodiscard]] Size GetNvals() const { return mRows.size(); }
        [[nodiscard]] static constexpr Size GetElementSize() { return sizeof(Value); }

        [[nodiscard]] const std::vector<Index> &GetRowIndices() const { return mRows; }
        [[nodiscard]] const std::vector<Index> &GetColIndices() const { return mCols; }

        [[nodiscard]] std::vector<Index> &GetRowIndices() { return mRows; }
        [[nodiscard]] std::vector<Index> &GetColIndices() { return mCols; }

        [[nodiscard]] std::conditional_t<HasValue, const ValueCollectionType &, nullptr_t> GetValues() const {
            return mVals;
        }

    private:
        void DoubleEdges() {
            auto nnz = GetNvals();
            for (std::size_t i = 0; i < nnz; i++) {
                if (mRows[i] != mCols[i]) {
                    mRows.push_back(mCols[i]);
                    mCols.push_back(mRows[i]);

                    if constexpr (HasValue) {
                        mVals.push_back(mVals[i]);
                    }
                }
            }
        }

        void ComputeStats(std::size_t &minDegree, std::size_t &maxDegree, double &averageDegree) {
            std::vector<std::size_t> degreePerVertex(GetNrows(), 0);

            for (std::size_t k = 0; k < GetNvals(); k++) {
                degreePerVertex[mRows[k]] += 1;
            }

            maxDegree = 0;
            minDegree = GetNvals() + 1;
            averageDegree = 0.0f;

            for (auto d : degreePerVertex) {
                maxDegree = std::max(maxDegree, d);
                minDegree = std::min(minDegree, d);
                averageDegree += static_cast<double>(d);
            }

            averageDegree = GetNrows() > 0 ? averageDegree / static_cast<double>(GetNrows()) : 0.0;
        }

        ValueCollectionType mVals{};
        std::vector<Index> mRows{};
        std::vector<Index> mCols{};

        Size mNrows{};
        Size mNcols{};
    };

    namespace {
        inline void OutputMeasurements(std::ostream &stream, const CpuTimer &warmUp, const std::vector<CpuTimer> &iters) {
            stream << "warm-up(ms): " << std::setprecision(15) << warmUp.GetElapsedMs() << "\n";
            stream << "iters(ms): ";
            for (auto &iter : iters)
                stream << iter.GetElapsedMs() << " ";
            stream << std::endl;
        }

        inline void OutputMeasurements(const CpuTimer &warmUp, const std::vector<CpuTimer> &iters) {
            OutputMeasurements(std::cout, warmUp, iters);
        }
    }// namespace

    /**
     * @}
     */

}// namespace spla

#endif//SPLA_SPLAUTILS_HPP