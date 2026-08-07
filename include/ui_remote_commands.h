#ifndef UI_REMOTE_COMMANDS_H
#define UI_REMOTE_COMMANDS_H

#include <pthread.h>
#include <stddef.h>

typedef struct {
    int recording_target;      /* -1 unchanged, 0 stop, 1 start */
    int parking_guides_target; /* -1 unchanged, 0 disable, 1 enable */
    int take_photo;
} UiRemoteCommandRequest;

typedef struct {
    pthread_mutex_t mutex;
    int initialized;
    UiRemoteCommandRequest pending;
} UiRemoteCommands;

int ui_remote_commands_init(UiRemoteCommands *commands);
void ui_remote_commands_destroy(UiRemoteCommands *commands);
void ui_remote_commands_post(UiRemoteCommands *commands,
                             const UiRemoteCommandRequest *request);
void ui_remote_commands_take(UiRemoteCommands *commands,
                             UiRemoteCommandRequest *request);
int ui_remote_commands_has_live_request(UiRemoteCommands *commands);

/* WeatherTextHandler-compatible callback for the shared TCP receiver. */
int ui_remote_commands_handle_text(const char *message, char *reply,
                                   size_t reply_size, void *user_data);

#endif
