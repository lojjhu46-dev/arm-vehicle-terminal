#ifndef MEMBER_SMS_H
#define MEMBER_SMS_H

#include <pthread.h>

#include "member_login.h"

typedef struct {
    pthread_mutex_t mutex;
    pthread_t thread;
    MemberLogin *login;
    MemberSmsRequest request;
    int running;
    int joinable;
} MemberSmsWorker;

int member_sms_send(const char *phone, const char *code);
int member_sms_worker_init(MemberSmsWorker *worker);
int member_sms_worker_start(MemberSmsWorker *worker, MemberLogin *login,
                            const MemberSmsRequest *request);
void member_sms_worker_destroy(MemberSmsWorker *worker);

#endif
