#!/bin/bash

# File: apply_style.sh
# Copyright 2017 Terrain Data, Inc.
#
# This file is part of FRC, a fast reference counting library for C++
# (see https://github.com/terraindata/frc).
#
# FRC is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# FRC is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with FRC.  If not, see <http://www.gnu.org/licenses/>.

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

