#ifndef MEMORY_H
#define MEMORY_H

struct memory;
typedef struct memory Memory;

struct io_register {
    void *data;
    uint16_t address;
    void (*write)(struct io_register *, uint16_t);
    uint16_t (*read)(struct io_register *);
};

uint16_t read_memory(Memory *, uint16_t);
void write_memory(Memory *, uint16_t, uint16_t);
void memory_register_io_register(Memory *, struct io_register *);
Memory *new_Memory(void);
void free_memory(Memory *);

#endif
