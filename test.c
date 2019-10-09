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

int conn(char *address, int port){
    int sock = -1, ret;
    struct addrinfo hints, *p, *dstinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    char port_str[6];
    int rc = snprintf(port_str, 6, "%d", port);
    if(rc < 0){
        return -7;
    }

    if((ret = getaddrinfo(address, port_str, &hints, &dstinfo)) != 0){
        return -2;
    }

    for(p = dstinfo; p != NULL; p = p->ai_next){
        if((sock = socket(p->ai_family, p->ai_socktype | SOCK_CLOEXEC, p->ai_protocol)) == -1){
            continue;
        }
        break;
    }

    int flags = 0;
    if ((flags = fcntl(sock, F_GETFL, NULL)) < 0) {
        close(sock);
        return 0;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        close(sock);
        return 0;
    }

retry:
    if(connect(sock, dstinfo->ai_addr, dstinfo->ai_addrlen) == -1){
        if(errno == 115){
            goto retry;
        }
        close(sock);
        freeaddrinfo(dstinfo);
        printf("%d\n", errno);
        return -3;
    }

    freeaddrinfo(dstinfo);
    return sock;
}

typedef struct loop_data {
    struct ev_loop *loop;
    struct ev_io *write_ev;
    struct ev_io *read_ev;
}u_data;

void enable_write(EV_P_ ev_async *w, int revents){
    u_data *dat = (u_data *)w->data;
    // printf("loop %p write_ev %p\n", loop, dat->write_ev);
    ev_io_start(loop, dat->write_ev);
}

int main(int argc, char **argv)
{
    int cbarg = 9;
    struct ev_loop *loop = ev_loop_new(0);

    // int fd = conn("127.0.0.1", 56999);

    // if(fd <= 0){
    //     printf("[panic]\n");
    //     exit(1);
    // }

    // u_data dat;

    // struct ev_io write_ev;
    // dat.write_ev = &write_ev;
    // write_ev.data = &cbarg;
    // ev_init(&write_ev, ev_write_callback);
    // ev_io_set(&write_ev, fd, EV_WRITE);
    // ev_io_start(loop, &write_ev);

    // struct ev_io read_ev;
    // dat.read_ev = &read_ev;
    // read_ev.data = &cbarg;
    // ev_init(&read_ev, ev_read_callback);
    // ev_io_set(&read_ev, fd, EV_READ);
    // ev_io_start(loop, &read_ev);

    // struct ev_async wake;
    // dat.loop = loop;
    // wake.data = &dat;
    // ev_async_init(&wake, enable_write);
    // ev_async_start(loop, &wake);

    // sleep(5);
    // printf("write event ran for 1 sec stop\n");
    // printf("loop %p write_ev %p\n", loop, &write_ev);
    // ev_io_stop(loop, &write_ev);

    // sleep(5);
    // printf("waited another sec start it again\n");
    // ev_async_send(loop, &wake);
    // printf("started again\n");

    struct Buffer *buf;
    
    buf = new_buffer(256, 1024);

    struct BufferedSocket *bs = new_buffered_socket(loop, "127.0.0.1", 56999,
        256, 1024, 256, 1024,
        connect_callback,
        close_callback,
        read_callback,
        write_callback,
        error_callback,
        &cbarg);

    buffered_socket_connect(bs);

    pthread_t t;
    pthread_attr_t t_attr;
    pthread_attr_init(&t_attr);
    pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&t, &t_attr, loop_thr, loop);

    // printf("bs: %p\n", bs);

    char b[1024];
    size_t n;
    n = sprintf(b, "GET / HTTP/1.0\r\n\r\n");
    buffer_reset(buf);
    buffer_add(buf, b, n);

    sleep(5);
    buffered_socket_write_buffer(bs, buf);

    while(1){
        buffer_reset(buf);
        buffer_add(buf, b, n);
        buffered_socket_write_buffer(bs, buf);
        sleep(1);
    }
}
