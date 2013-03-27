/*
**  ClanLib SDK
**  Copyright (c) 1997-2012 The ClanLib Team
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
**  Note: Some of the libraries ClanLib may link to may have additional
**  requirements or restrictions.
**
**  File Author(s):
**
**    Magnus Norddahl
**    Harry Storbacka
**    Kevin J Bluck
**    Mark Page
*/

#include "GL/precomp.h"
#include "opengl_window_provider_wgl.h"
#include "API/Core/Math/rect.h"
#include "API/Display/Window/display_window_description.h"
#include "API/Display/display.h"
#include "API/Display/display_target.h"
#include "API/Display/TargetProviders/display_window_provider.h"
#include "API/Display/Window/display_window.h"
#include "API/Display/Render/shared_gc_data.h"
#include "API/GL/opengl.h"
#include "API/GL/opengl_wrap.h"
#include "API/GL/opengl_window_description.h"
#include "API/Core/Text/logger.h"
#include "Display/Win32/cursor_provider_win32.h"
#include "Display/Win32/dwm_functions.h"
#include "../opengl_window_description_impl.h"
#include "../GL3/gl3_graphic_context_provider.h"
#include "../GL1/gl1_graphic_context_provider.h"
#include "opengl_creation_helper.h"
#include <commctrl.h>

