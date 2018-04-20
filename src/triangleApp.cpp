#include "triangleApp.h"
#include <windows.h>
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")

#include <iostream>
#include <cstdint>
#include <vector>

#define INIT_FRAME_SAVE_BUFFER_COUNT    50

// these values are initialized in init(); don't modify elsewhere
std::vector<char> valid_drives;

// make sure -mno-ms-bitfields is specified if compiling with MinGW
// otherwise attribute packed is ignored (WTF?!)
struct BMP_Header
{
    uint8_t signature[2]; // always ['B', 'M']
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t pixel_offset;
} __attribute__((packed));

struct DIB_Header
{
    uint32_t dib_header_size;
    int32_t width;
    int32_t height;
    uint16_t color_planes; // always 1
    uint16_t bits_per_pixel;
    uint32_t compression_mode;
    uint32_t image_size;
    uint32_t h_res_meters;
    uint32_t v_res_meters;
    uint32_t color_palette_count;
    uint32_t important_colors;
} __attribute__((packed));

BMP_Header bmp_header;
DIB_Header dib_header;

// choose output format: comment out next line to use TGA, uncomment to use BMP
//#define USE_BMP

#ifdef USE_BMP
    #define OUT_EXT ".bmp"
#else
    #define OUT_EXT ".tga"
#endif

