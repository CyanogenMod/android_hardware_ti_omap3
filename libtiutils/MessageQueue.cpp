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


#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <unistd.h>
#include <Errors.h>



#define LOG_TAG "MessageQueue"
#include <utils/Log.h>

#include "MessageQueue.h"

namespace android {

MessageQueue::MessageQueue()
{
    int fds[2] = {-1,-1};

    pipe(fds);

    this->fd_read = fds[0];
    this->fd_write = fds[1];

	mHasMsg = false;
}

MessageQueue::~MessageQueue()
{
    close(this->fd_read);
    close(this->fd_write);
}

int MessageQueue::get(Message* msg)
{
    char* p = (char*) msg;
    size_t read_bytes = 0;

    while( read_bytes  < sizeof(*msg) )
    {
        int err = read(this->fd_read, p, sizeof(*msg) - read_bytes);
        
        if( err < 0 ) {
            LOGE("read() error: %s", strerror(errno));
            return -1;
        }
        else
            read_bytes += err;
    }

#ifdef DEBUG_LOG

    LOGD("MQ.get(%d,%p,%p,%p,%p)", msg->command, msg->arg1,msg->arg2,msg->arg3,msg->arg4);    

#endif

	mHasMsg = false;

    return 0;
}

int MessageQueue::getInFd()
{
    return this->fd_read;
}

void MessageQueue::setInFd(int fd)
{
	this->fd_read = fd;
}

int MessageQueue::put(Message* msg)
{
    char* p = (char*) msg;
    size_t bytes = 0;

#ifdef DEBUG_LOG

    LOGD("MQ.put(%d,%p,%p,%p,%p)", msg->command, msg->arg1,msg->arg2,msg->arg3,msg->arg4);

#endif

    while( bytes  < sizeof(msg) )
    {
        int err = write(this->fd_write, p, sizeof(*msg) - bytes);
    	 
        if( err < 0 ) {
            LOGE("write() error: %s", strerror(errno));
            return -1;
        }
        else
            bytes += err;
    }

#ifdef DEBUG_LOG

    LOGD("MessageQueue::put EXIT");

#endif

    return 0;    
}


bool MessageQueue::isEmpty()
{
	if(mHasMsg) return false;
	
    struct pollfd pfd;

    pfd.fd = this->fd_read;
    pfd.events = POLLIN;
    pfd.revents = 0;

    if( -1 == poll(&pfd,1,0) )
    {
        LOGE("poll() error: %s", strerror(errno));
    }

	if(pfd.revents & POLLIN)
		{
		mHasMsg = true;
		}
	else
		{
		mHasMsg = false;
		}
    return mHasMsg;
}

int MessageQueue::waitForMsg(MessageQueue *queue1, MessageQueue *queue2, MessageQueue *queue3, int timeout)
	{	
	int n =1;
	struct pollfd pfd[3];

	if(!queue1)
		{
		return BAD_VALUE;
		}
	
    pfd[0].fd = queue1->getInFd();
    pfd[0].events = POLLIN;
	pfd[0].revents = 0;
	if(queue2)
		{
	    pfd[1].fd = queue2->getInFd();
	    pfd[1].events = POLLIN;
		pfd[1].revents = 0;
		n++;
		}

	if(queue3)
		{
	    pfd[2].fd = queue3->getInFd();
	    pfd[2].events = POLLIN;
		pfd[2].revents = 0;
		n++;
		}
	

    int ret = poll(pfd, n, timeout);
	if(ret==0)
		{
		return ret;
		}
	
	if(ret<NO_ERROR)
		{
		LOGE("Message queue returned error %d", ret);
		return ret;
		}

    if (pfd[0].revents & POLLIN) 
		{
        queue1->setMsg(true);
    	}

	if(queue2)
		{
	    if (pfd[1].revents & POLLIN) 
			{
	         queue2->setMsg(true);
	    	}
		}

	if(queue3)
		{
	    if (pfd[2].revents & POLLIN) 
			{
	         queue3->setMsg(true);
	    	}
		}
	
	return ret;
	}

void MessageQueue::setMsg(bool hasMsg)
	{
	mHasMsg = hasMsg;
	}

};
