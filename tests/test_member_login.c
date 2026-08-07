#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "member_login.h"

static void press_digit(MemberLogin *login, char digit)
{
    static const int key_x[] = {557, 427, 557, 689, 427, 557, 689, 427, 557, 689};
    static const int key_y[] = {438, 320, 320, 320, 359, 359, 359, 398, 398, 398};

    assert(member_login_handle_tap(login, key_x[digit - '0'],
                                   key_y[digit - '0'], NULL) ==
           MEMBER_LOGIN_ACTION_NONE);
}

static void enter_phone(MemberLogin *login, const char *phone)
{
    assert(member_login_handle_tap(login, 420, 125, NULL) ==
           MEMBER_LOGIN_ACTION_NONE);
    for (size_t i = 0; phone[i] != '\0'; ++i)
        press_digit(login, phone[i]);
}

int main(void)
{
    MemberLogin login;
    MemberLoginSnapshot snapshot;
    MemberSmsRequest request;
    char wrong_code[MEMBER_LOGIN_CODE_LENGTH + 1];

    assert(member_login_init(&login) == 0);
    assert(member_login_handle_tap(&login, 690, 180, &request) ==
           MEMBER_LOGIN_ACTION_NONE);
    member_login_snapshot(&login, &snapshot);
    assert(strcmp(snapshot.status, "请输入11位手机号") == 0);

    enter_phone(&login, "13800138000");
    member_login_snapshot(&login, &snapshot);
    assert(strcmp(snapshot.phone, "13800138000") == 0);
    assert(member_login_handle_tap(&login, 690, 180, &request) ==
           MEMBER_LOGIN_ACTION_SEND_SMS);
    assert(strlen(request.code) == MEMBER_LOGIN_CODE_LENGTH);
    member_login_complete_sms(&login, &request, 1);
    member_login_snapshot(&login, &snapshot);
    assert(snapshot.sms_ready == 1 && snapshot.sms_pending == 0);

    snprintf(wrong_code, sizeof(wrong_code), "%s", request.code);
    wrong_code[0] = wrong_code[0] == '9' ? '8' : '9';
    for (size_t i = 0; wrong_code[i] != '\0'; ++i)
        press_digit(&login, wrong_code[i]);
    assert(member_login_handle_tap(&login, 550, 235, NULL) ==
           MEMBER_LOGIN_ACTION_NONE);
    member_login_snapshot(&login, &snapshot);
    assert(strcmp(snapshot.status, "验证码错误") == 0);

    assert(member_login_handle_tap(&login, 420, 180, NULL) ==
           MEMBER_LOGIN_ACTION_NONE);
    for (size_t i = 0; i < MEMBER_LOGIN_CODE_LENGTH; ++i)
        assert(member_login_handle_tap(&login, 427, 438, NULL) ==
               MEMBER_LOGIN_ACTION_NONE);
    for (size_t i = 0; request.code[i] != '\0'; ++i)
        press_digit(&login, request.code[i]);
    assert(member_login_handle_tap(&login, 550, 235, NULL) ==
           MEMBER_LOGIN_ACTION_SUCCESS);

    member_login_reset(&login);
    enter_phone(&login, "13800138000");
    assert(member_login_handle_tap(&login, 690, 180, &request) ==
           MEMBER_LOGIN_ACTION_SEND_SMS);
    assert(member_login_handle_tap(&login, 420, 125, NULL) ==
           MEMBER_LOGIN_ACTION_NONE);
    assert(member_login_handle_tap(&login, 427, 438, NULL) ==
           MEMBER_LOGIN_ACTION_NONE);
    press_digit(&login, '1');
    member_login_complete_sms(&login, &request, 1);
    member_login_snapshot(&login, &snapshot);
    assert(snapshot.sms_ready == 0 && snapshot.sms_pending == 0);

    member_login_destroy(&login);
    return 0;
}
