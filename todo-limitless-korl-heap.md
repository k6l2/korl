- Approach 1:
    - wrap _Korl_Heap_* structs in a PTU
        - this will result in larger allocator sizes for normally smaller allocator types
- Approach 2:
    - change the korl_heap_*_create APIs to inject an extra X bytes in the address space preceding the allocator's resulting virtual address
        - where X is the # of bytes required to at least store a linked-list pointer to the next Korl_Heap address
    - this is kinda similar to how stb-ds arrays work
    - I'm leaning towards this option, as it seems like it is the least invasive to the korl-heap code, while still being flexible for whatever _Korl_Memory_Allocators need
    - uh oh... one issue I just realized is that korl-heap is imposing security settings on the page(s) containing the Korl_Heap data structure, which means we can't just get the pointer to the next korl-heap from this page! :(
    - ergo, it seems like we _must_ implement some kind of korl_heap_*_next() API if we're implementing a linked list of korl-heaps, as this would allow us to manage _Korl_Heap_* page security internally inside korl-heap, which _feels_ like a cleaner solution at first glance
- Approach 3:
    - allocate a separate chunk of virtual & physical memory to store a dynamically resizing list of allocator pools
        -ugh, this is probably the worst idea for various reasons...
- Approach 4: <- currently attempting this approach
    - manage a linked-list of `_Korl_Heap_*` structs entirely internal to korl-heap
    - when a call is about to fail due to the `_Korl_Heap_*` running out of memory or fragmenting, we simply attempt to perform the same call recursively on the next heap in the linked-list
    - if the next heap doesn't exist for the current `_Korl_Heap_*`, we simply create a new one

[x] modify `_Korl_Heap_Linear` to support an internal linked list
[x] modify `_Korl_Heap_General` to support an internal linked list
[x] make expanded heaps occupy 2x the space of their parent
[x] allow allocators to create an expanded heap if the requested allocation exceeds the capacity of the heap
[x] ensure that memory report functionality still works
[ ] ensure that save/load memory state functionality still works
