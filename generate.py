##################################################################################
# This file is part of spla project                                              #
# https://github.com/SparseLinearAlgebra/spla                                    #
##################################################################################
# MIT License                                                                    #
#                                                                                #
# Copyright (c) 2023 SparseLinearAlgebra                                         #
#                                                                                #
# Permission is hereby granted, free of charge, to any person obtaining a copy   #
# of this software and associated documentation files (the "Software"), to deal  #
# in the Software without restriction, including without limitation the rights   #
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell      #
# copies of the Software, and to permit persons to whom the Software is          #
# furnished to do so, subject to the following conditions:                       #
#                                                                                #
# The above copyright notice and this permission notice shall be included in all #
# copies or substantial portions of the Software.                                #
#                                                                                #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,       #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE    #
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER         #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,  #
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE  #
# SOFTWARE.                                                                      #
##################################################################################

import shared
import os
import argparse
import datetime

DATE = datetime.datetime.now()


def read_file(file):
    with open(file, "r") as source:
        return source.readlines()


def process_includes(source_lines, processed_lines, disabled_files, dir_in):
    for line in source_lines:
        if line.startswith("#include"):
            file_name = line.replace("#include ", "").replace("\"", "").replace("\n", "")
            if file_name not in disabled_files:
                disabled_files.add(file_name)
                process_includes(read_file(dir_in / file_name), processed_lines, disabled_files, dir_in)
        else:
            processed_lines.append(line)


def convert_file(file_prefix, file_in, file_out, dir_in):
    processed_file = []
    disabled_files = {"common_def.cl"}
    process_includes(read_file(file_in), processed_file, disabled_files, dir_in)

    with open(file_out, "w") as destination:
        destination.write("////////////////////////////////////////////////////////////////////\n")
        destination.write(f"// Copyright (c) 2021 - {DATE.date().year} SparseLinearAlgebra\n")
        destination.write(f"// Autogenerated file, do not modify\n")
        destination.write("////////////////////////////////////////////////////////////////////\n\n")
        destination.write("#pragma once\n\n")
        destination.write(f"static const char source_{file_prefix}[] = R\"(\n")
        destination.writelines(processed_file)
        destination.write("\n)\";")

    print(f"process cl file {file_prefix}")


def process_files(dir_in, dir_out):
    print(f"process directory {dir_in}")
    print(f"save to directory {dir_out}")
    for entry in os.listdir(dir_in):
        if os.path.isfile(dir_in / entry):
            file_prefix = entry.replace(".cl", "")
            file_in = dir_in / entry
            file_out = dir_out / f"auto_{file_prefix}.hpp"
            convert_file(file_prefix, file_in, file_out, dir_in)


def main():
    parser = argparse.ArgumentParser("Convert `.cl` sources into acceptable `.hpp` include files")
    parser.add_argument("--src", help="directory to process", default="./src/opencl/kernels")
    parser.add_argument("--dst", help="directory to save", default="./src/opencl/generated")
    args = parser.parse_args()
    src_dir = shared.ROOT / args.src
    dst_dir = shared.ROOT / args.dst
    os.makedirs(dst_dir, exist_ok=True)
    process_files(src_dir, dst_dir)


if __name__ == '__main__':
    main()
