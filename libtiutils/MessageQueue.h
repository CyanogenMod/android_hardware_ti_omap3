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

namespace android {


struct Message 
{
    unsigned int command;
    void*        arg1;
    void*        arg2;       
    void*        arg3;    
    void*        arg4;    
};

class MessageQueue 
{
public:
    MessageQueue();
    ~MessageQueue();
    int get(Message*);
    int getInFd();
	void setInFd(int fd);
    int put(Message*);
    bool isEmpty(); 
	void setMsg(bool hasMsg=false);
	static int waitForMsg(MessageQueue *queue1, MessageQueue *queue2=0, MessageQueue *queue3=0, int timeout = 0);
private:
    int fd_read;
    int fd_write;
	bool mHasMsg;
};

};

#endif
