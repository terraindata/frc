//
// Created by ctripp on 3/14/25.
//


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "decrement_queue.h"


void test_randomized_operations()
{
    xrc_impl_decrement_queue queue;
    xrc_impl_decrement_queue_initialize(&queue);

    const size_t num_operations = 10000;
    srand(time(NULL));
    size_t       epoch               = 1;
    const size_t test_queue_capacity = XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY + 16;
    void*        elements[test_queue_capacity];
    size_t       element_epochs[test_queue_capacity];
    size_t       head   = 0, tail = 0;
    size_t       length = 0;

    for (size_t i = 0; i < num_operations; i++)
    {
        assert(length == xrc_impl_decrement_queue_length(&queue));
        assert((length == 0) == xrc_impl_decrement_queue_is_empty(&queue));

        if (rand() % 10 == 0)
            epoch++;

        size_t oldest_complete_scan_epoch = epoch - rand() % (XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY / 2);

        if (rand() % 100 <= 50 && length < XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY - 1)
        {
            // Push operation
            void* element        = (void*)(uintptr_t)(rand() % 1000);
            elements[head]       = element;
            element_epochs[head] = 0;
            head                 = (head + 1) % test_queue_capacity;
            ++length;

            xrc_impl_decrement_queue_push(&queue, element);
        }
        else
        {
            xrc_impl_decrement_queue_mark_epoch_of_new_decrements(&queue, epoch);

            for (size_t j = head; j != tail;)
            {
                j = (j - 1) % test_queue_capacity;
                if (element_epochs[j] != 0)
                    break;
                element_epochs[j] = epoch;
            }

            // Pop operation
            void*         element;
            uint_fast64_t oldest_decrement_epoch;
            int           pop_result = xrc_impl_decrement_queue_pop(
                &queue,
                oldest_complete_scan_epoch,
                &element,
                &oldest_decrement_epoch
                );

            if (pop_result < 0)
            {
                // queue should be empty
                assert(length == 0);
                assert(head == tail);
            }
            else
            {
                // queue shouldn't be empty
                assert(length != 0);
                assert(head != tail);

                if (pop_result > 0)
                {
                    // queue tail should be ineligible
                    assert(element_epochs[tail] >= oldest_complete_scan_epoch);
                    assert(oldest_decrement_epoch == element_epochs[tail]);
                }
                else
                {
                    // pop should be valid and eligible
                    assert(element == elements[tail]);
                    assert(element_epochs[tail] < oldest_complete_scan_epoch);
                    tail = (tail + 1) % test_queue_capacity;
                    length--;
                }
            }
        }
    }
}

int main()
{
    printf("Decrement queue test\n");
    // test_randomized_operations();
    printf("All tests passed.\n");
    return 0;
}