namespace clan
{

/////////////////////////////////////////////////////////////////////////////
// OpenGLWindowProvider Construction:

OpenGLWindowProvider::OpenGLWindowProvider(OpenGLWindowDescription &opengl_desc)
: win32_window(),
  opengl_context(0), device_context(0), hwnd(0), shadow_window(false), dwm_layered(false), site(0), fullscreen(false),
  wglSwapIntervalEXT(0), swap_interval(-1), opengl_desc(opengl_desc)
{
	win32_window.func_on_resized().set(this, &OpenGLWindowProvider::on_window_resized);
}

OpenGLWindowProvider::~OpenGLWindowProvider()
{
	if (opengl_context)
	{
		if (!gc.is_null())
		{
			GL3GraphicContextProvider *gl_provider = dynamic_cast<GL3GraphicContextProvider*>(gc.get_provider());
			if (gl_provider)
				gl_provider->dispose();
		}

		// Delete the context
		if (wglGetCurrentContext() == opengl_context)
			OpenGL::set_active(0);
		wglDeleteContext(opengl_context);

		opengl_context = 0;
	}
	if (device_context)
	{
		ReleaseDC(win32_window.get_hwnd(), device_context);
		device_context = 0;
		hwnd = 0;
	}
	if (shadow_window)
	{
		if (hwnd)
			DestroyWindow(hwnd);
		shadow_window = false;
	}

	hwnd = 0;
}

/////////////////////////////////////////////////////////////////////////////
// OpenGLWindowProvider Attributes:

// std::list<HGLRC> OpenGLWindowProvider::opengl_contexts;

Rect OpenGLWindowProvider::get_geometry() const
{
	return win32_window.get_geometry();
}

Rect OpenGLWindowProvider::get_viewport() const
{
	return win32_window.get_viewport();
}

bool OpenGLWindowProvider::is_fullscreen() const
{
	return fullscreen;
}

bool OpenGLWindowProvider::has_focus() const
{
	return win32_window.has_focus();
}

bool OpenGLWindowProvider::is_minimized() const
{
	return win32_window.is_minimized();
}

bool OpenGLWindowProvider::is_maximized() const
{
	return win32_window.is_maximized();
}

bool OpenGLWindowProvider::is_visible() const
{
	return win32_window.is_visible();
}

Size OpenGLWindowProvider::get_minimum_size(bool client_area) const
{
	return win32_window.get_minimum_size(client_area);
}

Size OpenGLWindowProvider::get_maximum_size(bool client_area) const
{
	return win32_window.get_maximum_size(client_area);
}

std::string OpenGLWindowProvider::get_title() const
{
	return win32_window.get_title();
}

bool OpenGLWindowProvider::is_clipboard_text_available() const
{
	return win32_window.is_clipboard_text_available();
}

bool OpenGLWindowProvider::is_clipboard_image_available() const
{
	return win32_window.is_clipboard_image_available();
}

/////////////////////////////////////////////////////////////////////////////
// OpenGLWindowProvider Operations:

void OpenGLWindowProvider::make_current() const
{
	wglMakeCurrent(device_context, opengl_context);
}

Point OpenGLWindowProvider::client_to_screen(const Point &client)
{
	return win32_window.client_to_screen(client);
}

Point OpenGLWindowProvider::screen_to_client(const Point &screen)
{
	return win32_window.screen_to_client(screen);
}

void OpenGLWindowProvider::create_shadow_window(HWND wnd)
{
	WINDOWINFO window_info;
	memset(&window_info, 0, sizeof(WINDOWINFO));
	window_info.cbSize = sizeof(WINDOWINFO);
	GetWindowInfo(wnd, &window_info);
	if (window_info.dwStyle & WS_VISIBLE)
		window_info.dwStyle -= WS_VISIBLE;

	hwnd = CreateWindowEx(
		0, //window_info.dwExStyle,
		WC_STATIC, //(LPCTSTR) window_info.atomWindowType,
		TEXT(""),
		window_info.dwStyle,
		0, // window_info.rcWindow.left,
		0, // window_info.rcWindow.top,
		window_info.rcWindow.right - window_info.rcWindow.left,
		window_info.rcWindow.bottom - window_info.rcWindow.top,
		GetParent(wnd), 0, GetModuleHandle(0), 0);
	if (hwnd == 0)
		throw Exception("Unable to create display window (opengl offscreen window)");

	shadow_window = true;
}

void OpenGLWindowProvider::create(DisplayWindowSite *new_site, const DisplayWindowDescription &desc)
{
	site = new_site;
	fullscreen = desc.is_fullscreen();

	win32_window.create(site, desc);

	if (!opengl_context)
	{
		hwnd = win32_window.get_hwnd();
		dwm_layered = false;

		if (desc.is_layered() && !DwmFunctions::is_composition_enabled())
		{
			create_shadow_window(hwnd);
		}
		else 
		{
			if (desc.is_layered())
				dwm_layered = true;
		}
		device_context = GetDC(hwnd);

		HGLRC share_context = get_share_context();

		OpenGLCreationHelper helper(hwnd, device_context);
		helper.set_multisampling_pixel_format(desc);

		int gl_major = opengl_desc.get_version_major();
		int gl_minor = opengl_desc.get_version_minor();
		if (opengl_desc.get_allow_lower_versions() == false)
		{
			opengl_context = helper.create_opengl3_context(share_context, gl_major, gl_minor, opengl_desc);
			if (!opengl_context)
				throw Exception(string_format("This application requires OpenGL %1.%2 or above. Try updating your drivers, or upgrade to a newer graphics card.",  gl_major, gl_minor));
		}
		else
		{
			static const char opengl_version_list[] = 
			{
				// Clanlib supported version pairs
				4,3,
				4,2,
				4,1,
				4,0,
				3,3,
				3,2,
				3,1,
				3,0,
				0,0,	// End of list
			};

			const char *opengl_version_list_ptr = opengl_version_list;
			do
			{
				int major = *(opengl_version_list_ptr++);
				if (major == 0)
					break;
					
				int minor = *(opengl_version_list_ptr++);

				// Find the appropriate version in the list
				if (major > gl_major)
					continue;

				if (major == gl_major)
				{
					if (minor > gl_minor)
						continue;	
				}

				opengl_context = helper.create_opengl3_context(share_context, major, minor, opengl_desc);
			}while(!opengl_context);

			if (!opengl_context)
				opengl_context = helper.create_opengl2_context(share_context);

			if (!opengl_context)
				throw Exception("This application requires OpenGL. Try updating your drivers, or upgrade to a newer graphics card.");

		}

		bool use_gl3;
		int desc_version_major = opengl_desc.get_version_major();
		int desc_version_minor = opengl_desc.get_version_minor();

		// Do not attempt GL3, if not requested that version
		if ((desc_version_major < 3) || ((desc_version_major == 3) && (desc_version_minor < 2)))
		{
			use_gl3 = false;
		}
		else if (!opengl_desc.get_allow_lower_versions())	// If we do not allow lower versions, only attempt GL3
		{
			use_gl3 = true;
		}
		else
		{
			// Choose the target depending on the current opengl version
			int gl_version_major;
			int gl_version_minor;
			get_opengl_version(gl_version_major, gl_version_minor);
			if ((gl_version_major < 3) || ((gl_version_major == 3) && (gl_version_minor < 2)))
			{
				use_gl3 = false;
			}
			else
			{
				use_gl3 = true;
			}

		}

		if (use_gl3)
		{
			gc = GraphicContext(new GL3GraphicContextProvider(this));
		}
		else
		{
			gc = GraphicContext(new GL1GraphicContextProvider(this));
		}
	}

	wglSwapIntervalEXT = (ptr_wglSwapIntervalEXT)OpenGL::get_proc_address("wglSwapIntervalEXT");
	swap_interval = desc.get_swap_interval();
	if (wglSwapIntervalEXT && swap_interval != -1)
		wglSwapIntervalEXT(swap_interval);
}

void OpenGLWindowProvider::get_opengl_version(int &version_major, int &version_minor)
{
	make_current();
	std::string version = (char*)glGetString(GL_VERSION);

	version_major = 0;
	version_minor = 0;
    
	std::vector<std::string> split_version = StringHelp::split_text(version, ".");
	if(split_version.size() > 0)
		version_major = StringHelp::text_to_int(split_version[0]);
	if(split_version.size() > 1)
		version_minor = StringHelp::text_to_int(split_version[1]);
}

void OpenGLWindowProvider::on_window_resized()
{
	if (shadow_window)
	{
		RECT rect = { 0,0,0,0 };
		GetClientRect(win32_window.get_hwnd(), &rect);
		SetWindowPos(hwnd, 0, 0, 0, rect.right, rect.bottom, SWP_NOREPOSITION|SWP_NOZORDER);
	}

	if (gc.get_provider())
		((GL3GraphicContextProvider *) gc.get_provider())->on_window_resized();
}

void OpenGLWindowProvider::show_system_cursor()
{
	win32_window.show_system_cursor();
}

CursorProvider *OpenGLWindowProvider::create_cursor(const CursorDescription &cursor_description, const Point &hotspot)
{
	return new CursorProvider_Win32(cursor_description, hotspot);
}

void OpenGLWindowProvider::set_cursor(CursorProvider *cursor)
{
	win32_window.set_cursor(static_cast<CursorProvider_Win32 *>(cursor));
}

void OpenGLWindowProvider::set_cursor(StandardCursor type)
{
	win32_window.set_cursor(type);
}

void OpenGLWindowProvider::set_cursor_handle(HCURSOR cursor)
{
	win32_window.set_cursor_handle(cursor);
}

void OpenGLWindowProvider::hide_system_cursor()
{
	win32_window.hide_system_cursor();
}

void OpenGLWindowProvider::set_title(const std::string &new_title)
{
	win32_window.set_title(new_title);
}

void OpenGLWindowProvider::set_position(const Rect &pos, bool client_area)
{
	win32_window.set_position(pos, client_area);
}

void OpenGLWindowProvider::set_size(int width, int height, bool client_area)
{
	win32_window.set_size(width, height, client_area);
}

void OpenGLWindowProvider::set_minimum_size( int width, int height, bool client_area )
{
	win32_window.set_minimum_size(width,height,client_area);
}

void OpenGLWindowProvider::set_maximum_size( int width, int height, bool client_area )
{
	win32_window.set_maximum_size(width,height,client_area);
}

void OpenGLWindowProvider::set_enabled(bool enable)
{
	win32_window.set_enabled(enable);
}

void OpenGLWindowProvider::minimize()
{
	win32_window.minimize();
}

void OpenGLWindowProvider::restore()
{
	win32_window.restore();
}

void OpenGLWindowProvider::maximize()
{
	win32_window.maximize();
}

void OpenGLWindowProvider::show(bool activate)
{
	win32_window.show(activate);
}

void OpenGLWindowProvider::hide()
{
	win32_window.hide();
}

void OpenGLWindowProvider::bring_to_front()
{
	win32_window.bring_to_front();
}



void OpenGLWindowProvider::flip(int interval)
{
	OpenGL::set_active(get_gc());
	glFlush();

	if (shadow_window)
	{
		int width = get_viewport().get_width();
		int height = get_viewport().get_height();

		//glReadBuffer(GL_BACK);
		glDrawBuffer(GL_BACK);
		glReadBuffer(GL_FRONT);

		PixelBuffer pixelbuffer(width, height, tf_rgba8);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glPixelStorei(GL_PACK_ROW_LENGTH, pixelbuffer.get_pitch() / pixelbuffer.get_bytes_per_pixel());
		glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
		glPixelStorei(GL_PACK_SKIP_ROWS, 0);
		glReadPixels(
			0, 0,
			width, height,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			pixelbuffer.get_data());

		win32_window.update_layered(pixelbuffer);
	}
	else
	{
		if (interval != -1 && interval != swap_interval)
		{
			swap_interval = interval;
			if (wglSwapIntervalEXT)
				wglSwapIntervalEXT(swap_interval);
		}

		BOOL retval = SwapBuffers(get_device_context());

		if (dwm_layered)
		{
			int width = get_viewport().get_width();
			int height = get_viewport().get_height();

			glReadBuffer(GL_FRONT);

			PixelBuffer pixelbuffer(width, height, tf_r8);
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			glPixelStorei(GL_PACK_ROW_LENGTH, pixelbuffer.get_pitch() / pixelbuffer.get_bytes_per_pixel());
			glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
			glPixelStorei(GL_PACK_SKIP_ROWS, 0);
			glReadPixels(
				0, 0,
				width, height,
				GL_ALPHA,
				GL_BYTE, // use GL_BITMAP here for even less transfer?
				pixelbuffer.get_data());

			win32_window.update_layered(pixelbuffer);
		}
	}
}

void OpenGLWindowProvider::update(const Rect &_rect)
{
	int width = get_viewport().get_width();
	int height = get_viewport().get_height();

	Rect rect = _rect;
	if (rect.left < 0)
		rect.left = 0;
	if (rect.top < 0)
		rect.top = 0;
	if (rect.right > width)
		rect.right = width;
	if (rect.bottom > height)
		rect.bottom = height;
	if (rect.right <= rect.left || rect.bottom <= rect.top)
		return;

	OpenGL::set_active(gc);
	glFlush();

	if (shadow_window)
	{
		//glReadBuffer(GL_BACK);
		glDrawBuffer(GL_BACK);
		glReadBuffer(GL_FRONT);

		// ** Currently update layered windows only supports full screen rect update **
		rect = Rect(0,0, width, height);

		PixelBuffer pixelbuffer(rect.get_width(), rect.get_height(), tf_rgba8);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glPixelStorei(GL_PACK_ROW_LENGTH, pixelbuffer.get_pitch() / pixelbuffer.get_bytes_per_pixel());
		glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
		glPixelStorei(GL_PACK_SKIP_ROWS, 0);
		glReadPixels(
			rect.left, height - rect.bottom,
			rect.right - rect.left, rect.bottom - rect.top,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			pixelbuffer.get_data());

		win32_window.update_layered(pixelbuffer);
	}
	else
	{
		GLboolean isdoublebuffered = GL_TRUE;
		glGetBooleanv(GL_DOUBLEBUFFER, &isdoublebuffered);
		if (isdoublebuffered)
		{

			GLint read_last_bound;
			GLint draw_last_bound;

			glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_last_bound);
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_last_bound);

			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

			glReadBuffer(GL_BACK);
			glDrawBuffer(GL_FRONT);

			glBlitFramebuffer( 
				rect.left, height - rect.bottom,
				rect.right, height - rect.top,
				rect.left, height - rect.bottom,
				rect.right, height - rect.top,
				GL_COLOR_BUFFER_BIT, GL_NEAREST);

			glDrawBuffer(GL_BACK);
			glReadBuffer(GL_FRONT);

			if (read_last_bound)
				glBindFramebuffer(GL_READ_FRAMEBUFFER, read_last_bound);

			if (draw_last_bound)
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_last_bound);

