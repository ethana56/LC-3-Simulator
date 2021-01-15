#ifndef DEVICE_EVENT_LOOP_H
#define DEVICE_EVENT_LOOP_H

struct dvel;
typedef struct dvel Dvel;

Dvel *dvel_new(void);
void dvel_free(Dvel *);
void dvel_lock(Dvel *);
void dvel_unlock(Dvel *);
int dvel_add_listener_read(Dvel *, int, void (*)(int, void *), void *);

#endif
