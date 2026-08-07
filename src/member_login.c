#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "member_login.h"

#define PHONE_INPUT_X1 383
#define PHONE_INPUT_Y1 105
#define PHONE_INPUT_X2 735
#define PHONE_INPUT_Y2 146
#define CODE_INPUT_X1 383
#define CODE_INPUT_Y1 160
#define CODE_INPUT_X2 650
#define CODE_INPUT_Y2 200
#define SMS_BUTTON_X1 650
#define SMS_BUTTON_Y1 160
#define SMS_BUTTON_X2 735
#define SMS_BUTTON_Y2 200
#define LOGIN_BUTTON_X1 383
#define LOGIN_BUTTON_Y1 213
#define LOGIN_BUTTON_X2 735
#define LOGIN_BUTTON_Y2 255

typedef struct {
    int x1;
    int y1;
    int x2;
    int y2;
    char value;
} KeyArea;

static const KeyArea keypad[] = {
    {363, 301, 491, 339, '1'}, {493, 301, 623, 339, '2'}, {625, 301, 754, 339, '3'},
    {363, 340, 491, 378, '4'}, {493, 340, 623, 378, '5'}, {625, 340, 754, 378, '6'},
    {363, 379, 491, 417, '7'}, {493, 379, 623, 417, '8'}, {625, 379, 754, 417, '9'},
    {363, 418, 491, 458, 'B'}, {493, 418, 623, 458, '0'}, {625, 418, 754, 458, 'E'},
};

// 判断触摸点是否落在左闭右开的矩形区域内。
static int in_area(int x, int y, int x1, int y1, int x2, int y2)
{
    return x >= x1 && x < x2 && y >= y1 && y < y2;
}

// 更新登录页底部显示的状态提示。
static void set_status(MemberLogin *login, const char *status)
{
    snprintf(login->status, sizeof(login->status), "%s", status);
}

// 作废当前验证码请求，避免手机号变更后旧短信结果重新生效。
static void invalidate_sms(MemberLogin *login)
{
    login->sms_pending = 0;
    login->sms_ready = 0;
    login->code[0] = '\0';
    login->expected_code[0] = '\0';
    ++login->request_id;
}

// 生成零填充的四位随机验证码。
static void make_code(char *code)
{
    static int seeded;

    if (!seeded)
    {
        srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
        seeded = 1;
    }
    snprintf(code, MEMBER_LOGIN_CODE_LENGTH + 1, "%04d", rand() % 10000);
}

// 向当前选中的输入框追加一个数字，并执行长度限制。
static void append_digit(MemberLogin *login, char digit)
{
    char *target;
    size_t limit;
    size_t length;

    if (login->active_field == MEMBER_LOGIN_FIELD_PHONE)
    {
        target = login->phone;
        limit = MEMBER_LOGIN_PHONE_LENGTH;
    }
    else if (login->active_field == MEMBER_LOGIN_FIELD_CODE)
    {
        target = login->code;
        limit = MEMBER_LOGIN_CODE_LENGTH;
    }
    else
    {
        set_status(login, "请先选择输入框");
        return;
    }

    length = strlen(target);
    if (length >= limit)
    {
        set_status(login, login->active_field == MEMBER_LOGIN_FIELD_PHONE ?
                   "手机号最多11位" : "验证码最多4位");
        return;
    }
    target[length] = digit;
    target[length + 1] = '\0';
    if (login->active_field == MEMBER_LOGIN_FIELD_PHONE)
        invalidate_sms(login);
    login->status[0] = '\0';
}

// 删除当前输入框的最后一位数字。
static void delete_digit(MemberLogin *login)
{
    char *target = NULL;
    size_t length;

    if (login->active_field == MEMBER_LOGIN_FIELD_PHONE)
        target = login->phone;
    else if (login->active_field == MEMBER_LOGIN_FIELD_CODE)
        target = login->code;
    if (target == NULL)
        return;

    length = strlen(target);
    if (length > 0)
        target[length - 1] = '\0';
    if (login->active_field == MEMBER_LOGIN_FIELD_PHONE)
        invalidate_sms(login);
    login->status[0] = '\0';
}

// 校验手机号并创建一次可由后台线程发送的短信请求。
static MemberLoginAction request_sms(MemberLogin *login,
                                     MemberSmsRequest *request)
{
    if (strlen(login->phone) != MEMBER_LOGIN_PHONE_LENGTH)
    {
        set_status(login, "请输入11位手机号");
        return MEMBER_LOGIN_ACTION_NONE;
    }
    if (login->sms_pending)
    {
        set_status(login, "验证码发送中...");
        return MEMBER_LOGIN_ACTION_NONE;
    }

    ++login->request_id;
    login->sms_pending = 1;
    login->sms_ready = 0;
    login->code[0] = '\0';
    login->expected_code[0] = '\0';
    login->active_field = MEMBER_LOGIN_FIELD_CODE;
    set_status(login, "验证码发送中...");
    memset(request, 0, sizeof(*request));
    snprintf(request->phone, sizeof(request->phone), "%s", login->phone);
    make_code(request->code);
    request->request_id = login->request_id;
    return MEMBER_LOGIN_ACTION_SEND_SMS;
}

