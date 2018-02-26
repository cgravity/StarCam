#include "triangleApp.h"
#include <windows.h>
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")

#include <iostream>

void SaveRequest::save()
{
	char filename[80];

	sprintf(filename, "C://test//%d//%02d%02d%02d%03d.tga", camera, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    
    if(camera % 2 == 0)
        filename[0] = 'A';
    
	FILE * pImgFile;
	pImgFile = fopen(filename, "wb");

	// TGA header
	putc(0, pImgFile);
	putc(0, pImgFile);
	putc(2, pImgFile);                         /* uncompressed RGB */
	putc(0, pImgFile); putc(0, pImgFile);
	putc(0, pImgFile); putc(0, pImgFile);
	putc(0, pImgFile);
	putc(0, pImgFile); putc(0, pImgFile);           /* X origin */
	putc(0, pImgFile); putc(0, pImgFile);           /* y origin */
	putc((1920 & 0x00FF), pImgFile);
	putc((1920 & 0xFF00) / 256, pImgFile);
	putc((1080 & 0x00FF), pImgFile);
	putc((1080 & 0xFF00) / 256, pImgFile);
	putc(24, pImgFile);                        /* 24 bit bitmap */
	putc(0, pImgFile);

	fwrite(data, sizeof(unsigned char), 3*width*height, pImgFile);
	fclose(pImgFile);
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;  // Failure

	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

void bg_save(triangleApp* app)
{
	static std::vector<SaveRequest> requests;

	bool should_quit = false;

	while (!should_quit)
	{
		//std::unique_lock <std::mutex> lock(app->save_mutex);
		//app->save_cv.wait(lock);

        app->save_mutex.lock();
		requests.swap(app->save_requests);
		should_quit = app->should_quit;
		//lock.unlock();
        app->save_mutex.unlock();

		for (size_t i = 0; i < requests.size(); i++)
		{
			//requests[i].save();
		}

		app->reusable_mutex.lock();
		for (size_t i = 0; i < requests.size(); i++)
		{
			app->reusable_frame_chunks.push(requests[i].data);
			requests[i].data = NULL;
		}
		app->reusable_mutex.unlock();

		requests.clear();
	}
}

//empty constructor
triangleApp::triangleApp()
{
	save_thread = std::thread(bg_save, this);
}

void triangleApp::init(){
	numCams = 0;
	recording = false;
	draw_single_camera = -1; // draw all cameras

	//uncomment for silent setup
	//videoInput::setVerbose(false); 

	//uncomment for multithreaded setup
	videoInput::setComMultiThreaded(true); 

	//optional static function to list devices
	//for silent listDevices use listDevices(true);
	numCams = videoInput::listDevices();	

	//you can also now get the device list as a vector of strings 
	std::vector <std::string> list = videoInput::getDeviceList(); 
	for(int i = 0; i < list.size(); i++){
		printf("[%i] device is %s\n", i, list[i].c_str());
	}

	//we allocate our openGL texture objects
	//we give them a ma size of 1024 by 1024 pixels
	for (int i = 0; i < list.size(); i++) {
		IT.push_back(new imageTexture(2048, 2048, GL_RGB));
	}

	//by default we use a callback method
	//this updates whenever a new frame
	//arrives if you are only ocassionally grabbing frames
	//you might want to set this to false as the callback caches the last
	//frame for performance reasons. 
	VI.setUseCallback(false);

	//if you want to capture at a different frame rate (default is 30) 
	//specify it here, you are not guaranteed to get this fps though.
	//VI.setIdealFramerate(dev, 60);

	//we can specifiy the dimensions we want to capture at
	//if those sizes are not possible VI will look for the next nearest matching size
	//VI.setRequestedMediaSubType((int)MEDIASUBTYPE_MJPG);
	for (int i = 0; i < numCams; i++) {
		VI.setupDevice(i, 1920, 1080, VI_COMPOSITE);
	}
	
	//once the device is setup you can try and
	//set the format - this is useful if your device
	//doesn't remember what format you set it to
	//VI.setFormat(dev, VI_NTSC_M);					//optional set the format

	//we allocate our buffer based on the number
	//of pixels in each frame - this will be width * height * 3
	for (int i = 0; i < numCams; i++) {
		frames.push_back(new unsigned char[VI.getSize(i)]);
	}
}

void triangleApp::idle() {
	static struct frame_chunks {
		std::vector<unsigned char*> ptrs;

		frame_chunks(triangleApp* ta)
		{
			for (int i = 0; i < ta->numCams; i++)
			{
				ptrs.push_back(NULL);
			}
		}
	} frame_chunks(this);

	reusable_mutex.lock();
	for (int i = 0; i < numCams; i++)
	{
		if (frame_chunks.ptrs[i] == NULL)
		{
			if (reusable_frame_chunks.size() > 0)
			{
				frame_chunks.ptrs[i] = reusable_frame_chunks.front();
				reusable_frame_chunks.pop();
			}
			else
			{
				// FIXME: hardcoded frame size
				frame_chunks.ptrs[i] = new unsigned char[1920 * 1080 * 3];
			}
		}
	}
	reusable_mutex.unlock();

	save_mutex.lock();
	for (int i = 0; i < numCams; i++) {
		if (VI.isFrameNew(i)) {
			VI.getPixels(i, frames[i], false);

			// TODO split off writing files to separate thread
			if (recording)
			{
				memcpy(frame_chunks.ptrs[i], frames[i], 1920 * 1080 * 3);
				SaveRequest request(i, frame_chunks.ptrs[i], 1920, 1080);
				save_requests.push_back(request);
				frame_chunks.ptrs[i] = NULL;
			}
		}
	}
	save_mutex.unlock();
	save_cv.notify_one();

	for (int i = 0; i < numCams; i++)
	{
		//load frames into textures
		IT[i]->loadImageData(frames[i], VI.getWidth(i), VI.getHeight(i), 0x80E0);
	}
}


void triangleApp::draw(){
  
	setupScreen();

	if (draw_single_camera >= 0)
	{
		IT[draw_single_camera]->renderTexture(0, 0, VI.getWidth(draw_single_camera), VI.getHeight(draw_single_camera));
	}
	else
	{
		// draw all cameras
		int draw_x = 0;
		int draw_y = 0;

		for (int i = 0; i < numCams; i++) {
			if (i > 0 && i % 4 == 0)
			{
				draw_x = 0;
				draw_y += 1080 / 4;
			}

			IT[i]->renderTexture(draw_x, draw_y, 1920 / 4, 1080 / 4);
			draw_x += 1920 / 4;
		}
	}

}

void triangleApp::keyDown  (int c){

	if (c == GLFW_KEY_ESC)
	{
		save_mutex.lock();
		should_quit = true;
		save_mutex.unlock();
		save_cv.notify_one();
	}

	// settings in single camera mode
	if(c=='S' && draw_single_camera >= 0) VI.showSettingsWindow(draw_single_camera);

	// camera mode selection
	if (c >= '1' && c <= '8')
	{
		draw_single_camera = c - '1';
	}
	if (c == '0')
	{
		draw_single_camera = -1;
	}

	// set settings
	if (c == 'E') {
		long min, max, delta, val, flags, def = 0;
		for (int i = 0; i < numCams; i++)
		{
			printf("%d Exposure: %d\n", i, VI.getVideoSettingCamera(i, VI.propExposure, min, max, delta, val, flags, def));
			VI.setVideoSettingCamera(i, VI.propExposure, def, 2); // exposure flag 2 means manual

			printf("%d WB: %d\n", i, VI.getVideoSettingFilter(i, VI.propWhiteBalance, min, max, delta, val, flags, def));
			VI.setVideoSettingFilter(i, VI.propWhiteBalance, def, 2);

			printf("%d Focus: %d\n", i, VI.getVideoSettingCamera(i, VI.propFocus, min, max, delta, val, flags, def));
			VI.setVideoSettingCamera(i, VI.propFocus, min, 2);
			VI.setVideoSettingCamera(i, VI.propFocus, max, 2);
			VI.setVideoSettingCamera(i, VI.propFocus, 42, 2);
		}
	}

	// adjust exposure
	if (c == '.')
	{
		long min, max, delta, val, flags, def = 0;
		for (int i = 0; i < numCams; i++)
		{
			VI.getVideoSettingCamera(i, VI.propExposure, min, max, delta, val, flags, def);
			VI.setVideoSettingCamera(i, VI.propExposure, val + delta, 2); // exposure flag 2 means manual
		}
	}
	if (c == ',')
	{
		long min, max, delta, val, flags, def = 0;
		for (int i = 0; i < numCams; i++)
		{
			VI.getVideoSettingCamera(i, VI.propExposure, min, max, delta, val, flags, def);
			VI.setVideoSettingCamera(i, VI.propExposure, val - delta, 2); // exposure flag 2 means manual
		}
	}
	// adjust gain
	if (c == '\'')
	{
		long min, max, delta, val, flags, def = 0;
		for (int i = 0; i < numCams; i++)
		{
			VI.getVideoSettingFilter(i, VI.propGain, min, max, delta, val, flags, def);
			VI.setVideoSettingFilter(i, VI.propGain, val + delta, 2); // exposure flag 2 means manual
		}
	}
	if (c == ';')
	{
		long min, max, delta, val, flags, def = 0;
		for (int i = 0; i < numCams; i++)
		{
			VI.getVideoSettingFilter(i, VI.propGain, min, max, delta, val, flags, def);
			VI.setVideoSettingFilter(i, VI.propGain, val - delta, 2); // exposure flag 2 means manual
		}
	}

	// start/stop recording
	if (c == '[') recording = true;
	if (c == ']') recording = false;
}

void triangleApp::mouseMove( float x, float y ){

}

void triangleApp::mouseDrag( float x, float y ){

}

void triangleApp::mouseDown( float x, float y, int button ){

}

void triangleApp::mouseUp  ( float x, float y, int button ){

}


