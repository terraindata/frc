//#include <iostream>
//#include <atomic>
//#include <xmmintrin.h>
//#include <immintrin.h>
//
//class ObjectHeader {
////  unsigned short objectOffset;
////  unsigned short xrcTypeCode;
//  std::atomic<unsigned int> count;
//
//
//public:
//    virtual ~ObjectHeader() = default;
////    virtual void baseMethod() {}
//};
//
//class TestObject {
//public:
//    unsigned long data;
////    __m256 data;
//
//    virtual ~TestObject() = default;
//    virtual void method() {}
//};
//
//class ObjectContainer : public ObjectHeader {
//public:
//    TestObject obj;
//
//
//    // Overrides ObjectHeader's destructor, but does NOT introduce new virtual methods
////    ~ObjectContainer() override {}
//};


#include <iostream>
#include <cstddef>  // For std::size_t
#include <cstdint>  // For uintptr_t

template <typename T, typename H>
constexpr std::size_t findOffsetAbove() {
    constexpr std::size_t alignT = alignof(T);
    constexpr std::size_t alignH = alignof(H);
    constexpr std::size_t sizeT = sizeof(T);

    // Find the aligned base address for H (assumed at address X)
    constexpr std::uintptr_t alignedH = 0; // Assume H is at address 0 for computation

    // Compute the highest address below `alignedH` that is still aligned for `T`
    constexpr std::uintptr_t alignedT = (alignedH - sizeT) & ~(alignT - 1);

    // Compute the offset from `alignedH` to `alignedT`
    return alignedH - alignedT;
}

class H {
    alignas(16) int data[4];  // Example with strict alignment
};

class T {
    alignas(32) double values[4];  // Needs 32-byte alignment
};

int main() {
    constexpr std::size_t offset = findOffsetAbove<T, H>();
    std::cout << "Offset to place T above H: " << offset << " bytes\n";
}