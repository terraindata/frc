#pragma once

#include "xrc_settings.h"

namespace xrcpp::impl
{
class DecrementBuffer
{
private:
    size_t head, tail;
    void*  buffer[xrc_settings::per_thread_decrement_buffer_length];

    static constexpr auto mask = xrc_settings::per_thread_decrement_buffer_length - 1;

public:
    DecrementBuffer();

    bool enqueue(
        void* target
        ) noexcept
    {
        buffer[head] = target;
        head         = (head + 1) & mask;
        return (head == tail); //Buffer full!
    }
};
}
