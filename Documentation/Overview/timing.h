
/*! \page Timing Timing

MainDocs:Timing

From ClanLib Game SDK
 

Refresh Rate 

The refresh rate (sometimes known as vertical sync) is the number of times in a second that display hardware draws the data. 

Historicially in Americia the refresh rate for TV's was 60hz. In Europe it was 50hz. This made is easy for game to judge timing. 

However, PC monitors and modern televisions have a varied refresh rate. Usually between 60hz and 80hz, but refresh rates of 120hz are now available. 

CL_Display::flip() 

You only want to draw to the screen after you have finished drawing, else a partial drawn image is shown (known as screen tearing) 

The display hardware contains 2 screens in memory. One that is being written to and one that is being displayed. For reference, this is known by OpenGL as the GL_DRAW_FRAMEBUFFER and CL_READ_FRAMEBUFFER. 

To swap these screens you need to call flip() 

This is known as "double buffering" or "page switching" 

It is a waste to draw, 2 screens during 1 display refresh. Ideally you should wait after the first screen swap until the next display refresh. 

Fortunately, OpenGL (and DirectX) helps you do this. (Many years ago, you had to wait for the vertical blanking interrupt instead) 

There is an integer parameter used in flip() 

"-1" (Default). The screen is drawn using the display card default setting, this may be "do not wait for the next display refresh" or "wait for the next display refresh". 

"0" .The screen is drawn without waiting for the next display refresh 

"1" . The screen is drawn after the next display refresh 

"n" . The screen is drawn after the nth display refresh 

You must not assume that the application will wait for the next display refresh even if you asked for it. Some display drivers contain an option for the user to override the application setting. With NVidia it is the "Vertical Sync" option. 

If you want to test your application speed, use flip(0). For normal use, use flip(1) 

Do not use flip() to control the game cycle time 

CL_DisplayWindowDescription 

This contains: set_refresh_rate() 

It is not used by the OpenGL target. It used to in the past, but it had to be removed because it caused a number of problems. 

It is used by the SWRender target, to make it easier for the programmer to switch between ClanLib display targets. It calls CL_System::sleep() to give the application the appearance that it is being synchronized to the display refresh. If it is not defined, it defaults to 60hz. 

To recap, it is only used on clanSWRender when used with flip(1) 

Also this class contains: set_swap_interval() 

This controls the initial flip() swap interval setting, changing the default state of flip() 

I.E. It is only used when you call flip(-1) or flip(). 

This function is useful for controlling the display refresh when using clanGUI 

CL_System::sleep() 

This sleeps for a number of milliseconds, reducing cpu usage of your application. 

Internally, this function causes a thread to relinquish the remainder of its time slice. The system clock "ticks" at a constant rate. If the time is less than the resolution of the system clock, the thread may sleep for less than the specified length of time. If time is greater than one tick but less than two, the wait can be anywhere between one and two ticks, and so on 

If you specify 0 milliseconds, the thread will relinquish the remainder of its time slice, thus it will not return immediately. 

Note, on the windows platform, you can adjust the accuracy of sleep() using timeBeginPeriod - http://msdn.microsoft.com/en-us/libr...24(VS.85).aspx 

ClanLib currently does not do this. 

CL_System::pause() 

This is like CL_System::sleep(), except it may perform a spinlock to pause the application. This was required because sometimes on the windows platform sleep() may not return at exact intervals. 

Avoid using this, unless you require more exact timing. Other applications on the computer may run slower. 

CL_KeepAlive::process 

This processes the operating system messages (keyboard, mouse etc) 

The parameter to this function is the time out (in milliseconds) 

For games, you always want to keep this at 0. 

If you set it to -1 (Wait unless message is found), your game main loop will only operate when the mouse is moved (an interesting but useless effect). 

Main Loop Timing 

The ideal code will be: 

Code: ( See Examples/Display/Timing ) 


	unsigned int last_time = CL_System::get_time();
 
	while (!quit)
	{
		unsigned int current_time = CL_System::get_time();
		int time_difference = current_time - last_time;
		if (time_difference > 1000)
			time_difference = 1000;		// Limit the time difference, if the application was paused (eg, moving the window on WIN32)
		float time_delta_ms = static_cast<float> (time_difference);
		last_time = current_time;
 
		*** GAME CODE HERE 
		gc.clear(CL_Colorf(0.0f,0.0f,0.2f));
 
		sprite_position += speed * time_delta_ms;
		*** END CODE ENDS
 
		window.flip(1);
 
		CL_KeepAlive::process(0);
 
		current_time = CL_System::get_time();
		const int main_loop_rate = 10;	// 10 ms (100 hz)
		int time_to_sleep_for = main_loop_rate - (current_time - last_time);
		if (time_to_sleep_for > 0)
			CL_System::sleep(time_to_sleep_for);
 
	}



*/
