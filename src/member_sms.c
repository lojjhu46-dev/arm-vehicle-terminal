#define _POSIX_C_SOURCE 200112L

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "member_sms.h"

#define SMS_HOST "api.ihuyi.com"
#define SMS_PORT "80"
#define SMS_PATH "/sms/Submit.json"
#define SMS_RESPONSE_SIZE 8192
#define SMS_ACCOUNT_VALUE "C40397872"
#define SMS_PASSWORD_VALUE "3efe8a18bdba633efeb364b970625d7b"

// 将表单字段编码为 application/x-www-form-urlencoded 格式。
static int url_encode(const char *source, char *destination, size_t destination_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t source_index;
    size_t destination_index = 0;

    for (source_index = 0; source[source_index] != '\0'; ++source_index)
    {
        unsigned char character = (unsigned char)source[source_index];
        int safe = isalnum(character) || character == '-' || character == '_' ||
                   character == '.' || character == '~';
        size_t required = safe ? 1 : 3;

        if (destination_index + required >= destination_size)
            return -1;
        if (safe)
            destination[destination_index++] = (char)character;
        else
        {
            destination[destination_index++] = '%';
            destination[destination_index++] = hex[character >> 4];
            destination[destination_index++] = hex[character & 0x0f];
        }
    }
    destination[destination_index] = '\0';
    return 0;
}

// 处理短写和 EINTR，直到整段 HTTP 请求发送完成。
static int send_all(int socket_fd, const char *data, size_t data_size)
{
    size_t sent = 0;

    while (sent < data_size)
    {
        ssize_t result = send(socket_fd, data + sent, data_size - sent, 0);

        if (result < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (result == 0)
            return -1;
        sent += (size_t)result;
    }
    return 0;
}

// 从服务商 JSON 响应体中提取业务状态码。
static int response_code(const char *response)
{
    const char *body = strstr(response, "\r\n\r\n");
    const char *code;
    char *end;
    long value;

    if (body == NULL)
        return -1;
    code = strstr(body + 4, "\"code\"");
    if (code == NULL)
        return -1;
    code += strlen("\"code\"");
    while (*code == ':' || isspace((unsigned char)*code))
        ++code;
    value = strtol(code, &end, 10);
    return end == code ? -1 : (int)value;
}

// 同步请求短信服务商，成功时返回 0；由后台线程调用。
int member_sms_send(const char *phone, const char *code)
{
    char content[256];
    char account[512];
    char password[512];
    char encoded_phone[64];
    char encoded_content[1024];
    char body[2300];
    char request[3200];
    char response[SMS_RESPONSE_SIZE];
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    int socket_fd = -1;
    int result = -1;
    size_t response_length = 0;

    if (snprintf(content, sizeof(content),
                 "您的验证码是：%s。请不要把验证码泄露给其他人。", code) < 0 ||
        url_encode(SMS_ACCOUNT_VALUE, account, sizeof(account)) != 0 ||
        url_encode(SMS_PASSWORD_VALUE, password, sizeof(password)) != 0 ||
        url_encode(phone, encoded_phone, sizeof(encoded_phone)) != 0 ||
        url_encode(content, encoded_content, sizeof(encoded_content)) != 0 ||
        snprintf(body, sizeof(body), "account=%s&password=%s&mobile=%s&content=%s",
                 account, password, encoded_phone, encoded_content) >= (int)sizeof(body) ||
        snprintf(request, sizeof(request),
                 "POST %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n"
                 "Content-Type: application/x-www-form-urlencoded\r\n"
                 "Content-Length: %zu\r\n\r\n%s",
                 SMS_PATH, SMS_HOST, strlen(body), body) >= (int)sizeof(request))
    {
        fprintf(stderr, "[SMS] request construction failed\n");
        return -1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(SMS_HOST, SMS_PORT, &hints, &addresses) != 0)
    {
        fprintf(stderr, "[SMS] DNS lookup failed\n");
        return -1;
    }
    for (address = addresses; address != NULL; address = address->ai_next)
    {
        socket_fd = socket(address->ai_family, address->ai_socktype,
                           address->ai_protocol);
        if (socket_fd >= 0 &&
            connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0)
            break;
        if (socket_fd >= 0)
            close(socket_fd);
        socket_fd = -1;
    }
    if (socket_fd < 0 || send_all(socket_fd, request, strlen(request)) != 0)
    {
        fprintf(stderr, "[SMS] provider connection failed\n");
        goto done;
    }
    while (response_length < sizeof(response) - 1)
    {
        ssize_t received = recv(socket_fd, response + response_length,
                                sizeof(response) - 1 - response_length, 0);

        if (received < 0)
        {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "[SMS] provider response failed\n");
            goto done;
        }
        if (received == 0)
            break;
        response_length += (size_t)received;
    }
    response[response_length] = '\0';
    if (sscanf(response, "HTTP/%*s %d", &result) != 1 || result != 200 ||
        response_code(response) != 2)
    {
        fprintf(stderr, "[SMS] provider rejected request\n");
        result = -1;
        goto done;
    }
    result = 0;

done:
    if (socket_fd >= 0)
        close(socket_fd);
    freeaddrinfo(addresses);
    return result;
}

// 执行耗时网络请求，并把结果回写到登录状态机。
static void *member_sms_worker_main(void *argument)
{
    MemberSmsWorker *worker = argument;
    MemberSmsRequest request;
    MemberLogin *login;
    int success;

    pthread_mutex_lock(&worker->mutex);
    request = worker->request;
    login = worker->login;
    pthread_mutex_unlock(&worker->mutex);
    success = member_sms_send(request.phone, request.code) == 0;
    member_login_complete_sms(login, &request, success);
    pthread_mutex_lock(&worker->mutex);
    worker->running = 0;
    pthread_mutex_unlock(&worker->mutex);
    return NULL;
}

// 初始化单任务短信工作器。
int member_sms_worker_init(MemberSmsWorker *worker)
{
    memset(worker, 0, sizeof(*worker));
    return pthread_mutex_init(&worker->mutex, NULL) == 0 ? 0 : -1;
}

// 启动一次后台短信发送；上一次线程结束后先回收其线程句柄。
int member_sms_worker_start(MemberSmsWorker *worker, MemberLogin *login,
                            const MemberSmsRequest *request)
{
    pthread_mutex_lock(&worker->mutex);
    if (worker->running)
    {
        pthread_mutex_unlock(&worker->mutex);
        return -1;
    }
    if (worker->joinable)
    {
        pthread_t finished_thread = worker->thread;

        pthread_mutex_unlock(&worker->mutex);
        pthread_join(finished_thread, NULL);
        pthread_mutex_lock(&worker->mutex);
        worker->joinable = 0;
    }
    worker->login = login;
    worker->request = *request;
    worker->running = 1;
    if (pthread_create(&worker->thread, NULL, member_sms_worker_main, worker) != 0)
    {
        worker->running = 0;
        pthread_mutex_unlock(&worker->mutex);
        return -1;
    }
    worker->joinable = 1;
    pthread_mutex_unlock(&worker->mutex);
    return 0;
}

// 等待在途短信任务结束并释放工作器互斥锁。
void member_sms_worker_destroy(MemberSmsWorker *worker)
{
    pthread_t thread;
    int joinable;

    pthread_mutex_lock(&worker->mutex);
    thread = worker->thread;
    joinable = worker->joinable;
    pthread_mutex_unlock(&worker->mutex);
    if (joinable)
        pthread_join(thread, NULL);
    pthread_mutex_destroy(&worker->mutex);
    memset(worker, 0, sizeof(*worker));
}
