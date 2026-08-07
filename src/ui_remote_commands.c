#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ui_remote_commands.h"

static void clear_request(UiRemoteCommandRequest *request)
{
    request->recording_target = -1;
    request->parking_guides_target = -1;
    request->take_photo = 0;
}

/* Accept a natural-language reply, but keep whitespace-padded protocol
 * commands invalid as they were before. */
static int contains_natural_command(const char *message, const char *command)
{
    const char *match = strstr(message, command);
    size_t command_length = strlen(command);

    if (match == NULL)
        return 0;
    if (strcmp(message, command) == 0)
        return 1;
    for (const char *cursor = message; cursor < match; ++cursor)
    {
        if (!isspace((unsigned char)*cursor))
            return 1;
    }
    for (const char *cursor = match + command_length; *cursor != '\0'; ++cursor)
    {
        if (!isspace((unsigned char)*cursor))
            return 1;
    }
    return 0;
}

int ui_remote_commands_init(UiRemoteCommands *commands)
{
    if (commands == NULL || pthread_mutex_init(&commands->mutex, NULL) != 0)
        return -1;
    commands->initialized = 1;
    clear_request(&commands->pending);
    return 0;
}

void ui_remote_commands_destroy(UiRemoteCommands *commands)
{
    if (commands != NULL && commands->initialized)
    {
        pthread_mutex_destroy(&commands->mutex);
        commands->initialized = 0;
    }
}

void ui_remote_commands_post(UiRemoteCommands *commands,
                             const UiRemoteCommandRequest *request)
{
    if (commands == NULL || request == NULL || !commands->initialized)
        return;
    pthread_mutex_lock(&commands->mutex);
    if (request->recording_target >= 0)
        commands->pending.recording_target = request->recording_target;
    if (request->parking_guides_target >= 0)
        commands->pending.parking_guides_target = request->parking_guides_target;
    if (request->take_photo)
        commands->pending.take_photo = 1;
    pthread_mutex_unlock(&commands->mutex);
}

void ui_remote_commands_take(UiRemoteCommands *commands,
                             UiRemoteCommandRequest *request)
{
    if (request == NULL)
        return;
    clear_request(request);
    if (commands == NULL || !commands->initialized)
        return;
    pthread_mutex_lock(&commands->mutex);
    *request = commands->pending;
    clear_request(&commands->pending);
    pthread_mutex_unlock(&commands->mutex);
}

int ui_remote_commands_has_live_request(UiRemoteCommands *commands)
{
    int requested = 0;

    if (commands == NULL || !commands->initialized)
        return 0;
    pthread_mutex_lock(&commands->mutex);
    requested = commands->pending.recording_target == 1 ||
                commands->pending.take_photo;
    pthread_mutex_unlock(&commands->mutex);
    return requested;
}

int ui_remote_commands_handle_text(const char *message, char *reply,
                                   size_t reply_size, void *user_data)
{
    UiRemoteCommands *commands = user_data;
    UiRemoteCommandRequest request;
    const char *matched_command = NULL;
    int accepted = 1;

    if (message == NULL || reply == NULL || reply_size == 0 || commands == NULL)
        return -1;
    clear_request(&request);
    /* The AI client sends its natural-language reply to the board. Keep the
     * exact protocol commands, while accepting the action phrase in a reply
     * such as "已收到，开始录像。". */
    if (contains_natural_command(message, "开始录像"))
    {
        request.recording_target = 1;
        matched_command = "开始录像";
    }
    else if (contains_natural_command(message, "停止录像"))
    {
        request.recording_target = 0;
        matched_command = "停止录像";
    }
    else if (contains_natural_command(message, "拍照"))
    {
        request.take_photo = 1;
        matched_command = "拍照";
    }
    else if (contains_natural_command(message, "开启倒车线"))
    {
        request.parking_guides_target = 1;
        matched_command = "开启倒车线";
    }
    else if (contains_natural_command(message, "关闭倒车线"))
    {
        request.parking_guides_target = 0;
        matched_command = "关闭倒车线";
    }
    else
        accepted = 0;

    if (accepted)
    {
        ui_remote_commands_post(commands, &request);
        fprintf(stderr, "[Remote] accepted command=%s text=%s\n",
                matched_command, message);
        snprintf(reply, reply_size, "%s", "OK command accepted\n");
    }
    else
    {
        fprintf(stderr, "AI text received: %s\n", message);
        snprintf(reply, reply_size, "%s", "OK text received\n");
    }
    return 0;
}
