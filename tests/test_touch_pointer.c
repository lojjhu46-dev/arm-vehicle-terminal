#include <assert.h>
#include <linux/input.h>
#include <pthread.h>
#include <unistd.h>

#include "app_config.h"
#include "touch.h"

static void send_event(int fd, unsigned short type, unsigned short code,
                       int value)
{
    struct input_event event;

    event.type = type;
    event.code = code;
    event.value = value;
    assert(write(fd, &event, sizeof(event)) == (ssize_t)sizeof(event));
}

static void expect_pointer(Touch *touch, int expected_x, int expected_y,
                           int expected_pressed)
{
    int x = -1;
    int y = -1;
    int pressed = -1;

    assert(touch_read_pointer(touch, &x, &y, &pressed) == 0);
    assert(x == expected_x);
    assert(y == expected_y);
    assert(pressed == expected_pressed);
}

int main(void)
{
    int pipe_fds[2];
    Touch touch;
    int x;
    int y;

    assert(pipe(pipe_fds) == 0);
    touch.fd = pipe_fds[0];
    touch.touchsrceen_x = 0;
    touch.touchsrceen_y = 0;
    touch.touch_active = 0;
    assert(pthread_mutex_init(&touch.mutex, NULL) == 0);

    send_event(pipe_fds[1], EV_ABS, ABS_X, 512);
    expect_pointer(&touch, 400, 0, 0);
    send_event(pipe_fds[1], EV_ABS, ABS_Y, 300);
    expect_pointer(&touch, 400, 240, 0);
    send_event(pipe_fds[1], EV_KEY, BTN_TOUCH, 1);
    expect_pointer(&touch, 400, 240, 1);
    send_event(pipe_fds[1], EV_ABS, ABS_Y, 100);
    expect_pointer(&touch, 400, 80, 1);
    send_event(pipe_fds[1], EV_KEY, BTN_TOUCH, 0);
    expect_pointer(&touch, 400, 80, 0);

    send_event(pipe_fds[1], EV_KEY, BTN_TOUCH, 1);
    assert(touch_read_tap(&touch, &x, &y) == 0);
    send_event(pipe_fds[1], EV_KEY, BTN_TOUCH, 0);
    assert(touch_read_tap(&touch, &x, &y) == 1);
    assert(x == 400 && y == 80);

    close(pipe_fds[1]);
    touch_close(&touch);
    return 0;
}