// 校验验证码状态和输入内容，成功时通知 UI 进入会员中心。
static MemberLoginAction submit(MemberLogin *login)
{
    if (login->sms_pending)
    {
        set_status(login, "验证码发送中...");
        return MEMBER_LOGIN_ACTION_NONE;
    }
    if (!login->sms_ready)
    {
        set_status(login, "请先获取验证码");
        return MEMBER_LOGIN_ACTION_NONE;
    }
    if (strlen(login->code) != MEMBER_LOGIN_CODE_LENGTH)
    {
        set_status(login, "请输入4位验证码");
        return MEMBER_LOGIN_ACTION_NONE;
    }
    if (strcmp(login->code, login->expected_code) != 0)
    {
        set_status(login, "验证码错误");
        return MEMBER_LOGIN_ACTION_NONE;
    }
    set_status(login, "登录成功");
    return MEMBER_LOGIN_ACTION_SUCCESS;
}

// 初始化互斥锁和空白登录状态。
int member_login_init(MemberLogin *login)
{
    memset(login, 0, sizeof(*login));
    if (pthread_mutex_init(&login->mutex, NULL) != 0)
        return -1;
    login->version = 1;
    return 0;
}

// 释放登录状态持有的同步资源。
void member_login_destroy(MemberLogin *login)
{
    pthread_mutex_destroy(&login->mutex);
    memset(login, 0, sizeof(*login));
}

// 清空敏感输入并递增请求号，使在途短信结果失效。
void member_login_reset(MemberLogin *login)
{
    pthread_mutex_lock(&login->mutex);
    login->phone[0] = '\0';
    login->code[0] = '\0';
    login->expected_code[0] = '\0';
    login->status[0] = '\0';
    login->active_field = MEMBER_LOGIN_FIELD_NONE;
    login->sms_pending = 0;
    login->sms_ready = 0;
    ++login->request_id;
    ++login->version;
    pthread_mutex_unlock(&login->mutex);
}

// 按登录图片中的控件坐标处理一次完整点击。
// 数字键、删除键和确认键统一在此处转成状态机动作。
MemberLoginAction member_login_handle_tap(MemberLogin *login, int x, int y,
                                          MemberSmsRequest *request)
{
    MemberLoginAction action = MEMBER_LOGIN_ACTION_NONE;

    if (request != NULL)
        memset(request, 0, sizeof(*request));
    pthread_mutex_lock(&login->mutex);
    if (in_area(x, y, PHONE_INPUT_X1, PHONE_INPUT_Y1, PHONE_INPUT_X2, PHONE_INPUT_Y2))
    {
        login->active_field = MEMBER_LOGIN_FIELD_PHONE;
        login->status[0] = '\0';
    }
    else if (in_area(x, y, CODE_INPUT_X1, CODE_INPUT_Y1, CODE_INPUT_X2, CODE_INPUT_Y2))
    {
        login->active_field = MEMBER_LOGIN_FIELD_CODE;
        login->status[0] = '\0';
    }
    else if (in_area(x, y, SMS_BUTTON_X1, SMS_BUTTON_Y1, SMS_BUTTON_X2, SMS_BUTTON_Y2))
    {
        if (request != NULL)
            action = request_sms(login, request);
        else
            set_status(login, "验证码请求失败");
    }
    else if (in_area(x, y, LOGIN_BUTTON_X1, LOGIN_BUTTON_Y1,
                     LOGIN_BUTTON_X2, LOGIN_BUTTON_Y2))
    {
        action = submit(login);
    }
    else
    {
        for (size_t i = 0; i < sizeof(keypad) / sizeof(keypad[0]); ++i)
        {
            if (!in_area(x, y, keypad[i].x1, keypad[i].y1,
                         keypad[i].x2, keypad[i].y2))
                continue;
            if (keypad[i].value >= '0' && keypad[i].value <= '9')
                append_digit(login, keypad[i].value);
            else if (keypad[i].value == 'B')
                delete_digit(login);
            else
                action = submit(login);
            break;
        }
    }
    ++login->version;
    pthread_mutex_unlock(&login->mutex);
    return action;
}

// 接收后台短信发送结果；仅接受手机号和请求号仍匹配的结果。
void member_login_complete_sms(MemberLogin *login, const MemberSmsRequest *request,
                               int success)
{
    pthread_mutex_lock(&login->mutex);
    if (request->request_id == login->request_id &&
        strcmp(request->phone, login->phone) == 0)
    {
        login->sms_pending = 0;
        if (success)
        {
            login->sms_ready = 1;
            snprintf(login->expected_code, sizeof(login->expected_code), "%s",
                     request->code);
            login->active_field = MEMBER_LOGIN_FIELD_CODE;
            set_status(login, "验证码已发送");
        }
        else
        {
            login->sms_ready = 0;
            login->expected_code[0] = '\0';
            set_status(login, "短信发送失败");
        }
        ++login->version;
    }
    pthread_mutex_unlock(&login->mutex);
}

// 将后台线程创建失败转换为普通的短信发送失败状态。
void member_login_set_sms_start_failed(MemberLogin *login,
                                       const MemberSmsRequest *request)
{
    member_login_complete_sms(login, request, 0);
}

// 在锁保护下复制供渲染线程读取的登录状态快照。
void member_login_snapshot(MemberLogin *login, MemberLoginSnapshot *snapshot)
{
    pthread_mutex_lock(&login->mutex);
    snprintf(snapshot->phone, sizeof(snapshot->phone), "%s", login->phone);
    snprintf(snapshot->code, sizeof(snapshot->code), "%s", login->code);
    snprintf(snapshot->status, sizeof(snapshot->status), "%s", login->status);
    snapshot->active_field = login->active_field;
    snapshot->sms_pending = login->sms_pending;
    snapshot->sms_ready = login->sms_ready;
    snapshot->version = login->version;
    pthread_mutex_unlock(&login->mutex);
}
