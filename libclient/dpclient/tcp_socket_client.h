/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef DPCLIENT_TCP_SOCKET_CLIENT_H
#define DPCLIENT_TCP_SOCKET_CLIENT_H

#ifdef __EMSCRIPTEN__
#    error "TCP Sockets not available in Emscripten, should be using WebSockets"
#endif

#include "client.h"
#include <dpcommon/common.h>
#include <dpcommon/queue.h>
#include <SDL_atomic.h>
#include <uriparser/Uri.h>

typedef struct DP_Client DP_Client;
typedef struct DP_Message DP_Message;
typedef struct DP_Mutex DP_Mutex;
typedef struct DP_Semaphore DP_Semaphore;
typedef struct DP_Thread DP_Thread;


#define DP_TCP_SOCKET_CLIENT_SCHEME       "drawpile"
#define DP_TCP_SOCKET_CLIENT_DEFAULT_PORT "27750"

#define DP_TCP_SOCKET_CLIENT_NULL                   \
    (DP_TcpSocketClient)                            \
    {                                               \
        {0}, DP_QUEUE_NULL, NULL, NULL, NULL, NULL, \
        {                                           \
            -1                                      \
        }                                           \
    }

typedef struct DP_TcpSocketClient {
    UriUriA uri;
    DP_Queue queue;
    DP_Mutex *mutex_queue;
    DP_Semaphore *sem_queue;
    DP_Thread *thread_send;
    DP_Thread *thread_recv;
    SDL_atomic_t socket;
} DP_TcpSocketClient;


DP_ClientUrlValidationResult DP_tcp_socket_client_url_valid(const char *url);

bool DP_tcp_socket_client_init(DP_Client *client);

void DP_tcp_socket_client_dispose(DP_Client *client);

void DP_tcp_socket_client_stop(DP_Client *client);

void DP_tcp_socket_client_send(DP_Client *client, DP_Message *msg);


#endif
