#!/bin/bash

# File: apply_style.sh
# Copyright 2017 Terrain Data, Inc.
#
# This file is part of FRC, a fast reference counting library for C++
# (see <https://github.com/terraindata/frc>).
#
# FRC is distributed under the MIT License, which is found in
# COPYING.md in the repository.
#
# You should have received a copy of the MIT License
# along with FRC.  If not, see <https://opensource.org/licenses/MIT>.

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

astyle=astyle

force=

if [ "$1" == "-f" ];
then
	 force=1
	 shift
fi

# override path to astyle
if [ "$1" == "--astyle" ];
then
	astyle=$2
	shift
	shift
fi

extraopts=
if [ "$1" == "--dry-run" ];
then
	 extraopts="$extraopts --dry-run"
	 shift
fi

# Use astyle to format C++ code
$astyle -r -X -Q -Z -n --formatted --ignore-exclude-errors-x --lineend=linux --style=allman --indent=spaces=4 --indent-switches --indent-labels --indent-col1-comments --pad-oper --unpad-paren --align-pointer=type --align-reference=type  --keep-one-line-statements --max-code-length=95 'src/*.cpp' 'src/*.h' 'test/*.cpp' 'test/*.h' 'benchmark/*.cpp' 'benchmark/*.h' $extraopts

