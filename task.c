#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

const int PAGE_SIZE = 4096;
const int ALIGN = 8;

char *map;
int total_map_bits;
char *data;
int next_map_bit = 0;

unsigned long div_ceil(unsigned long x, unsigned long y)
{
    return (x + y - 1) / y;
}

void *get_aligned_address(void *min_address, int align)
{
    unsigned long blocks = div_ceil((uintptr_t)min_address, align);
    return (void *)(blocks * align);
}

void set_map_bit(int map_bit, bool value)
{
    int map_byte = map_bit / 8;
    int lsb_index = 7 - map_bit % 8;
    if (value)
    {
        map[map_byte] |= 1 << lsb_index;
    }
    else
    {
        map[map_byte] &= ~(1 << lsb_index);
    }
}

void set_map_bits(int start_map_bit, int size, bool value)
{
    for (int i = start_map_bit; i < start_map_bit + size; i++)
    {
        set_map_bit(i, value);
    }
}

bool get_map_bit(int map_bit)
{
    int map_byte = map_bit / 8;
    int lsb_index = 7 - map_bit % 8;
    bool value = (bool)(map[map_byte] & (1 << lsb_index));
    return value;
}

bool test_free_map_bits(int start_map_bit, int bits)
{
    if (start_map_bit + bits > total_map_bits)
    {
        return false;
    }

    for (int i = start_map_bit; i < start_map_bit + bits; i++)
    {
        bool marked = get_map_bit(i);
        if (marked)
        {
            return false;
        }
    }
    return true;
}

int find_free_map_bits(int bits, int align)
{
    char *next_address = data - 1;
    int start_map_bit;
    do
    {
        next_address = get_aligned_address(next_address + 1, align);
        start_map_bit = ((uintptr_t)next_address - (uintptr_t)data) / ALIGN;
        if (start_map_bit + bits > total_map_bits)
        {
            return -1;
        }
        if (test_free_map_bits(start_map_bit, bits))
        {
            return start_map_bit;
        }
    } while (start_map_bit < total_map_bits);

    return -1;
}

int simple_memory_allocator_init(char *memory_allocator_buf, unsigned int memory_allocator_buf_size)
{
    memory_allocator_buf_size =
        memory_allocator_buf_size <= PAGE_SIZE ? memory_allocator_buf_size : PAGE_SIZE;

    map = memory_allocator_buf;
    int bruto_map_bits = div_ceil(memory_allocator_buf_size, ALIGN);
    int map_size = div_ceil(bruto_map_bits, 8);
    data = get_aligned_address(map + map_size, ALIGN);

    int neto_size = memory_allocator_buf_size - ((uintptr_t)data - (uintptr_t)map);
    if (neto_size <= 0)
    {
        printf("Too small allocation\n");
        return -1;
    }

    total_map_bits = div_ceil(neto_size, ALIGN);
    memset(map, 0, map_size);

    printf("Neto allocated size: %d bytes\n", neto_size);
    return 0;
}

void *alloc_on_next_map_bit(int num_map_bits, int align)
{
    if (next_map_bit >= total_map_bits)
    {
        next_map_bit = 0;
    }
    char *address = get_aligned_address(&data[ALIGN * next_map_bit], align);
    int start_map_bit = ((uintptr_t)address - (uintptr_t)data) / ALIGN;

    if (test_free_map_bits(start_map_bit, num_map_bits))
    {
        set_map_bits(start_map_bit, num_map_bits, true);
        next_map_bit += num_map_bits;
        return address;
    }

    return NULL;
}

void *simple_memory_allocator_alloc_aligned(unsigned int alloc_size, int align)
{
    if (align % ALIGN != 0)
    {
        printf("invalid alignment\n");
        return NULL;
    }

    int num_map_bits = div_ceil(alloc_size, ALIGN);
    void *address = alloc_on_next_map_bit(num_map_bits, align);
    if (address != NULL)
    {
        return address;
    }

    int start_map_bit = find_free_map_bits(num_map_bits, align);
    if (start_map_bit < 0)
    {
        printf("Not enough continuous space\n");
        return NULL;
    }

    set_map_bits(start_map_bit, num_map_bits, true);
    next_map_bit += num_map_bits;
    return &data[ALIGN * start_map_bit];
}

void *simple_memory_allocator_alloc(unsigned int alloc_size)
{
    return simple_memory_allocator_alloc_aligned(alloc_size, ALIGN);
}

void simple_memory_allocator_free(void *ptr, unsigned int alloc_size)
{
    if (!ptr)
    {
        return;
    }

    int start_map_bit = ((uintptr_t)ptr - (uintptr_t)data) / ALIGN;
    int num_map_bits = div_ceil(alloc_size, ALIGN);
    if (start_map_bit >= 0 && start_map_bit + num_map_bits <= total_map_bits)
    {
        set_map_bits(start_map_bit, num_map_bits, false);
        next_map_bit = start_map_bit;
        while (next_map_bit > 0 && !get_map_bit(next_map_bit - 1))
        {
            next_map_bit--;
        }
    }
}

int main()
{
    char *buffer = malloc(PAGE_SIZE);

    int error = simple_memory_allocator_init(buffer, 3000);
    if (error)
    {
        return error;
    }

    void *p1 = simple_memory_allocator_alloc(17);
    void *p2 = simple_memory_allocator_alloc_aligned(99, 128);
    void *p3 = simple_memory_allocator_alloc(9);
    void *p4 = simple_memory_allocator_alloc_aligned(120, 128);
    void *p5 = simple_memory_allocator_alloc(128);
    void *p6 = simple_memory_allocator_alloc_aligned(2528, 16);
    void *p7 = simple_memory_allocator_alloc_aligned(2520, 16);
    simple_memory_allocator_free(p7, 2520);
    void *p8 = simple_memory_allocator_alloc(2520);

    free(buffer);
    return 0;
}
