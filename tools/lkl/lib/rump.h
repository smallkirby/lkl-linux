/*
 * Rump hypercall interface for Linux
 * Copyright (c) 2015 Hajime Tazaki
 *
 * Author: Hajime Tazaki <thehajime@gmail.com>
 */

#ifndef __dead
#define __dead
#endif
#ifndef __printflike
#define __printflike(x, y)
#endif
#include <rump/rumpuser.h>

struct irq_data;

void rump_exit(void);

void rump_sysproxy_init(void);
void rump_sysproxy_fini(void);

extern const struct rumpuser_hyperup hyp;

int rump_pci_irq_request(struct irq_data *data);
void rump_pci_irq_release(struct irq_data *data);

void *rumpuser_thread_self(void);
void *rumpuser_getcookie(void);
void rumpuser_setcookie(void* cookie);
