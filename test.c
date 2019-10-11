#include <assert.h>
#include <time.h>
#include <pthread.h>
#include "evbuffsock.h"

int first = 0;

void connect_callback(struct BufferedSocket *buffsock, void *arg){
    int cbarg = *(int *)arg;
    printf("%s: cbarg %d\n", __FUNCTION__, cbarg);
}
void close_callback(struct BufferedSocket *buffsock, void *arg){
    int cbarg = *(int *)arg;
    printf("%s: cbarg %d\n", __FUNCTION__, cbarg);
}
void read_callback(struct BufferedSocket *buffsock, struct Buffer *buf, void *arg){
    int cbarg = *(int *)arg;
    printf("%s: cbarg %d\n", __FUNCTION__, cbarg);
}
void ev_read_callback(EV_P_ struct ev_io *w, int revents){
    int cbarg = *(int *)w->data;
    printf("%s: cbarg %d\n", __FUNCTION__, cbarg);
    char buf[1024];
    int rc = 0;
    do{
        rc = read(w->fd, buf, 1024);
        printf("read %d bytes\n", rc);
    }while(rc > 0);
}
void ev_write_callback(EV_P_ struct ev_io *w, int revents){
    first = 1;
    // printf("%s: cbarg %d\n", __FUNCTION__, cbarg);
    dprintf(w->fd, "GET / HTTP/1.0\r\n\r\n");
}
void write_callback(struct BufferedSocket *buffsock, void *arg){
    int cbarg = *(int *)arg;
    first = 1;
    printf("%s: cbarg %d\n", __FUNCTION__, cbarg);
}
void async_write_callback(struct BufferedSocket *bs, void *arg){
    int cbarg = *(int *)arg;
    first = 1;
    struct Buffer *buf;    
    buf = new_buffer(256, 1024);

    char b[1024];
    size_t n;
    n = sprintf(b, "GET / HTTP/1.0\r\n\r\n");

    buffer_reset(buf);
    buffer_add(buf, b, n);
    buffered_socket_write_buffer(bs, buf);
    printf("%s: cbarg %d\n", __FUNCTION__, cbarg);
}
void error_callback(struct BufferedSocket *buffsock, void *arg){
    int cbarg = *(int *)arg;
    printf("%s: cbarg %d error %s\n", __FUNCTION__, cbarg, strerror(errno));
}

void *loop_thr(void *loop){
    srand(time(NULL));
    ev_loop((struct ev_loop*)loop, 0);
    printf("loop broken exiting thread\n");
    return NULL;
}

int main(int argc, char **argv)
{

    printf("async %p\n", async_write_callback);
    int cbarg = 9;
    struct ev_loop *loop = ev_loop_new(0);
    struct BufferedSocket *bs = new_buffered_socket(loop, "127.0.0.1", 56999,
        256, 1024, 256, 1024,
        connect_callback,
        close_callback,
        read_callback,
        write_callback,
        async_write_callback,
        error_callback,
        &cbarg);

    buffered_socket_connect(bs);

    pthread_t t;
    pthread_attr_t t_attr;
    pthread_attr_init(&t_attr);
    pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&t, &t_attr, loop_thr, loop);

    printf("calling for %p\n", bs->async_write_callback);
    int rc = buffered_socket_async_write(bs);

    while(rc < 0){
        buffered_socket_async_write(bs);
    }

    printf("rc %d\n", rc);

    sleep(100);
}