			glFlush();

		}

		if (dwm_layered)
		{
			glDrawBuffer(GL_BACK);
			glReadBuffer(GL_FRONT);

			// ** Currently update layered windows only supports full screen rect update **
			rect = Rect(0,0, width, height);

			PixelBuffer pixelbuffer(rect.get_width(), rect.get_height(), tf_r8);
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			glPixelStorei(GL_PACK_ROW_LENGTH, pixelbuffer.get_pitch() / pixelbuffer.get_bytes_per_pixel());
			glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
			glPixelStorei(GL_PACK_SKIP_ROWS, 0);
			glReadPixels(
				rect.left, height - rect.bottom,
				rect.right - rect.left, rect.bottom - rect.top,
				GL_ALPHA,
				GL_BYTE, // use GL_BITMAP here for even less transfer?
				pixelbuffer.get_data());

			win32_window.update_layered(pixelbuffer);
		}
	}
}

void OpenGLWindowProvider::capture_mouse(bool capture)
{
	win32_window.capture_mouse(capture);
}

void OpenGLWindowProvider::set_clipboard_text(const std::string &text)
{
	win32_window.set_clipboard_text(text);
}

std::string OpenGLWindowProvider::get_clipboard_text() const
{
	return win32_window.get_clipboard_text();
}

