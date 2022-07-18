// license:BSD-3-Clause
// copyright-holders:Mario Montminy, Anthony Campbell
#ifndef DIVECTOR_H
#define DIVECTOR_H

#pragma once

#include "osdcore.h"
#include "screen.h"
#include "device.h"
#include "divideo.h"
#include "emu.h"

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************


class vector_interface : public device_t, public device_video_interface
{
public:
	// construction/destruction
	vector_interface(const machine_config& mconfig, device_type type, const char* tag, device_t* owner, uint32_t clock = 0);
	 
	virtual void add_line(float xf0, float yf0, float xf1, float yf1, int intensity);
	virtual void add_point(int x, int y, rgb_t color, int intensity);
	virtual uint32_t screen_update(screen_device& screen, bitmap_rgb32& bitmap, const rectangle& cliprect);
	 

protected:
	int m_vector_index;
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_stop() override;
	virtual void device_add_mconfig(machine_config& config) override;
	// optional operation overrides
 

};

// iterator
typedef device_interface_enumerator<vector_interface> vector_device_enumerator;

#endif // DIVECTOR_H
