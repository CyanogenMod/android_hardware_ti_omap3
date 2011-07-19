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


#include <Errors.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace android {

class Semaphore
{
public:

    Semaphore();
    ~Semaphore();

    //Release semaphore
    status_t Release();

    ///Create the semaphore with initial count value
    status_t Create(int count=0);

    ///Wait operation
    status_t Wait();

    ///Signal operation
    status_t Signal();

    ///Current semaphore count
    int Count();

    ///Wait operation with a timeout
    status_t WaitTimeout(int timeoutMicroSecs);

private:
    sem_t *mSemaphore;

};

};
