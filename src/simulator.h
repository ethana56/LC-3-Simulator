#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <stdint.h>

#include "device.h"
#include "device_io.h"

#define LOW_ADDRESS  0
#define HGIH_ADDRESS UINT16_MAX

#define LOW_VECTOR_LOCATION  0
#define HIGH_VECTOR_LOCATION UINT8_MAX
#define LOW_PRIORITY         0
#define HIGH_PRIORITY        7

enum simulator_address_status {OUT_OF_BOUNDS, DEVICE_REGISTER, VALUE};

struct simulator;
typedef struct simulator Simulator;

enum simulator_address_status simulator_read_address(Simulator *, uint16_t, uint16_t *);
int simulator_run_until_end(Simulator *);
int simulator_load_program(Simulator *, int (*)(void *, uint16_t *), void *);
int simulator_attach_device(Simulator *, struct device *);
int simulator_load_program(Simulator *, int (*)(void *, uint16_t *), void *);

Simulator *simulator_new(struct device_io *);
void simulator_free(Simulator *);

#endif