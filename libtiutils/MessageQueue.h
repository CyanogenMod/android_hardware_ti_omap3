/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef __MESSAGEQUEUE_H__
#define __MESSAGEQUEUE_H__

#include "DebugUtils.h"
#include <stdint.h>

///Uncomment this macro to debug the message queue implementation
//#define DEBUG_LOG

///Camera HAL Logging Functions
#ifndef DEBUG_LOG

#define MSGQ_LOGDA(str)
#define MSGQ_LOGDB(str, ...)

#undef LOG_FUNCTION_NAME
#undef LOG_FUNCTION_NAME_EXIT
#define LOG_FUNCTION_NAME
#define LOG_FUNCTION_NAME_EXIT

#else

#define MSGQ_LOGDA DBGUTILS_LOGDA
#define MSGQ_LOGDB DBGUTILS_LOGDB

#endif

#define MSGQ_LOGEA DBGUTILS_LOGEA
#define MSGQ_LOGEB DBGUTILS_LOGEB


namespace android {

///Message type
struct Message
{
    unsigned int command;
    void*        arg1;
    void*        arg2;
    void*        arg3;
    void*        arg4;
    int64_t     id;
};

///Message queue implementation
class MessageQueue
{
public:

    MessageQueue();
    ~MessageQueue();

    ///Get a message from the queue
    status_t get(Message*);

    ///Get the input file descriptor of the message queue
    int getInFd();

    ///Set the input file descriptor for the message queue
    void setInFd(int fd);

    ///Queue a message
    status_t put(Message*);

    ///Returns if the message queue is empty or not
    bool isEmpty();

    ///Force whether the message queue has message or not
    void setMsg(bool hasMsg=false);

    ///Wait for message in maximum three different queues with a timeout
    static int waitForMsg(MessageQueue *queue1, MessageQueue *queue2=0, MessageQueue *queue3=0, int timeout = 0);


private:
    int fd_read;
    int fd_write;
    bool mHasMsg;
};

};

#endif
