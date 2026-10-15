#include <assert.h>
#include <string.h>

#include "ui_remote_commands.h"

static void expect_command(UiRemoteCommands *commands, const char *text,
                           int recording_target, int guides_target, int photo)
{
    char reply[64];
    UiRemoteCommandRequest request;

    assert(ui_remote_commands_handle_text(text, reply, sizeof(reply), commands) == 0);
    assert(strcmp(reply, "OK command accepted\n") == 0);
    ui_remote_commands_take(commands, &request);
    assert(request.recording_target == recording_target);
    assert(request.parking_guides_target == guides_target);
    assert(request.take_photo == photo);
}

int main(void)
{
    UiRemoteCommands commands;
    UiRemoteCommandRequest request;
    char reply[64];

    assert(ui_remote_commands_init(&commands) == 0);
    expect_command(&commands, "开始录像", 1, -1, 0);
    expect_command(&commands, "已收到，开始录像。", 1, -1, 0);
    expect_command(&commands, "停止录像", 0, -1, 0);
    expect_command(&commands, "好的，停止录像。", 0, -1, 0);
    expect_command(&commands, "拍照", -1, -1, 1);
    expect_command(&commands, "已收到，马上拍照。", -1, -1, 1);
    expect_command(&commands, "开启倒车线", -1, 1, 0);
    expect_command(&commands, "关闭倒车线", -1, 0, 0);

    assert(ui_remote_commands_handle_text(" 开始录像", reply, sizeof(reply),
                                          &commands) == 0);
    assert(strcmp(reply, "OK text received\n") == 0);
    ui_remote_commands_take(&commands, &request);
    assert(request.recording_target == -1 && request.parking_guides_target == -1 &&
           request.take_photo == 0);
    assert(ui_remote_commands_handle_text("开始录像 ", reply, sizeof(reply),
                                          &commands) == 0);
    assert(strcmp(reply, "OK text received\n") == 0);
    assert(ui_remote_commands_handle_text("开始录像\n", reply, sizeof(reply),
                                          &commands) == 0);
    assert(strcmp(reply, "OK text received\n") == 0);
    assert(ui_remote_commands_handle_text("开始录制", reply, sizeof(reply),
                                          &commands) == 0);
    assert(strcmp(reply, "OK text received\n") == 0);

    expect_command(&commands, "开始录像", 1, -1, 0);
    expect_command(&commands, "开始录像", 1, -1, 0);
    expect_command(&commands, "停止录像", 0, -1, 0);
    expect_command(&commands, "停止录像", 0, -1, 0);
    ui_remote_commands_destroy(&commands);
    return 0;
}
