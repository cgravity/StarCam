#ifndef _TRIANGLE_APP

#include <stdio.h>
#include "glfw.h"
#include "simpleApp.h"
#include "imageTexture.h"
#include "videoInput.h"
#include "uuids.h"

#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <queue>

#include <windows.h>

struct SaveRequest
{
	unsigned char* data;
	int width;
	int height;
    
	int camera;
	SYSTEMTIME st;
    double dshow_time; // time from directshow is always 0, unfortunately :(
    
    int one_shot_tag;
    bool need_one_shot;

	SaveRequest(int which_camera, unsigned char* ptr, int w, int h) : data(ptr), width(w), height(h), camera(which_camera), one_shot_tag(0x7FFFFFFF), need_one_shot(false), dshow_time(0.0)
	{
//		FILETIME ft;
//		GetSystemTimePreciseAsFileTime(&ft);
//		FileTimeToSystemTime(&ft, &st);
	}

	void save();
};

class triangleApp;

struct CameraSaveThread
{
    triangleApp* triangle_app;
    
    std::mutex mutex;   // protects requests, free_buffers, etc.
    std::condition_variable cv; // signaled when save thread should take action
    
    std::vector<SaveRequest> requests;
    std::vector<unsigned char*> free_buffers;
    bool should_quit;
    bool need_one_shot;
    int  one_shot_tag;
    
    std::vector<SaveRequest> thread_local_requests;
    std::thread save_thread;
    
    CameraSaveThread() : should_quit(false) {}
};

class triangleApp : public simpleApp{
	
	public:
		triangleApp();
		virtual void idle();
		virtual void init();
		virtual void draw();
		virtual void keyDown  (int c);
		virtual void mouseMove( float x, float y );
		virtual void mouseDrag( float x, float y );
		virtual void mouseDown( float x, float y, int button );
		virtual void mouseUp  ( float x, float y, int button );
		

		videoInput     VI;

		std::vector <imageTexture*> IT;
		std::vector <unsigned char*> frames;
        
        std::vector<unsigned char*> small_image_data;
        std::vector<imageTexture*> small_images;
		
		int numCams;
		bool recording;
        bool one_shot_save; // e.g. for camera calibration frame
		int draw_single_camera;
        bool should_quit;
        
        int one_shot_tag;
        
        std::vector<CameraSaveThread*> save_threads;
};

#endif	// _triangle_APP