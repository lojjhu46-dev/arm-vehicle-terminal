#ifndef MEMBER_LOGIN_H
#define MEMBER_LOGIN_H

#include <pthread.h>
#include <stddef.h>

#define MEMBER_LOGIN_PHONE_LENGTH 11
#define MEMBER_LOGIN_CODE_LENGTH 4
#define MEMBER_LOGIN_STATUS_SIZE 96

typedef enum {
    MEMBER_LOGIN_FIELD_NONE = 0,
    MEMBER_LOGIN_FIELD_PHONE,
    MEMBER_LOGIN_FIELD_CODE
} MemberLoginField;

typedef enum {
    MEMBER_LOGIN_ACTION_NONE = 0,
    MEMBER_LOGIN_ACTION_SEND_SMS,
    MEMBER_LOGIN_ACTION_SUCCESS
} MemberLoginAction;

typedef struct {
    char phone[MEMBER_LOGIN_PHONE_LENGTH + 1];
    char code[MEMBER_LOGIN_CODE_LENGTH + 1];
    unsigned long request_id;
} MemberSmsRequest;

typedef struct {
    char phone[MEMBER_LOGIN_PHONE_LENGTH + 1];
    char code[MEMBER_LOGIN_CODE_LENGTH + 1];
    char status[MEMBER_LOGIN_STATUS_SIZE];
    MemberLoginField active_field;
    int sms_pending;
    int sms_ready;
    unsigned long version;
} MemberLoginSnapshot;

typedef struct {
    pthread_mutex_t mutex;
    char phone[MEMBER_LOGIN_PHONE_LENGTH + 1];
    char code[MEMBER_LOGIN_CODE_LENGTH + 1];
    char expected_code[MEMBER_LOGIN_CODE_LENGTH + 1];
    char status[MEMBER_LOGIN_STATUS_SIZE];
    MemberLoginField active_field;
    int sms_pending;
    int sms_ready;
    unsigned long request_id;
    unsigned long version;
} MemberLogin;

int member_login_init(MemberLogin *login);
void member_login_destroy(MemberLogin *login);
void member_login_reset(MemberLogin *login);
MemberLoginAction member_login_handle_tap(MemberLogin *login, int x, int y,
                                          MemberSmsRequest *request);
void member_login_complete_sms(MemberLogin *login, const MemberSmsRequest *request,
                               int success);
void member_login_set_sms_start_failed(MemberLogin *login,
                                       const MemberSmsRequest *request);
void member_login_snapshot(MemberLogin *login, MemberLoginSnapshot *snapshot);

#endif
