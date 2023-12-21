#ifndef __CVI_PTHREAD_H__
#define __CVI_PTHREAD_H__
// #define USE_POSIX
#ifdef USE_POSIX
#include <semaphore.h>
#include <pthread.h>
#include <base_ctx.h>
#else
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#endif

#if 1//def USE_POSIX
#define MUTEX_T pthread_mutex_t
#define MUTEX_LOCK(mutex) pthread_mutex_lock(&mutex)
#define MUTEX_UNLOCK(mutex) pthread_mutex_unlock(&mutex)
#define MUTEX_DESTROY(mutex) pthread_mutex_destroy(&mutex)

// #define SEM_T ptread_sem_s
#define SEM_T sem_t
#define SEM_INIT(mutex) sem_init(&mutex, 0, 0)
#define SEM_WAIT(mutex) sem_wait(&mutex)
#define SEM_POST(mutex) sem_post(&mutex)
#define SEM_TIMEDWAIT(mutex, ts) sem_timedwait(&mutex, &ts)
#define SEM_DESTROY(mutex) sem_destroy(&mutex)

#define PTHREAD_T pthread_t

#else
#define MUTEX_T SemaphoreHandle_t
#define MUTEX_INIT(mutex) mutex = xSemaphoreCreateMutex();
#define MUTEX_LOCK(mutex) xSemaphoreTake(mutex, -1)
#define MUTEX_UNLOCK(mutex) xSemaphoreGive(mutex)
#define MUTEX_DESTROY(mutex) vSemaphoreDelete(mutex)

#define SEM_T SemaphoreHandle_t
#define SEM_INIT(mutex) mutex = xSemaphoreCreateMutex(); xSemaphoreTake(mutex, -1)
#define SEM_WAIT(mutex) xSemaphoreTake(mutex, -1)
#define SEM_POST(mutex) xSemaphoreGive(mutex)
#define SEM_DESTROY(mutex) vSemaphoreDelete(mutex)

#define PTHREAD_T TaskHandle_t
#endif

#endif //__CVI_PTHREAD_H__
