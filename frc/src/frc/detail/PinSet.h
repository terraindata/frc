/*
 * File: thread_data.h
 * Copyright 2016-7 Terrain Data, Inc.
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

#pragma once

#include <frc/util/util.h>
#include <assert.h>
#include <frc/util/tls.h>

#include "FRCConstants.h"

namespace terrain {
    namespace frc {
        namespace detail {
            extern tls(atm<void*>*, head);

            extern bool isThreadRegistered() noexcept;

            class PinSet {
            private:
                static sz constexpr size = FRCConstants::pinSetSize;

            public:
                PinSet();

                // /**
                //  * acquires a pin
                //  */
                // static atm<void *> *acquire() noexcept {
                //     assert(isThreadRegistered());
                //     assert(head != nullptr); // all pins acquired
                //
                //     auto value = head;
                //     auto next = (atm<void *> *) value->load(orlx);
                //     head = next;
                //     return value;
                // }
                //
                //
                // /**
                //  * releases a pin
                //  */
                // static void release(atm<void *> *value) noexcept {
                //     assert(isThreadRegistered());
                //
                //     auto next = head;
                //     value->store(next, orls);
                //     head = value;
                // }

                sht acquireCounted() noexcept {
                    assert(isThreadRegistered());
                    assert(head_index < size); // all pins acquired

                    auto index = head_index;
                    head_index = counts[index];
                    counts[index] = 0;

                    // auto& pin = protectedObjects[index];
                    // auto next = reinterpret_cast<sht>(pin.load(orlx));
                    // pin.store(value, orls);
                    // head_index = next;

                    if (index == 0)
                        printf("acquire_counted() %d with next head %d\n", index, counts[index]);
                    return index;
                }

                void releaseCounted(sht index) noexcept {
                    if (index == 0)
                        printf("Released %d with count %d and head %d\n", index, counts[index], head_index);
                    //     return;

                    if (counts[index] > 0) {
                        counts[index]--;
                        return;
                    }

                    protectedObjects[index].store(nullptr, orls);
                    //printf("Released %d with count %d and head %d\n", index, counts[index], head_index);
                    counts[index] = head_index;
                    head_index = index;
                }

                sht acquireCountedFromRawPointer(void *value) noexcept {
                    // if (value == nullptr)
                    //     return 0;

                    sht index = acquireCounted();
                    protectedObjects[index].store(value, orls);
                    return index;
                }

                template<class V>
                sht acquireCountedFromSharedPointer(V const &that) noexcept {
                    sht index = acquireCounted();
                    auto &pin = protectedObjects[index];
                    pin.store(reinterpret_cast<void *>(detail::FRCConstants::busySignal), orls);
                    auto ptr = that.get(oacq);
                    pin.store(ptr, orls);
                    // if (ptr == nullptr) {
                    //     counts[index] = head_index;
                    //     head_index = index;
                    //     return 0;
                    // }
                    // printf("acquire_counted_from_shared_pointer() %d with count %d, next head %d\n", index,
                    //        counts[index], head_index);
                    return index;
                }

                sht copyCounted(sht index) noexcept {
                    if (index == 0)
                        printf("Copy count %d with count %d\n", index, counts[index]);
                    counts[index]++;
                    return index;
                }



                static void setProtectedPointer(atm<void *> *value) noexcept {
                    head = value;
                }

                static auto getProtectedPointer() noexcept {
                    return head;
                }

                bool isValid(void *ptr) noexcept {
                    return ptr != nullptr &&
                           ((sz) ptr < (sz) &protectedObjects[0] ||
                            (sz) ptr > (sz) &protectedObjects[size]);
                }

            public:
                // std::unique_ptr<atm<void*>[]> protectedObjects;
                sht head_index;
                sht counts[size];
                atm<void *> protectedObjects[size];
            };
        }
    }
}
