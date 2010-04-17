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

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <utils/Log.h>

#include <ui/Surface.h>
#include <ui/ISurface.h>
#include <ui/Overlay.h>
#include <ui/SurfaceComposerClient.h>
#include "v4l2_utils.h"

#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>


#define LCD_WIDTH 800
#define LCD_HEIGHT 480


using namespace android;

namespace android {
class Test {
public:
    void getISurface(const sp<SurfaceControl>& s) {
	mSurface =  s->getISurface();
	return;
    }

	sp<ISurface> mSurface;

};

/** overlay test class definition**/
class OverlayTest : public Test
{
public:
	sp<ProcessState> mProc;
	void* mBuffers1[6];
	void* mBuffers2[6];
	int   mNumBuffers;

	//surface flinger client data structures
	sp<SurfaceComposerClient> mClient1;
	sp<SurfaceComposerClient> mClient2;

	sp<SurfaceControl> mSurfaceCtrlLcd1;
	sp<SurfaceControl> mSurfaceCtrlLcd2;
	sp<SurfaceControl> mSurfaceCtrlTv;
	sp<SurfaceControl> mSurfaceCtrlDlp;

	sp<Overlay> mOverlay1;
	sp<Overlay> mOverlay2;

	sp<ISurface> mSurfaceLcd1;
	sp<ISurface> mSurfaceLcd2;
	sp<ISurface> mSurfaceTv;
	sp<ISurface> mSurfaceDlp;

	float alpha1;
	float alpha2;

	int deg1;
	int deg2;

	int mirror1;
	int mirror2;

	int newwidth1;
	int newheight1;
	int newwidth2;
	int newheight2;

	int zOrder1;
	int zOrder2;

