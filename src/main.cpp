// videoInputBasicDemo.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
triangleApp TAPP;


#include "network.h"
NetworkThread* networking = NULL;
	 

void GLFWCALL keyfun( int key, int action )
{
    if( action != GLFW_PRESS )
    {
        return;
    }

    switch( key )
    {
	    case GLFW_KEY_ESC:
        	printf( "ESC => quit program\n" );
        	break;    	
       
    }

    static Message msg;
    msg.write_char('k'); // key pressed
    msg.write_int32(key);  // which key
    networking->send(msg);
    
	TAPP.keyDown(key);

    fflush( stdout );
}




int main( void )
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
    
    char my_hostname[1024];
    memset(my_hostname, 0, sizeof(my_hostname));
    gethostname(my_hostname, sizeof(my_hostname));
    printf("my hostname: \"%s\"\n", my_hostname);
    
    if(strcmp(my_hostname, "sensei-box") == 0)
        networking = new Server();
    else
        networking = new Client("10.0.0.2");

    networking->start_thread();
    
    int     width, height, running, frames, x, y;
    double  t, t0, fps;
    char    titlestr[ 200 ];
	

    // Initialise GLFW
    glfwInit();

    // Open OpenGL window    
    if (!glfwOpenWindow(1920,1080,    // Open window
    24, 24, 24,                                // Red, green, and blue bits for color buffer
    24,                                        // Bits for alpha buffer
    24,                                        // Bits for depth buffer (Z-buffer)
    24,                                        // Bits for stencil buffer
    GLFW_WINDOW)){
        glfwTerminate();
        return 0;
    }
    
    glfwSetKeyCallback( keyfun );
    TAPP.init();
    //glfwEnable( GLFW_STICKY_KEYS );
    // Disable vertical sync (on cards that support it)
    glfwSwapInterval( 0 );

    // Main loop
    running = GL_TRUE;
    frames = 0;
    t0 = glfwGetTime();
    while( !TAPP.should_quit ){
    
        // Get time and mouse position
        t = glfwGetTime();
        glfwGetMousePos( &x, &y );
        
        // Calculate and display FPS (frames per second)
        if( (t-t0) > 1.0 || frames == 0 )
        {
            fps = (double)frames / (t-t0);
            sprintf( titlestr, "videoInput Demo App (%.1f FPS)", fps );
            glfwSetWindowTitle( titlestr );
            t0 = t;
            frames = 0;
        }
        frames ++;
        TAPP.idle();
		 
		
        // Get window size (may be different than the requested size)
        glfwGetWindowSize( &width, &height );
        height = height > 0 ? height : 1;
        // Set viewport
        glViewport( 0, 0, width, height );
        // Clear color buffer
        glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
        glClear( GL_COLOR_BUFFER_BIT );

		  TAPP.draw();			
        // Swap buffers
        glfwSwapBuffers();
    }

    printf("Waiting for save threads to finish...\n");
    for(size_t i = 0; i < TAPP.save_threads.size(); i++)
    {
        CameraSaveThread* cst = TAPP.save_threads[i];
        cst->save_thread.join();
        printf("Save thread %llu of %llu done.\n", i+1, TAPP.save_threads.size());
	}

    // Close OpenGL window and terminate GLFW
    glfwTerminate();
    return 0;
}
