/*
 * Copyright 2014, 2017 Terrain Data, Inc.
 *
 * This file is part of FRC, a fast reference counting library for C++
 * (see <https://github.com/terraindata/frc>).
 *
 * FRC is distributed under the MIT License, which is found in
 * COPYING.md in the repository.
 *
 * You should have received a copy of the MIT License
 * along with FRC.  If not, see <https://opensource.org/licenses/MIT>.
 */

#include "PinSet.h"

namespace terrain {
    namespace frc {
        namespace detail {
            tls(atm<void*>*, head);

            PinSet::PinSet() : head_index(1),
                               counts{0},
                               protectedObjects()
            // protectedObjects(new atm<void*>[size])
            {
                for (sz i = 1; i < size; ++i)
                    protectedObjects[i].store(&protectedObjects[i + 1], orlx);
                protectedObjects[size - 1].store(nullptr, orls);

                protectedObjects[0].store(nullptr, orls);
                head = &protectedObjects[1];

                for (sz i = 0; i < size; i++)
                    counts[i] = static_cast<sht>(i + 1);

                counts[0] = 1;
                // head_index = 0;
            }
        } /* namespace detail */
    } /* namespace frc */
} /* namespace terrain */