void OpenGLWindowProvider::request_repaint(const Rect &rect)
{
	win32_window.request_repaint(rect);
}

void OpenGLWindowProvider::set_large_icon(const PixelBuffer &image)
{
	win32_window.set_large_icon(image);
}

void OpenGLWindowProvider::set_small_icon(const PixelBuffer &image)
{
	win32_window.set_small_icon(image);
}

void OpenGLWindowProvider::enable_alpha_channel(const Rect &blur_rect)
{
	win32_window.enable_alpha_channel(blur_rect);
}

void OpenGLWindowProvider::extend_frame_into_client_area(int height)
{
	win32_window.extend_frame_into_client_area(height);
}


/////////////////////////////////////////////////////////////////////////////
// OpenGLWindowProvider Implementation:

HGLRC OpenGLWindowProvider::get_share_context()
{
	HGLRC share_context = 0;
	std::unique_ptr<MutexSection> mutex_section;
	GraphicContextProvider* gc_providers = SharedGCData::get_provider(mutex_section);
	if (gc_providers)
	{
		GL3GraphicContextProvider *gl_provider = dynamic_cast<GL3GraphicContextProvider*>(gc_providers);
		if (gl_provider)
		{
			const DisplayWindowProvider *rwp = &gl_provider->get_render_window();
			const OpenGLWindowProvider *render_window_wgl = dynamic_cast<const OpenGLWindowProvider*>(rwp);
			if (render_window_wgl)
				share_context = render_window_wgl->opengl_context;
		}
	}
	return share_context;
}

void OpenGLWindowProvider::set_clipboard_image( const PixelBuffer &buf )
{
	win32_window.set_clipboard_image(buf);
}

PixelBuffer OpenGLWindowProvider::get_clipboard_image() const
{
	return win32_window.get_clipboard_image();
}

}