	bool zoom1;
	bool zoom2;
	bool autoscale1;
	bool autoscale2;

public:
	int init();
	void resetParams()
	{
	alpha1 = 0;
	alpha2 = 0;

	deg1 = -1;
	deg2 = -1;

	mirror1 = -1;
	mirror2 = -1;

	newwidth1 = -1;
	newheight1 = -1;
	newwidth2 = -1;
	newheight2 = -1;

	zOrder1 = -1;
	zOrder2 = -1;

	zoom1 = false;
	zoom2 = false;

	autoscale1 = false;
	autoscale2 = false;
	}
	void testOverlay(char* img1, uint32_t w1, uint32_t h1, uint8_t panel1, char* img2=NULL, uint32_t w2=0, uint32_t h2=0, uint8_t panel2=0);

};


//Single client + one Overlay
void OverlayTest::testOverlay(char* img1, uint32_t w1, uint32_t h1, uint8_t panel1,
							char* img2, uint32_t w2, uint32_t h2, uint8_t panel2)
{
	LOGE("testOverlay++");
	// now request an overlay
	sp<ISurface> firstSurface;
	sp<SurfaceComposerClient> firstClient;
	sp<SurfaceControl> firstSurfaceCtrl;
	sp<OverlayRef> ref1;

	sp<ISurface> secondSurface;
	sp<SurfaceComposerClient> secondClient;
	sp<SurfaceControl> secondSurfaceCtrl;
	sp<OverlayRef> ref2;

	switch(panel1)
	{
	case 0:
		firstSurface = mSurfaceLcd1;
		firstClient = mClient1;
		firstSurfaceCtrl = mSurfaceCtrlLcd1;
		break;
	case 1:
		firstSurface = mSurfaceLcd2;
		firstClient = mClient2;
		firstSurfaceCtrl = mSurfaceCtrlLcd2;
		break;
	case 2:
		firstSurface = mSurfaceTv;
		firstClient = mClient1;
		firstSurfaceCtrl = mSurfaceCtrlTv;
		break;
	case 3:
		firstSurface = mSurfaceDlp;
		firstClient = mClient2;
		firstSurfaceCtrl = mSurfaceCtrlDlp;
		break;
	default:
		firstSurface = mSurfaceLcd1;
		firstClient = mClient1;
		firstSurfaceCtrl = mSurfaceCtrlLcd1;
	};

	ref1 = firstSurface->createOverlay(w1, h1, OVERLAY_FORMAT_RGB_565);
	if(ref1 == NULL)
	{
		LOGE("NoMemory for overlayRef[%d]", __LINE__);
		return;
	}
	mOverlay1 = new Overlay(ref1);
	if(mOverlay1 == NULL)
	{
		LOGE("NoMemory for overlay[%d]", __LINE__);
		return;
	}

	if (img2 != NULL)
	{
		switch(panel2)
		{
		case 0:
			secondSurface = mSurfaceLcd1;
			secondClient = mClient1;
			secondSurfaceCtrl = mSurfaceCtrlLcd1;
			break;
		case 1:
			secondSurface = mSurfaceLcd2;
			secondClient = mClient2;
			secondSurfaceCtrl = mSurfaceCtrlLcd2;
			break;
		case 2:
			secondSurface = mSurfaceTv;
			secondClient = mClient1;
			secondSurfaceCtrl = mSurfaceCtrlTv;
			break;
		case 3:
			secondSurface = mSurfaceDlp;
			secondClient = mClient2;
			secondSurfaceCtrl = mSurfaceCtrlDlp;
			break;
		default:
			secondSurface = mSurfaceLcd1;
			secondClient = mClient1;
			secondSurfaceCtrl = mSurfaceCtrlLcd1;
		};

	ref2 = secondSurface->createOverlay(w2, h2, OVERLAY_FORMAT_RGB_565);
	if(ref2 == NULL)
	{
		LOGE("NoMemory for overlayRef[%d]", __LINE__);
		return;
	}
	mOverlay2 = new Overlay(ref2);
	if(mOverlay2 == NULL)
	{
		LOGE("NoMemory for overlay[%d]", __LINE__);
		return;
	}
	}

	firstClient->openTransaction();
	firstSurfaceCtrl->setPosition(112, 0);
	if ((newwidth1 != -1) && (newheight1 != -1))
	{
		firstSurfaceCtrl->setSize(newwidth1, newheight1);
	}
	else
	{
		firstSurfaceCtrl->setSize(w1, h1);
	}

	if (zOrder1 != -1)
	{
		firstSurfaceCtrl->setLayer(zOrder1);
	}
	if (deg1 != -1)
	{
	switch(deg1)
		{
		case 0:
			firstClient->setOrientation(panel1,ISurfaceComposer::eOrientationDefault,0);
			break;
		case 90:
			firstClient->setOrientation(panel1,ISurfaceComposer::eOrientation90,0);
			break;
		case 180:
			firstClient->setOrientation(panel1,ISurfaceComposer::eOrientation180,0);
			break;
		case 270:
			firstClient->setOrientation(panel1,ISurfaceComposer::eOrientation270,0);
			break;
		default:
			firstClient->setOrientation(panel1,ISurfaceComposer::eOrientationDefault,0);
		};
	}
    firstSurfaceCtrl->setAlpha(alpha1);
    firstClient->closeTransaction();

	if (img2 != NULL)
	{
		secondClient->openTransaction();
		secondSurfaceCtrl->setPosition(320, 0);
		if ((newwidth2 != -1) && (newheight2 != -1))
		{
			secondSurfaceCtrl->setSize(newwidth2, newheight2);
		}
		else
		{
			secondSurfaceCtrl->setSize(w2, h2);
		}
		if (zOrder2 != -1)
		{
			secondSurfaceCtrl->setLayer(zOrder2);
		}
		if (deg2 != -1)
		{
		switch(deg2)
			{
			case 0:
				secondClient->setOrientation(panel2,ISurfaceComposer::eOrientationDefault,0);
				break;
			case 90:
				secondClient->setOrientation(panel2,ISurfaceComposer::eOrientation90,0);
				break;
			case 180:
				secondClient->setOrientation(panel2,ISurfaceComposer::eOrientation180,0);
				break;
			case 270:
				secondClient->setOrientation(panel2,ISurfaceComposer::eOrientation270,0);
				break;
			default:
				secondClient->setOrientation(panel2,ISurfaceComposer::eOrientationDefault,0);
			};
		}
    secondSurfaceCtrl->setAlpha(alpha2);
    secondClient->closeTransaction();
	}

	int filedes1 = -1;
	int file1size = 0;
	mapping_data_t* data1;
	int cnt1 = 0, iter = 0;
	overlay_buffer_t buffer1;
	int numBuffers1;
	uint32_t page_width1 = (w1 * 2   + 4096 -1) & ~(4096 -1);  // width rounded to the 4096 bytes
	uint32_t framesize1 = w1*h1*2;
	uint8_t* localbuffer1 = new uint8_t [framesize1];

	int filedes2 = -1;
	int file2size = 0;
	mapping_data_t* data2;
	int cnt2 = 0;
	overlay_buffer_t buffer2;
	int numBuffers2;
	uint32_t page_width2 = (w2 * 2   + 4096 -1) & ~(4096 -1);  // width rounded to the 4096 bytes
	uint32_t framesize2 = w2*h2*2;
	uint8_t* localbuffer2 = new uint8_t [framesize2];

	filedes1 = open(img1, O_RDONLY);
	if ( filedes1 < 0 ) {
		LOGE("\n\n\n\nError: failed to open the file %s for reading\n\n\n\n", img1);
		return;
	}
	struct stat stat;
	fstat(filedes1, &stat);
	file1size = stat.st_size;
	LOGE("File1 size = %ld", file1size);
	numBuffers1 = mOverlay1->getBufferCount();

	for (int i1 = 0; i1 < numBuffers1; i1++) {
		data1 = (mapping_data_t *)mOverlay1->getBufferAddress((void*)i1);
		if (data1 == NULL)
		{
			LOGE("No Buffer from Overlay [%x]", data1);
			return;
		}
		mBuffers1[i1] = data1->ptr;
	}

	if (img2 != NULL)
	{
		filedes2 = open(img2, O_RDONLY);
		if ( filedes2 < 0 ) {
		LOGE("\nError: failed to open the file %s for reading\n", img2);
		return;
	}
	fstat(filedes2, &stat);
	file2size = stat.st_size;
	LOGE("File2 size = %ld", file2size);
	numBuffers2 = mOverlay2->getBufferCount();
	for (int i2 = 0; i2 < numBuffers2; i2++) {
		data2 = (mapping_data_t *)mOverlay2->getBufferAddress((void*)i2);
		if (data2 == NULL)
		{
			LOGE("No Buffer from Overlay [%x]", data2);
			return;
		}
		mBuffers1[i2] = data2->ptr;
		}
	}

 int x, y, w, h, cropw, croph, cropx, cropy;
 int totalIter = file1size/(framesize1*numBuffers1);

	for (int i3 = 0; i3 < 3; i3++)
	{
		// copy the file into the buffers:
		//we can use read: provided we have the file des
		int ret1 = read (filedes1, (void*)localbuffer1, framesize1);
		if (ret1 != framesize1)
		{
			LOGE ("\nReading error file1.[%s]\n", errno);
			return;
		}
		file1size -= framesize1;

		uint8_t *p1 = (uint8_t*) mBuffers1[cnt1];
		uint8_t* runningptr1 = localbuffer1;

		#ifdef TARGET_OMAP4
		for (int k1 = 0; k1 < h1  ; k1 += 1){
		for (int j1 = 0; j1 < w1; j1+= 2){
			*(uint32_t *)(p1) = *(uint32_t*)(runningptr1);
			p1 += 4;
			runningptr1 += 4;
		}
			p1    +=  page_width1 - w1*2; //go to the beginning of the next row
		}
		#else
			memcpy(mBuffers1[cnt1], runningptr1, framesize1);
		#endif
		mOverlay1->queueBuffer((void*)(cnt1++));

		if (img2 != NULL)
		{
			//we can use pread: provided we have the file des
			int ret2 = read (filedes2, (void*)localbuffer2, framesize2);
			if (ret2 != framesize2)
			{
				LOGE ("\nReading error file1.[%s]\n", errno);
				return;
			}
			file2size -= framesize2;
			uint8_t *p2 = (uint8_t*) mBuffers2[cnt2];
			uint8_t* runningptr2 = localbuffer2;
			#ifdef TARGET_OMAP4
			for (int k2 = 0; k2 < h2  ; k2 += 1){
			for (int j2 = 0; j2 < w2; j2 += 2){
				*(uint32_t *)(p2) = *(uint32_t*)(runningptr2);
				p2 += 4;
				runningptr2 += 4;
			}
				p2    +=  page_width2 - w2*2; //go to the beginning of the next row
			}
			#else
				memcpy(mBuffers2[cnt2], runningptr2, framesize2);
			#endif
			mOverlay2->queueBuffer((void*)(cnt2++));
		}
	}

	do{
	if (file1size > 0)
	{
		int ret1 = read (filedes1, (void*)localbuffer1, framesize1);
		if (ret1 != framesize1)
		{
			LOGE ("\nReading error file or File End.[%s]\n", errno);
			break;
		}
		file1size -= framesize1;
		uint8_t *p1 = (uint8_t*) mBuffers1[cnt1];
		uint8_t* runningptr1 = localbuffer1;
		#ifdef TARGET_OMAP4
		for (int k1 = 0; k1 < h1  ; k1 += 1) {
		for (int j1 = 0; j1 < w1; j1 += 2) {
			*(uint32_t *)(p1) = *(uint32_t*)(runningptr1);
			p1 += 4;
			runningptr1 += 4;
		}
			p1    +=  page_width1 - w1*2; //go to the beginning of the next row
		}
		#else
			memcpy(mBuffers1[cnt1], runningptr1, framesize1);
		#endif

	//start queueing the buffers
	mOverlay1->queueBuffer((void*)(cnt1++));

	//now deque the buffers
	mOverlay1->dequeueBuffer(&buffer1);
	}
	if (img2 != NULL)
	{
		if (file2size > 0)
		{
			int ret2 = read (filedes2, (void*)localbuffer2, framesize2);
			if (ret2 != framesize2)
			{
				LOGE ("\nReading error file2.[%s]\n", errno);
				break;
			}
			file2size -= framesize2;
			uint8_t *p2 = (uint8_t*) mBuffers2[cnt2];
			uint8_t* runningptr2 = localbuffer2;
			#ifdef TARGET_OMAP4
			for (int k2 = 0; k2 < h2  ; k2 += 1) {
			for (int j2 = 0; j2 < w2; j2 += 2) {
				*(uint32_t *)(p2) = *(uint32_t*)(runningptr2);
				p2 += 4;
				runningptr2 += 4;
			}
				p2    +=  page_width2 - w2*2; //go to the beginning of the next row
			}
			#else
				memcpy(mBuffers2[cnt2], runningptr2, framesize2);
			#endif

			//start queueing the buffers
			mOverlay2->queueBuffer((void*)(cnt2++));
			//now deque the buffers
			mOverlay2->dequeueBuffer(&buffer2);
		}
	}

	if (cnt1 > (numBuffers1-1))
	{
		iter++;
		cnt1 = 0;
		cnt2 = 0;

		if (zoom1)
		{
			bool changed = false;
			if (iter == 1)
			{
				cropx = 30;
				cropy = 30;
				cropw = 100;
				croph = 100;
				changed = true;
			}
			else if (iter == totalIter/4)
			{
				cropx = 100;
				cropy = 0;
				cropw = 100;
				croph = 100;
				changed = true;
			}
			else if (iter == totalIter/2)
			{
				cropx = 0;
				cropy = 100;
				cropw = 100;
				croph = 100;
				changed = true;
			}
			else if (iter == 3*totalIter/4)
			{
				cropx = 80;
				cropy = 80;
				cropw = 100;
				croph = 100;
				changed = true;
			}
			if (changed)
			{
				mOverlay1->setCrop(cropx, cropy,cropw, croph);
				firstClient->openTransaction();
				firstSurfaceCtrl->setPosition(50, 50);
				firstSurfaceCtrl->setSize(w1, h1);
				firstClient->closeTransaction();
				changed = false;
			}
		}
		if (autoscale1)
		{
			if (iter < 100)
			{
				firstClient->openTransaction();
				firstSurfaceCtrl->setPosition(30+iter*8, 30+iter*4);
				firstSurfaceCtrl->setSize(w1 -(w1*iter)/100, h1-(h1*iter)/100);
				firstClient->closeTransaction();
			}
			else if (iter < 200)
			{
				firstClient->openTransaction();
				firstSurfaceCtrl->setPosition(30+(iter-100)*8, 30+(iter-100)*4);
				firstSurfaceCtrl->setSize(w1-(w1*(iter-100))/(100), h1-(h1*(iter-100))/(100));
				firstClient->closeTransaction();
			}
			else if (iter < 300)
			{
				firstClient->openTransaction();
				firstSurfaceCtrl->setPosition(30+(iter-200)*8, 30+(iter-200)*4);
				firstSurfaceCtrl->setSize(w1-(w1*(iter-200))/(100), h1-(h1*(iter-200))/(100));
				firstClient->closeTransaction();
			}
			else if (iter == 300)
			{
				firstClient->openTransaction();
				firstSurfaceCtrl->setPosition(30, 30);
				firstSurfaceCtrl->setSize(w1, h1);
				firstClient->closeTransaction();
			}
		}

		if (zoom2)
		{
			bool changed = false;
			if (iter == 1)
			{
				cropx = 30;
				cropy = 30;
				cropw = 100;
				croph = 100;
				changed = true;
			}
			else if (iter == totalIter/4)
			{
				cropx = 100;
				cropy = 0;
				cropw = 100;
				croph = 100;
				changed = true;
			}
			else if (iter == totalIter/2)
			{
				cropx = 0;
				cropy = 100;
				cropw = 100;
				croph = 100;
				changed = true;
			}
			else if (iter == 3*totalIter/4)
			{
				cropx = 80;
				cropy = 80;
				cropw = 100;
				croph = 100;
				changed = true;
			}
			if (changed)
			{
				mOverlay2->setCrop(cropx, cropy,cropw, croph);
				secondClient->openTransaction();
				secondSurfaceCtrl->setPosition(50, 50);
				secondSurfaceCtrl->setSize(w2, h2);
				secondClient->closeTransaction();
				changed = false;
			}
		}

		if (autoscale2)
		{
			if (iter < 100)
			{
				secondClient->openTransaction();
				secondSurfaceCtrl->setPosition(30+iter*8, 30+iter*4);
				secondSurfaceCtrl->setSize(w2 -(w2*iter)/100, h2-(h2*iter)/100);
				secondClient->closeTransaction();
			}
			else if (iter < 200)
			{
				secondClient->openTransaction();
				secondSurfaceCtrl->setPosition(30+(iter-100)*8, 30+(iter-100)*4);
				secondSurfaceCtrl->setSize(w2-(w2*(iter-100))/(100), h2-(h2*(iter-100))/(100));
				secondClient->closeTransaction();
			}
			else if (iter < 300)
			{
				secondClient->openTransaction();
				secondSurfaceCtrl->setPosition(30+(iter-200)*8, 30+(iter-200)*4);
				secondSurfaceCtrl->setSize(w2-(w2*(iter-200))/(100), h2-(h2*(iter-200))/(100));
				secondClient->closeTransaction();
			}
			else if (iter == 300)
			{
				secondClient->openTransaction();
				secondSurfaceCtrl->setPosition(30, 30);
				secondSurfaceCtrl->setSize(w2, h2);
				secondClient->closeTransaction();
			}
		}
	}
	}while((file1size > 0) || (file2size > 0));

	printf("Playback Done");
	resetParams();
	if (mOverlay1 != NULL)
	{
		mOverlay1->destroy();
		mOverlay1 = NULL;
	}
	if (mOverlay2 != NULL)
	{
		mOverlay2->destroy();
		mOverlay2 = NULL;
	}
	if (localbuffer1 != NULL)
	{
		delete localbuffer1;
		localbuffer1 = NULL;
	}
	if (localbuffer2 != NULL)
	{
		delete localbuffer2;
		localbuffer2 = NULL;
	}
	LOGE("testOverlay--");
	}

/** Initialize the surfaces**/
int OverlayTest::init()
{
	LOGE("init++");
	// set up the thread-pool
	mProc = (ProcessState::self());
	ProcessState::self()->startThreadPool();

	// create a client to surfaceflinger
	mClient1 = new SurfaceComposerClient();
	if (mClient1 == NULL)
	{
		LOGE("No Memory[%d]", __LINE__);
		return -1;
	}
	// create pushbuffer surface
	mSurfaceCtrlLcd1 = (mClient1->createSurface(getpid(), OVERLAY_ON_PRIMARY, LCD_WIDTH, LCD_HEIGHT,
						PIXEL_FORMAT_UNKNOWN, ISurfaceComposer::ePushBuffers | ISurfaceComposer::eFXSurfaceNormal));
	if (mSurfaceCtrlLcd1 == NULL)
	{
		LOGE("No Memory[%d]", __LINE__);
		return -1;
	}

	// get to the isurface
	Test::getISurface(mSurfaceCtrlLcd1);
	mSurfaceLcd1 = mSurface;
	if (mSurfaceLcd1 == NULL)
	{
		LOGE("No Memory[%d]", __LINE__);
		return -1;
	}

	// create a client to surfaceflinger
	mClient2 = new SurfaceComposerClient();
	if (mClient2 == NULL)
	{
		LOGE("No Memory[%d]", __LINE__);
		return -1;
	}
	// create pushbuffer surface
	mSurfaceCtrlLcd2 = (mClient2->createSurface(getpid(), OVERLAY_ON_SECONDARY, LCD_WIDTH, LCD_HEIGHT,
						PIXEL_FORMAT_UNKNOWN, ISurfaceComposer::ePushBuffers | ISurfaceComposer::eFXSurfaceNormal));
	if (mSurfaceCtrlLcd2 == NULL)
	{
		LOGE("No Memory[%d]", __LINE__);
		return -1;
	}

	// get to the isurface
	Test::getISurface(mSurfaceCtrlLcd2);
	mSurfaceLcd2 = mSurface;
	if (mSurfaceLcd2 == NULL)
	{
		LOGE("No Memory[%d]", __LINE__);
		return -1;
	}

	// create pushbuffer surface
	mSurfaceCtrlTv = (mClient1->createSurface(getpid(), OVERLAY_ON_TV, LCD_WIDTH, LCD_HEIGHT,
					PIXEL_FORMAT_UNKNOWN, ISurfaceComposer::ePushBuffers | ISurfaceComposer::eFXSurfaceNormal));
	if (mSurfaceCtrlTv == NULL)
	{
		LOGE("No Memory[%d]", __LINE__);
		return -1;
	}

	// get to the isurface
	Test::getISurface(mSurfaceCtrlTv);
	mSurfaceTv = mSurface;
	if (mSurfaceTv == NULL)
	{
		LOGE("No Memory[%d]", __LINE__);
		return -1;
	}

	// create pushbuffer surface
	mSurfaceCtrlDlp = (mClient2->createSurface(getpid(), OVERLAY_ON_PICODLP, LCD_WIDTH, LCD_HEIGHT,
					PIXEL_FORMAT_UNKNOWN, ISurfaceComposer::ePushBuffers | ISurfaceComposer::eFXSurfaceNormal));
	if (mSurfaceCtrlDlp == NULL)
	{
		LOGE("No Memory[%d]", __LINE__);
		return -1;
	}

	// get to the isurface
	Test::getISurface(mSurfaceCtrlDlp);
	mSurfaceDlp = mSurface;
	if (mSurfaceDlp == NULL)
	{
		LOGE("No Memory[%d]", __LINE__);
		return -1;
	}
	resetParams();
	return 0;
	LOGE("init--");

}
}; //android name space

