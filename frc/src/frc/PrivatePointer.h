/*
 * File: PrivatePointer.h
 * Copyright 2016-2018 Terrain Data, Inc.
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

#include <frc/detail/FRCConstants.h>
#include "AtomicPointer.h"
#include "SharedPointer.h"

namespace terrain {
    namespace frc {
        template<class T>
        class PrivatePointer;

        template<class T>
        class SharedPointer;

        template<class T>
        class AtomicPointer;

        /**
         * TODO: test stack-allocation based private pointers
         * Doing so may simplify setting and copying/assigning pointers by preventing values from "escaping" detection
         * during a pin set scan. If the pin set is scanned top to bottom, then
         */

        /**
         * Skinny and fast protected set pointer. Good for most applications.
         *
         * An object of this class will prevent the object/memory pointed to from being
         * collected.
         *
         * The main use for protected set pointers is as stack-resident pointers
         * when traversing data structures. This prevents the objects being
         * read from being reclaimed out from under the reader.
         *
         * There is a limit (currently 1024) to the number of pins allowed at one
         * time for a particular thread. For performance reasons,
         * there is no safeguard in place to prevent over-allocations of pins beyond
         * this limit. Typical applications will only pin a handful of things.
         *
         * T is the type of the object being pointed to.
         */
        template<class T>
        class PrivatePointer {
        private:
            template<class V>
            friend
            class PrivatePointer;

            template<class V>
            friend
            class SharedPointer;

            template<class V>
            friend
            class AtomicPointer;

        private:
            sht index;

        public:
            //PrivatePointer() noexcept : index(0) {
            //}
            PrivatePointer() noexcept : index(getPinSet().acquireCountedFromRawPointer(nullptr)) {
            }

            /**
            * Move constructor.
            */
            template<class V>
            PrivatePointer(PrivatePointer &&that) noexcept : index(
                getPinSet().copyCounted(that.index)) {
            }

            //            template<class V>
            //            PrivatePointer(PrivatePointer &&that) noexcept : index(that.index) {
            //                that.index = 0;
            //            }

            /**
            * Copy constructor.
            */
            template<class V>
            PrivatePointer(PrivatePointer<V> const &that) noexcept : index(
                getPinSet().copyCounted(that.index)) {
            }

            template<class V>
            PrivatePointer(SharedPointer<V> const &that) noexcept : index(
                getPinSet().acquireCountedFromSharedPointer(that)) {
            }

            template<class V>
            PrivatePointer(AtomicPointer<V> const &that) noexcept : index(
                getPinSet().acquireCountedFromSharedPointer(that)) {
            }

            template<class V>
            PrivatePointer(AtomicPointer<V> &that) noexcept : PrivatePointer((AtomicPointer<V> const &) that) {
                ;
            }

            template<class V>
            PrivatePointer(SharedPointer<V> &that) noexcept : PrivatePointer((SharedPointer<V> const &) that) {
                ;
            }

            PrivatePointer(AtomicPointer<T> const &that) noexcept : index(
                getPinSet().acquireCountedFromSharedPointer(that)) {
            }

            PrivatePointer(SharedPointer<T> const &that) noexcept : index(
                getPinSet().acquireCountedFromSharedPointer(that)) {
            }

            template<class... Args>
            explicit PrivatePointer(Args &&... args) : index(doEmplace<T>(std::forward<Args>(args)...)) {
            }

            template<class V, class... Args>
            explicit PrivatePointer(Args &&... args) : index(doEmplace<V>(std::forward<Args>(args)...)) {
            }

            ~PrivatePointer() noexcept {
                // while (detail::ThreadData::isScanning());
                getPinSet().releaseCounted(index);
            }

            template<class... Args>
            void make(Args &&... args) {
                makeType<T>(std::forward<Args>(args)...);
            }

            template<class V, class... Args>
            void makeType(Args &&... args) {
                auto ptr = detail::makeNewObject<V>(1, std::forward<Args>(args)...);

                auto &pinSet = getPinSet();
                auto old_index = index;
                index = pinSet.acquireCountedFromRawPointer(ptr);
                pinSet.releaseCounted(old_index);
                detail::registerDecrement(ptr);
            }

            // void makeArray(sz length, bool initialize = true) {
            //     doEmplace(detail::makeNewArray<T>(1, length, initialize));
            // }

            T &operator*() const noexcept {
                return *get();
            }

            T *operator->() const noexcept {
                return get();
            }

            T &operator[](sz idx) const noexcept {
                assert(idx < length());
                return get()[idx];
            }

            T *get() const noexcept {
                //if (index == 0)
                //    return nullptr;
                return static_cast<T *>(getPinSet().protectedObjects[index].load(orlx));
            }

            sz length() const noexcept {
                return detail::getObjectHeader(get())->length();
            }

            PrivatePointer &operator=(std::nullptr_t const) noexcept {
                reset();
            }

            template<class V>
            PrivatePointer &operator=(PrivatePointer<V> const &that) noexcept {
                auto &pinSet = getPinSet();
                auto old_index = index;
                index = pinSet.copyCounted(that.index);
                pinSet.releaseCounted(old_index);
                return *this;
            }

            // template<class V>
            // PrivatePointer &operator=(PrivatePointer<V> &&that) noexcept {
            //     return swap(that);
            // }

            template<class V>
            PrivatePointer &operator=(SharedPointer<V> const &that) noexcept {
                return set(that);
            }

            template<class V>
            PrivatePointer &operator=(AtomicPointer<V> const &that) noexcept {
                return set(that);
            }

            template<class V>
            bool operator==(V *const that) const noexcept {
                return get() == that;
            }

            bool operator==(std::nullptr_t const) const noexcept {
                return get() == nullptr;
            }

            template<class V>
            bool operator==(PrivatePointer<V> const &that) const noexcept {
                return that.get() == get();
            }

            template<class V>
            bool operator==(AtomicPointer<V> const &that) const noexcept {
                return that.get() == get();
            }

            template<class V>
            bool operator!=(V const &that) const noexcept {
                return !(*this == that);
            }

        public:
            friend void swap(PrivatePointer &a, PrivatePointer &b) noexcept {
                auto tmp = a.index;
                a.index = b.index;
                b.index = tmp;
            }

            void swap(PrivatePointer &that) noexcept {
                swap(*this, that);
            }

            void reset() noexcept {
                //                 getPinSet().release_counted(index);
                //                 index = 0;
                auto &pinSet = getPinSet();
                auto old_index = index;
                index = pinSet.acquireCountedFromRawPointer(nullptr);
                pinSet.releaseCounted(old_index);
            }

            size_t use_count() const noexcept {
                T *ptr = get();
                if (!ptr)
                    return 0;
                return detail::getObjectHeader(ptr)->getCount();
            }

            explicit operator bool() const noexcept {
                return get() != nullptr;
            }

        private:
            static detail::PinSet &getPinSet() noexcept {
                return detail::threadData->pinSet;
            }

            PrivatePointer &set(std::nullptr_t) noexcept {
                reset();
            }

            template<class V>
            PrivatePointer &set(V const &that) noexcept {
                auto &pinSet = getPinSet();
                auto old_index = index;
                index = pinSet.acquireCountedFromSharedPointer(that);
                pinSet.releaseCounted(old_index);
                return *this;
            }

            /**
             * Strategies with the first increment:
             * + Create object with count = 1
             *  - Must register a decrement or put in ZCT to check if set to a shared ptr
             * + Create object with count = 0 **using this one**
             *  - Must register an increment when set to a shared ptr
             * + Create object with count = 1 and decrement if not set
             *  + minimizes work done in the common case
             *  - must keep additional state information about private vs shared status
             */
            template<class V, class... Args>
            static auto doEmplace(Args &&... args) noexcept {
                auto &pinSet = getPinSet();
                //auto old_index = index;
                auto ptr = detail::makeNewObject<V>(1, std::forward<Args>(args)...);
                auto index = pinSet.acquireCountedFromRawPointer(ptr);
                //pinSet.releaseCounted(old_index);
                detail::registerDecrement(ptr);
                return index;
            }

            T *setCountedPointer() const noexcept {
                T *ptr = get();
                detail::registerIncrement(ptr);
                return ptr;
            }
        };
    } /* namespace frc */
} /* namespace terrain */

namespace std {
    /**
     * std lib specialization of std::hash for SharedPointer's
     */
    template<class T>
    struct hash<terrain::frc::PrivatePointer<T> > {
        std::size_t operator()(const terrain::frc::PrivatePointer<T> &k) const {
            return hash<T *>()(k.get());
        }
    };
}
