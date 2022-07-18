// license:BSD-3-Clause
// copyright-holders:Anthony Campbell, Trammel Hudson, Mario Montminy
#ifndef VECTOR_VECTRX2020_H
#define VECTOR_VECTRX2020_H
#pragma once

#include "video/vector.h"
#include "divector.h"
#include "emuopts.h"

class vector_vectrx2020_device;

 

class vector_vectrx2020_options
{
public:
    friend class vector_vectrx2020_device;
    static float s_vector_scale;
    static float s_vector_scale_x;
    static float s_vector_scale_y;
    static float s_vector_offset_x;
    static float s_vector_offset_y;
    static char *s_vector_port;

    static int s_vector_rotate;
    static int s_vector_bright;

protected:
    static void init(emu_options &options);
};

class vector_vectrx2020_device : public vector_device
{
public:
    vector_vectrx2020_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

    virtual uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect) override;
    virtual void clear_list() override;
	virtual void add_point(int x, int y, rgb_t color, int intensity) override;
    virtual void add_line(float xf0, float yf0, float xf1, float yf1, int intensity) override;

private:
    // Serial output option for driving vector display hardware
    osd_file::ptr m_serial;
    size_t m_serial_offset;
    std::unique_ptr<uint8_t[]> m_serial_buf;
    unsigned m_vector_transit[3];
    int m_serial_drop_frame;
    int m_serial_sort;
    serial_segment_t *m_serial_segments;
    serial_segment_t *m_serial_segments_tail;
    int serial_send();
    void serial_reset();
    int serial_read(uint8_t *buf, int size);
    std::error_condition serial_write(uint8_t *buf, int size);
    void serial_draw_point(unsigned x, unsigned y, int intensity);

protected:
    virtual void device_start() override;
};

// device type definition
DECLARE_DEVICE_TYPE(VECTOR_VECTRX2020, vector_vectrx2020_device)
#endif // VECTOR_VECTRX2020_H
