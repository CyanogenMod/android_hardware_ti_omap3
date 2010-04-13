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


#include "Semaphore.h"
#include <utils/Log.h>


namespace android {


Semaphore::Semaphore()
{
	mSemaphore = NULL;
}

Semaphore::~Semaphore()
{
	sem_destroy(mSemaphore);
	free(mSemaphore);
}


status_t Semaphore::Create(int count)
{
	if(count<0)
		{		
		return BAD_VALUE;
		}
    mSemaphore = (sem_t*)malloc(sizeof(sem_t)) ;
    return sem_init(mSemaphore, 0x00, count);

}


status_t Semaphore::Wait()
{

	if(!mSemaphore)
		{
		return BAD_VALUE;
		}

	return sem_wait(mSemaphore);
	
	
}

status_t Semaphore::Signal()
{
	if(!mSemaphore)
		{
		return BAD_VALUE;
		}
	return sem_post(mSemaphore);

}

status_t Semaphore::Count()
{
	int val;
	if(!mSemaphore)
		{
		return BAD_VALUE;
		}	
	sem_getvalue(mSemaphore, &val);
	return val;
}

status_t Semaphore::WaitTimeout(int timeoutMicroSecs)
{
	struct timespec timeSpec;

	if(!mSemaphore)
		{
		return BAD_VALUE;
		}
	
	timeSpec.tv_sec = (timeoutMicroSecs/1000000);
	timeSpec.tv_nsec = (timeoutMicroSecs - timeSpec.tv_sec*1000000)*1000;	

	return sem_timedwait(mSemaphore, &timeSpec);

}

};


