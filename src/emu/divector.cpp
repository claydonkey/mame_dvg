// license:BSD-3-Clause
// copyright-holders: Anthony Campbell
#include "emu.h"
#include "emuopts.h"
#include "divector.h"


#define VERBOSE 0
#include "logmacro.h"

//-------------------------------------------------
//  ~device_video_interface - destructor
//-------------------------------------------------

 
vector_interface::vector_interface(const machine_config& mconfig, device_type type, const char* tag, device_t* owner, uint32_t clock)
	: device_t(mconfig, type, tag, owner, clock), device_video_interface(mconfig, *this) {
	 m_vector_index = 0;
}

void vector_interface::add_point(int x, int y, rgb_t color, int intensity) {};
void vector_interface::add_line(float xf0, float yf0, float xf1, float yf1, int intensity) {};

uint32_t vector_interface::screen_update(screen_device& screen, bitmap_rgb32& bitmap, const rectangle& cliprect)
{
	return 0;
};



void vector_interface::device_add_mconfig(machine_config& config)
{
	device_t::device_add_mconfig(config);
};
void vector_interface::device_stop() {};
void vector_interface::device_start() {};
void vector_interface::device_reset() {};