void SaveRequest::save()
{
	char filename[256];
    char one_shot_prefix[20];
    
    if(need_one_shot)
        snprintf(one_shot_prefix, sizeof(one_shot_prefix), "c_%d_", one_shot_tag);
    else
        one_shot_prefix[0] = '\0';
    
	snprintf(filename, sizeof(filename),
        "C://test//%d//%s%02d_%02d_%02d_%03d" OUT_EXT, camera, 
        one_shot_prefix, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    
    filename[0] = valid_drives[camera % valid_drives.size()];
    
    //if(camera % 2 == 0)
    //    filename[0] = 'A';
    
	FILE * pImgFile;

	pImgFile = fopen(filename, "wb");
    setbuf(pImgFile, NULL); // disable write buffering

    // BMP header
  #ifdef USE_BMP
    fwrite(&bmp_header, sizeof(bmp_header), 1, pImgFile);
    fwrite(&dib_header, sizeof(dib_header), 1, pImgFile);
  #else
	// TGA header
	putc(0, pImgFile);
	putc(0, pImgFile);
	putc(2, pImgFile);                         // uncompressed RGB
	putc(0, pImgFile); putc(0, pImgFile);
	putc(0, pImgFile); putc(0, pImgFile);
	putc(0, pImgFile);
	putc(0, pImgFile); putc(0, pImgFile);           // X origin
	putc(0, pImgFile); putc(0, pImgFile);           // y origin
	putc((1920 & 0x00FF), pImgFile);
	putc((1920 & 0xFF00) / 256, pImgFile);
	putc((1080 & 0x00FF), pImgFile);
	putc((1080 & 0xFF00) / 256, pImgFile);
	putc(24, pImgFile);                        // 24 bit bitmap
	putc(0, pImgFile);
  #endif
  
    // Assumes that a row can always be divided by 4 -- otherwise,
    // padding bytes must be added
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

void bg_save(CameraSaveThread* cst)
{
    triangleApp* app = cst->triangle_app;
	std::vector<SaveRequest>& requests = cst->thread_local_requests;

	bool should_quit = false;

    while(!should_quit)
    {
        // wait until there's work to do
        {
            std::unique_lock<std::mutex> lock(cst->mutex);
            requests.swap(cst->requests);     // FIXME: This causes crashes?!
            should_quit = cst->should_quit;
            
            
            
            while(requests.size() == 0 && !should_quit)
            {
                cst->cv.wait(lock);
                requests.swap(cst->requests);
                should_quit = cst->should_quit;
            }
        }
        
        for(size_t i = 0; i < requests.size(); i++)
        {
            requests[i].save();
        }
        
        {
            std::unique_lock<std::mutex> lock(cst->mutex);
            //cst->mutex.lock();
            for(size_t i = 0; i < requests.size(); i++)
            {
                cst->free_buffers.push_back(requests[i].data);
                requests[i].data = NULL;
            }
            //cst->mutex.unlock();
        }
        
        requests.clear();
    }
}

//empty constructor
triangleApp::triangleApp()
{
	//save_thread = std::thread(bg_save, this);
}

void triangleApp::init(){
    
    valid_drives.push_back('C');
    valid_drives.push_back('C');
    valid_drives.push_back('D');
    valid_drives.push_back('F');
    valid_drives.push_back('G');
    valid_drives.push_back('H');
    valid_drives.push_back('I');
    valid_drives.push_back('J');
    
    memset(&bmp_header, 0, sizeof(bmp_header));
    bmp_header.signature[0] = 'B';
    bmp_header.signature[1] = 'M';
    bmp_header.file_size = sizeof(bmp_header) + sizeof(dib_header) + 1920*1080*3;
    bmp_header.pixel_offset = sizeof(dib_header);
    
    memset(&dib_header, 0, sizeof(dib_header));
    dib_header.dib_header_size = sizeof(dib_header);
    dib_header.width = 1920;
    dib_header.height = 1080;
    dib_header.color_planes = 1;
    dib_header.bits_per_pixel = 24;
    dib_header.image_size = 0;
    
	numCams = 0;
    one_shot_tag = 0;
	recording = false;
    one_shot_save = false;
	draw_single_camera = -1; // draw all cameras
    should_quit = false;

	//uncomment for silent setup
	//videoInput::setVerbose(false); 

	//uncomment for multithreaded setup
	videoInput::setComMultiThreaded(true); 

	//optional static function to list devices
	//for silent listDevices use listDevices(true);
	numCams = videoInput::listDevices();	

    // allocate camera threads and save-buffer memory
    for(int i = 0; i < numCams; i++)
    {
        CameraSaveThread* cst = new CameraSaveThread();
        cst->triangle_app = this;
        cst->save_thread = std::thread(bg_save, cst);
        save_threads.push_back(cst);
        
        for(int j = 0; j < INIT_FRAME_SAVE_BUFFER_COUNT; j++)
        {
            unsigned char* buffer = new unsigned char[1920*1080*3];
            cst->free_buffers.push_back(buffer);
        }
    }
    
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
    
    for(int i = 0; i < list.size(); i++)
    {
        small_image_data.push_back(new unsigned char[1920/4 * 1080/4 * 3]);
        small_images.push_back(new imageTexture(1920/4, 1080/4, GL_RGB));
    }

	//by default we use a callback method
	//this updates whenever a new frame
	//arrives if you are only ocassionally grabbing frames
	//you might want to set this to false as the callback caches the last
	//frame for performance reasons. 
	VI.setUseCallback(true);

/*
    // restart cameras after dropped frames detected
    for(int i = 0; i < numCams; i++)
    {
        VI.setAutoReconnectOnFreeze(i, true, 60);
    }
*/
    
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

void squash_frame(unsigned char* small, unsigned char* big)
{
    int i = 0;
    
    for(int y = 0; y < 1080/4; y++)
    for(int x = 0; x < 1920/4; x++)
    {
        // note: pixel selection converts BGR -> RGB
        unsigned char r = big[3*(1920*(4*y) + (4*x)) + 2];
        unsigned char g = big[3*(1920*(4*y) + (4*x)) + 1];
        unsigned char b = big[3*(1920*(4*y) + (4*x)) + 0];
        
        small[3*i + 0] = r;
        small[3*i + 1] = g;
        small[3*i + 2] = b;
        i++;
    }
}

void triangleApp::idle()
{    
    if(should_quit)
        return;

    FILETIME ft;
    SYSTEMTIME st;
    GetSystemTimePreciseAsFileTime(&ft);
    FileTimeToSystemTime(&ft, &st);
    
    int one_shot_save_counter = 0;
    
	for (int i = 0; i < numCams; i++) {
		if (VI.isFrameNew(i)) {
            double timestamp = 0;
			VI.getPixels(i, frames[i], timestamp, false);
            

			if (recording || one_shot_save)
			{
                CameraSaveThread* cst = save_threads[i];
                
                unsigned char* buffer = NULL;
                {
                    std::unique_lock<std::mutex> lock(cst->mutex);
                    
                //cst->mutex.lock();
                    
                    if(cst->free_buffers.size() > 0)
                    {
                        buffer = cst->free_buffers[cst->free_buffers.size()-1];
                        cst->free_buffers.pop_back();
                    }
                //cst->mutex.unlock();
                }
                
                if(buffer == NULL)
                    buffer = new unsigned char[1920*1080*3];
                
				memcpy(buffer, frames[i], 1920 * 1080 * 3);
				SaveRequest request(i, buffer, 1920, 1080);
                request.st = st;
                request.dshow_time = timestamp;
                
                // push work to thread and signal
                {
                    std::unique_lock<std::mutex> lock(cst->mutex);
                    
                    bool need_push = recording;
                    
                    if(one_shot_save && cst->need_one_shot)
                    {
                        cst->need_one_shot = false;
                        request.one_shot_tag = cst->one_shot_tag;
                        request.need_one_shot = true;
                        
                        need_push = true;
                        
                        printf("One shot for camera %d (of %d) [counter: %d]\n", i, numCams, one_shot_save_counter);
                    }
                    
                    if(one_shot_save)
                    {
                        if(cst->one_shot_tag == one_shot_tag)
                            one_shot_save_counter++;
                    }
                    
                    if(need_push)
                    {
                        cst->requests.push_back(request);
                        cst->cv.notify_all();
                    }
                    else
                    {
                        cst->free_buffers.push_back(buffer);
                    }
                }
			}
		}
	}
    
    if(one_shot_save)
    {
        printf("ONE SHOT! counter: %d\n", one_shot_save_counter);
        fflush(stdout);
    }
    
    if(one_shot_save && one_shot_save_counter == numCams)
        one_shot_save = false;

    if(draw_single_camera == -1)
    {
        // upload all frames
        for (int i = 0; i < numCams; i++)
        {
            //IT[i]->loadImageData(frames[i], VI.getWidth(i), VI.getHeight(i), 0x80E0);
            squash_frame(small_image_data[i], frames[i]);
            small_images[i]->loadImageData(small_image_data[i], 1920/4, 1080/4, GL_RGB);
        }
    }
    else if(draw_single_camera == -2)
    {
        // don't upload anything
    }
    else
    {
        // upload only the camera we want to see
        int i = draw_single_camera;
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

			//IT[i]->renderTexture(draw_x, draw_y, 1920 / 4, 1080 / 4);
            small_images[i]->renderTexture(draw_x, draw_y, 1920 / 4, 1080 / 4);
			draw_x += 1920 / 4;
		}
	}

}

void triangleApp::keyDown  (int c){

    if(c == 'G')
    {
        
        std::cerr << "Writing graph to C:\\test\\debug_graph.grf\n";
        VI.SaveGraphFile(VI.VDList[0]->pGraph, L"C:\\test\\debug_graph.grf");
    }

	if (c == GLFW_KEY_ESC)
	{
        should_quit = true;
        
        for(size_t i = 0; i < save_threads.size(); i++)
        {
            CameraSaveThread* cst = save_threads[i];
            std::unique_lock<std::mutex> lock(cst->mutex);
            cst->should_quit = true;
            cst->cv.notify_all();
        }
	}
    
    if(c == GLFW_KEY_SPACE)
    {
        one_shot_save = true;
        one_shot_tag++;
        for(size_t i = 0; i < save_threads.size(); i++)
        {
            CameraSaveThread* cst = save_threads[i];
            std::unique_lock<std::mutex> lock(cst->mutex);
            cst->need_one_shot = true;
            cst->one_shot_tag = one_shot_tag;
        }
    }

	// settings in single camera mode
	if(c=='S' && draw_single_camera >= 0) VI.showSettingsWindow(draw_single_camera);

	// camera mode selection
	if (c >= '1' && c <= '8')
	{
		draw_single_camera = c - '1';
        
        if(glfwGetKey(GLFW_KEY_LSHIFT) == GLFW_PRESS)
            draw_single_camera += 8;
        
        //std::cerr << "Showing view: " << VI.getDeviceName(draw_single_camera) << '\n';
	}
	if (c == '0')
	{
		draw_single_camera = -1;
	}
    
    if(c == '-')
    {
        draw_single_camera = -2;
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
            
			printf("%d Gain: %d\n", i, VI.getVideoSettingFilter(i, VI.propGain, min, max, delta, val, flags, def));
			VI.setVideoSettingFilter(i, VI.propGain, def, 2);
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
	if (c == '[') {
        recording = true;
        printf("recording started\n");
    }
	if (c == ']') {
        recording = false;
        printf("recording stopped\n");
    }
}

void triangleApp::mouseMove( float x, float y ){

}

void triangleApp::mouseDrag( float x, float y ){

}

void triangleApp::mouseDown( float x, float y, int button ){

}

void triangleApp::mouseUp  ( float x, float y, int button ){

}



