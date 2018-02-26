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

	SaveRequest(int which_camera, unsigned char* ptr, int w, int h) : data(ptr), width(w), height(h), camera(which_camera)
	{
		FILETIME ft;
		GetSystemTimePreciseAsFileTime(&ft);
		FileTimeToSystemTime(&ft, &st);
	}

	void save();
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
		
		int numCams;
		bool recording;
		int draw_single_camera;

		std::vector<SaveRequest> save_requests;
		std::thread save_thread;
		std::mutex  save_mutex;
		std::condition_variable save_cv;
		bool should_quit;

		std::queue<unsigned char*> reusable_frame_chunks;
		std::mutex reusable_mutex;

		//std::vector<Camera> cameras;
};

#endif	// _triangle_APP