/** main entry point**/
int main (int argc, char* argv[])
{
	if ((argc < 7) || ((argc >8) && (argc <11))) {
	printf("\nUsage: for single overlay");
	printf("\noverlay_test <single/multi ovly> <test scenario> <file> <w> <h> <panel>");
	printf("\nExample: ./system/bin/overlay_test S 1 test.yuv 320 240 0");
	printf("\nUsage: for multiple overlay");
	printf("\noverlay_test <single/multi ovly> <test scenario> <file1> <w1> <h1> <panel1> <file2> <w2> <h2> <panel2>");
	printf("\nExample: ./system/bin/overlay_test M 5 test1.yuv 320 240 0 test2.rgb 640 480 1");
	printf("\ntest scenarios");
	printf("\n1 -\tTo test Alpha Blend");
	printf("\n2 -\tTo test Scaling");
	printf("\n3 -\tTo test Rotation");
	printf("\n4 -\tTo test Mirroring");
	printf("\n5 -\tTo test z-order");
	printf("\n6 -\tTo test digital Zoom");
	printf("\n7 -\tTo test auto scale");
	printf("\nPANEL Ids");
	printf("\n0 -\tPrimary LCD");
	printf("\n1 -\tSecondary LCD");
	printf("\n2 -\tHDTV");
	printf("\n3 -\tPICO DLP\n");

	return 0;
	}

	OverlayTest test;
	int ret = test.init();
	if (ret != 0)
	{
		printf("Overlay Test Init failed\n");
		return 0;
	}
	int val = -1;
	bool cmp1 = strcmp(argv[1], "S");
	bool cmp2 = strcmp(argv[1], "s");
	if ((!cmp1) || (!cmp2))
	{
		switch(atoi(argv[2]))
		{
		case 1:
			printf("Enter the Alpha value (1 to 10)\n");
			scanf("%d", &val);
			test.alpha1 = val/1.0;
		break;

		case 2:
			printf("Enter the new width\n");
			scanf("%d", &val);
			test.newwidth1 = val;
			printf("Enter the new height\n");
			scanf("%d", &val);
			test.newheight1 = val;
		break;

		case 3:
			printf("Enter the rotation angle (0/90/180/270\n");
			scanf("%d", &val);
			test.deg1 = val;
		break;

		case 4:
			printf("Enter the mirror (1-Horizontal, 2-Vertical)\n");
			scanf("%d", &val);
			test.mirror1 = val;
		break;

		case 5:
			printf("Enter the zOrder (0 to 3)\n");
			scanf("%d", &val);
			test.zOrder1 = val;
		break;

		case 6:
			test.zoom1 = true;
		break;

		case 7:
			test.autoscale1 = true;
		break;

		default:
			printf("Un supported option: enter 1 -6\n");
		return 0;
		};
		test.testOverlay(argv[3], atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
	}
	else
	{
		switch(atoi(argv[2]))
		{
			case 1:
				printf("Enter the first Alpha value (1 to 10)\n");
				scanf("%d", &val);
				test.alpha1 = val/1.0;
				printf("Enter the second Alpha value (1 to 10)\n");
				scanf("%d", &val);
				test.alpha2 = val/1.0;
			break;

			case 2:
				printf("Enter the first new width\n");
				scanf("%d", &val);
				test.newwidth1 = val;
				printf("Enter the first new height\n");
				scanf("%d", &val);
				test.newheight1 = val;
				printf("Enter the sec new width\n");
				scanf("%d", &val);
				test.newwidth2 = val;
				printf("Enter the sec new height\n");
				scanf("%d", &val);
				test.newheight2 = val;
			break;

			case 3:
				printf("Enter the first rotation angle (0/90/180/270)\n");
				scanf("%d", &val);
				test.deg1 = val;
				printf("Enter the sec rotation angle (0/90/180/270)\n");
				scanf("%d", &val);
				test.deg2 = val;
			break;

			case 4:
				printf("Enter the first mirror (1-Horizontal, 2-Vertical)\n");
				scanf("%d", &val);
				test.mirror1 = val;
				printf("Enter the sec mirror (1-Horizontal, 2-Vertical)\n");
				scanf("%d", &val);
				test.mirror2 = val;
			break;

			case 5:
				printf("Enter the first zOrder (0 to 3)\n");
				scanf("%d", &val);
				test.zOrder1 = val;
				printf("Enter the sec zOrder (0 to 3)\n");
				scanf("%d", &val);
				test.zOrder2 = val;
			break;

			case 6:
				test.zoom1 = true;
				test.zoom2 = true;
			break;

			case 7:
				test.autoscale1 = true;
				test.autoscale2 = true;
			break;

			default:
				printf("Un supported option: enter 1 -6\n");
			return 0;
		};
		test.testOverlay(argv[3], atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), argv[7], atoi(argv[8]), atoi(argv[9]), atoi(argv[10]));
	}
return 0;
}
