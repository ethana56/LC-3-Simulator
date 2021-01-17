#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <pthread.h>

#define EV_STANDALONE 1
#include <ev.c>

#include "device_event_loop.h"

#include "util.h"

#define IO    0
#define TIMER 1

static char *revents_err_msg = "revents error";

struct dvel_watcher;

struct dvel {
    struct ev_loop *loop;
    struct dvel_watcher *watcher_queue_head;
    struct dvel_watcher *watcher_queue_tail;
    pthread_mutex_t queue_lock;
    pthread_mutex_t callback_lock;
    pthread_t thread;
    ev_async async_w;
    int thread_pipe[2];
};

struct dvel_watcher {
    ev_io io_watcher;
    struct dvel_watcher *next;
    struct dvel_watcher *prev;
    void (*callback)(int, void *);
    void *data;
};

static void *l_run(void *arg) {
    struct ev_loop *loop = arg;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
    ev_run(EV_A_ 0);
    return 0;
}

static void dvel_enqueue(Dvel *dvel, struct dvel_watcher *watcher) {
    if (dvel->watcher_queue_head == NULL && dvel->watcher_queue_tail == NULL) {
        dvel->watcher_queue_head = watcher;
	    dvel->watcher_queue_tail = watcher;
	    watcher->next = NULL;
	    return;
    }
    dvel->watcher_queue_tail->next = watcher;
    dvel->watcher_queue_tail = watcher;
}

static struct dvel_watcher *dvel_dequeue(Dvel *dvel) {
    struct dvel_watcher *watcher;
    watcher = dvel->watcher_queue_head;
    if (watcher == NULL) {
        return NULL;
    }
    dvel->watcher_queue_head = watcher->next;
    if (dvel->watcher_queue_head == NULL) {
        dvel->watcher_queue_tail = NULL;
    }
    watcher->next = NULL;
    return watcher;
}

static void async_callback(EV_P_ ev_async *async_watcher, int revents) {
    Dvel *dvel;
    struct dvel_watcher *cur_watcher;
    if (revents & EV_ERROR) {
        safe_write(STDERR_FILENO, revents_err_msg, sizeof(revents_err_msg) - 1);
	    abort();
    }
    dvel = async_watcher->data;
    pthread_mutex_lock(&dvel->queue_lock);
    while ((cur_watcher = dvel_dequeue(dvel)) != NULL) {
        ev_io_start(EV_A_ &cur_watcher->io_watcher);
    }
    pthread_mutex_unlock(&dvel->queue_lock);
}

static void dvel_io_callback(EV_P_ ev_io *watcher, int revents) {
    Dvel *dvel;
    struct dvel_watcher *dvel_watcher;
    dvel_watcher = (struct dvel_watcher *)watcher;
    if (revents & EV_ERROR) {
        safe_write(STDERR_FILENO, revents_err_msg, sizeof(revents_err_msg) - 1);
	    abort();
    } 
    dvel = ev_userdata(EV_A);
    pthread_mutex_lock(&dvel->callback_lock);
    dvel_watcher->callback(watcher->fd, dvel_watcher->data);
    pthread_mutex_unlock(&dvel->callback_lock);
}

void dvel_lock(Dvel *dvel) {
    pthread_mutex_lock(&dvel->callback_lock);
}

void dvel_unlock(Dvel *dvel) {
    pthread_mutex_unlock(&dvel->callback_lock);
}

int dvel_add_listener_read(Dvel *dvel, int fd, void (*callback)(int, void *), void *data) {
    struct dvel_watcher *watcher;
    struct ev_loop *loop;
    ev_io *ev_io_watcher;
    loop = dvel->loop;
    watcher = malloc(sizeof(struct dvel_watcher));
    if (watcher == NULL) {
        return -1;
    }
    ev_io_watcher = &watcher->io_watcher;
    ev_init(ev_io_watcher, dvel_io_callback);
    ev_io_set(ev_io_watcher, fd, EV_READ);
    /* TODO CHECK DICTIONARY */
    /* setting caller data */
    watcher->data = data;
    watcher->callback = callback;
    pthread_mutex_lock(&dvel->queue_lock);
    dvel_enqueue(dvel, watcher);
    pthread_mutex_unlock(&dvel->queue_lock);
    ev_async_send(EV_A_ &dvel->async_w);
    return 0;
}

void dvel_free(Dvel *dvel) {
   
}

Dvel *dvel_new(void) {
    Dvel *dvel;
    struct ev_loop *loop;
    dvel = malloc(sizeof(Dvel));
    if (dvel == NULL) {
        goto dvel_alloc_err;
    }
    loop = ev_loop_new(EVFLAG_AUTO);
    if (!loop) {
        goto loop_init_err;
    }
    dvel->loop = loop;
    if (pthread_mutex_init(&dvel->queue_lock, NULL) < 0) {
        goto queue_lock_init_err;
    }
    if (pthread_mutex_init(&dvel->callback_lock, NULL) < 0) {
        goto callback_lock_init_err;
    }
    ev_set_userdata(EV_A_ dvel);
    dvel->watcher_queue_head = NULL;
    dvel->watcher_queue_tail = NULL;

    ev_async_init(&dvel->async_w, async_callback);
    dvel->async_w.data = dvel;
    ev_async_start(EV_A_ &dvel->async_w);
    if (pthread_create(&dvel->thread, NULL, l_run, EV_A) < 0) {
        goto thread_create_err;
    }
    return dvel;

thread_create_err:
    ev_async_stop(EV_A_ &dvel->async_w);
    pthread_mutex_destroy(&dvel->callback_lock);
callback_lock_init_err:
    pthread_mutex_destroy(&dvel->queue_lock);
queue_lock_init_err:
    ev_loop_destroy(EV_A);
loop_init_err:
    free(dvel);
dvel_alloc_err:
    return NULL;     
}

























