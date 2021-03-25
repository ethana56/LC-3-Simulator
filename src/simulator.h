#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <stdint.h>

#define LOW_ADDRESS  0
#define HGIH_ADDRESS UINT16_MAX

#define LOW_VECTOR_LOCATION  0
#define HIGH_VECTOR_LOCATION UINT8_MAX
#define LOW_PRIORITY         0
#define HIGH_PRIORITY        7

struct simulator;
typedef struct simulator Simulator;

int simulator_bind_addresses(Simulator *, uint16_t *, size_t, uint16_t (*)(uint16_t), void (*)(uint16_t, uint16_t));
int simulator_bind_addresses_range(Simulator *, uint16_t, uint16_t, uint16_t (*)(uint16_t), void (*)(uint16_t, uint16_t));
void simulator_unbind_all_addresses(Simulator *);
void simulator_subscribe_on_tick(Simulator *, void (*)(void *), void *);
void simulator_run_until_end(Simulator *);
int simulator_load_program(Simulator *, int (*)(void *, uint16_t *), void *);

Simulator *simulator_new(void);
void simulator_free(Simulator *);

#endif