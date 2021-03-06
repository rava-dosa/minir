#include "io.h"
#include <string.h>

namespace {
class video_none : public video {
	uint32_t features() { return 0; }
	
	void set_source(unsigned int max_width, unsigned int max_height, videoformat depth) {}
	void draw_2d(unsigned int width, unsigned int height, const void * data, unsigned int pitch) {}
	void set_dest_size(unsigned int width, unsigned int height) {}
	~video_none() {}
};
video* video_create_none(uintptr_t windowhandle) { return new video_none(); }
};

const video::driver video::driver_none={ "None", video_create_none, NULL, 0 };


namespace {
class video_compat : public video {
	cvideo* child;
	
	uintptr_t windowhandle;
	
	videoformat depth;
	unsigned int base_width;
	unsigned int base_height;
	
	unsigned int last_width;
	unsigned int last_height;
	
public:
	
	uint32_t features()
	{
		uint32_t ret=0;
		if (child->has_sync(child)) ret|=f_vsync;
		return ret;
	}
	
	void set_source(unsigned int max_width, unsigned int max_height, videoformat depth)
	{
		this->depth=depth;
		child->reinit(child, this->base_width, this->base_height, this->depth, 60);
	}
	
	void set_dest_size(unsigned int width, unsigned int height)
	{
		this->base_width=width;
		this->base_height=height;
		child->reinit(child, this->base_width, this->base_height, this->depth, 60);
	}
	
	void draw_2d(unsigned int width, unsigned int height, const void * data, unsigned int pitch)
	{
		this->last_width=width;
		this->last_height=height;
		child->draw(child, width, height, data, pitch);
	}
	
	void draw_repeat()
	{
		child->draw(child, this->last_width, this->last_height, NULL, 0);
	}
	
	void set_vsync(double fps)
	{
		this->child->set_sync(this->child, fps!=0);
	}
	
	~video_compat() { child->free(child); }
	
	video_compat(cvideo* child)
	{
		this->child=child;
		this->depth=fmt_xrgb1555;
		this->base_width=256;
		this->base_height=256;
	}
};

}

video* video_create_compat(cvideo* child)
{
	if (!child) return NULL;
	return new video_compat(child);
}

const video::driver* const video::drivers[]={
#ifdef VIDEO_D3D9
	&driver_d3d9,
#endif
#ifdef VIDEO_DDRAW
	&driver_ddraw,
#endif
#ifdef VIDEO_OPENGL
	&driver_opengl,
#endif
#ifdef VIDEO_GDI
	&driver_gdi,
#endif
#ifdef VIDEO_XSHM
	&driver_xshm,
#endif
#ifdef VIDEO_OPENGL
	&driver_opengl_old,
#endif
	&driver_none,
	NULL
};

const char * video::shader::langtable[]={NULL, VIDEO_SHADER_LANG_NAMES};
