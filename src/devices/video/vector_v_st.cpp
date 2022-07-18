// license:BSD-3-Clause
// copyright-holders: Anthony Campbell, TH
#include "emu.h"
#include "emuopts.h"
#include "rendutil.h"
#include "video/vector.h"
#include "video/vector_v_st.h"
#include <stdint.h>
#include <fcntl.h>
#include "emu.h"

#if defined(__MINGW32__) || defined(_WIN32) 
#if defined TERMIOS
#include <termiWin.h>
#endif
#elif defined TERMIOS
#include <termios.h>
#include <unistd.h>
#endif
#define VERBOSE 0
#define MAX_POINTS 20000
#define VECTOR_SERIAL_MAX 4095
#define CHUNK_SIZE 64
#include "logmacro.h"

DEFINE_DEVICE_TYPE(VECTOR_V_ST, vector_device_v_st, "vector_v_st", "VECTOR_V_ST")


float vector_device_v_st_options::s_vector_scale = 0.0f;
float vector_device_v_st_options::s_vector_scale_x = 0.0f;
float vector_device_v_st_options::s_vector_scale_y = 0.0f;
float vector_device_v_st_options::s_vector_offset_x = 0.0f;
float vector_device_v_st_options::s_vector_offset_y = 0.0f;
char* vector_device_v_st_options::s_vector_port;
int vector_device_v_st_options::s_vector_rotate = 0;
int vector_device_v_st_options::s_vector_bright = 0;

void vector_device_v_st_options::init(emu_options& options)
{
	const float scale = options.vector_scale();
	if (scale != 0.0)
	{	// user specified a scale on the command line
		vector_device_v_st_options::s_vector_scale_x = vector_device_v_st_options::s_vector_scale_y = scale;
	}
	else
	{	// use the per-axis scales
		vector_device_v_st_options::s_vector_scale_x = options.vector_scale_x();
		vector_device_v_st_options::s_vector_scale_y = options.vector_scale_y();
	}
	vector_device_v_st_options::s_vector_offset_x = options.vector_offset_x();
	vector_device_v_st_options::s_vector_offset_y = options.vector_offset_y();
	vector_device_v_st_options::s_vector_port = const_cast<char*>(options.vector_port());
	vector_device_v_st_options::s_vector_bright = options.vector_bright();
	vector_device_v_st_options::s_vector_rotate = options.vector_rotate();
};
class vector_device_v_st;


vector_device_v_st::vector_device_v_st(const machine_config& mconfig, const char* tag, device_t* owner, uint32_t clock): vector_interface(mconfig, VECTOR_V_ST, tag, owner, clock)  {}

void vector_device_v_st::serial_reset()
{
	this->m_serial_offset = 0;
	this->m_serial_buf[this->m_serial_offset++] = 0;
	this->m_serial_buf[this->m_serial_offset++] = 0;
	this->m_serial_buf[this->m_serial_offset++] = 0;
	this->m_serial_buf[this->m_serial_offset++] = 0;
	this->m_serial_buf[this->m_serial_offset++] = 0;
	this->m_serial_buf[this->m_serial_offset++] = 0;
	this->m_serial_buf[this->m_serial_offset++] = 0;
	this->m_serial_buf[this->m_serial_offset++] = 0;

	this->m_vector_transit[0] = 0;
	this->m_vector_transit[1] = 0;
	this->m_vector_transit[2] = 0;
}

void vector_device_v_st::add_line(float xf0, float yf0, float xf1, float yf1, int intensity)
{
	if (m_serial == nullptr)
		return;

	// scale and shift each of the axes.
	const int x0 = std::clamp(static_cast<int>((xf0 * VECTOR_SERIAL_MAX - VECTOR_SERIAL_MAX / 2) * vector_device_v_st_options::s_vector_scale_x + vector_device_v_st_options::s_vector_offset_x),0, VECTOR_SERIAL_MAX);
	const int y0 = std::clamp(static_cast<int>((yf0 * VECTOR_SERIAL_MAX - VECTOR_SERIAL_MAX / 2) * vector_device_v_st_options::s_vector_scale_y + vector_device_v_st_options::s_vector_offset_y), 0, VECTOR_SERIAL_MAX);
	const int x1 = std::clamp(static_cast<int>((xf1 * VECTOR_SERIAL_MAX - VECTOR_SERIAL_MAX / 2) * vector_device_v_st_options::s_vector_scale_x + vector_device_v_st_options::s_vector_offset_x), 0, VECTOR_SERIAL_MAX);
	const int y1 = std::clamp(static_cast<int>((yf1 * VECTOR_SERIAL_MAX - VECTOR_SERIAL_MAX / 2) * vector_device_v_st_options::s_vector_scale_y + vector_device_v_st_options::s_vector_offset_y), 0, VECTOR_SERIAL_MAX);

	serial_segment_t* const new_segment = new serial_segment_t(x0, y0, x1, y1, intensity);

	if (this->m_serial_segments_tail)
		this->m_serial_segments_tail->next = new_segment;
	else
		this->m_serial_segments = new_segment;

	this->m_serial_segments_tail = new_segment;
}
int vector_device_v_st::serial_read(uint8_t *buf, int size)
{
    return 0;
}
#ifdef TERMIOS
int vector_device_v_st::serial_open(const char* const dev)
{
	const int fd = open(dev, O_RDWR);
	if (fd < 0)
		return -1;

	// Disable modem control signals
	struct termios attr;
	tcgetattr(fd, &attr);
	attr.c_cflag |= CLOCAL | CREAD;
	attr.c_oflag &= ~OPOST;
	tcsetattr(fd, TCSANOW, &attr);

	return fd;
}
void vector_device_v_st::device_start() {
	/* Grab the settings for this session */
	vector_device_v_st_options::init(machine().options());
	this->m_serial_segments = m_serial_segments_tail = NULL;

	this->m_serial_drop_frame = 0;
	this->m_serial_sort = 1;
	this->m_serial = vector_device_v_st_options::s_vector_port;
	// allocate enough buffer space, although we should never use this much
	this->m_serial_buf = make_unique_clear<unsigned char[]>((MAX_POINTS + 2) * 4);


	serial_reset();

	if (!this->m_serial || strcmp(this->m_serial, "") == 0)
	{
		fprintf(stderr, "no serial vector display configured\n");
		this->m_serial_fd = -1;
	}
	else {
		this->m_serial_fd = serial_open(this->m_serial);
		fprintf(stderr, "serial dev='%s' fd=%d\n", this->m_serial, this->m_serial_fd);
	}
}
int vector_device_v_st::serial_send() {
	if (m_serial_fd < 0)
		return -1;

	int last_x = -1;
	int last_y = -1;

	// find the next closest point to the last one.
	// greedy sorting algorithm reduces beam transit time
	// fairly significantly. doesn't matter for the
	// vectorscope, but makes a big difference for Vectrex
	// and other slower displays.
	while (this->m_serial_segments)
	{
		int reverse = 0;
		int min = 1e6;
		serial_segment_t** min_seg
			= &this->m_serial_segments;

		if (this->m_serial_sort)
			for (serial_segment_t** s = min_seg; *s; s = &(*s)->next)
			{
				int dx0 = (*s)->x0 - last_x;
				int dy0 = (*s)->y0 - last_y;
				int dx1 = (*s)->x1 - last_x;
				int dy1 = (*s)->y1 - last_y;
				int d0 = sqrt(dx0 * dx0 + dy0 * dy0);
				int d1 = sqrt(dx1 * dx1 + dy1 * dy1);

				if (d0 < min)
				{
					min_seg = s;
					min = d0;
					reverse = 0;
				}

				if (d1 < min)
				{
					min_seg = s;
					min = d1;
					reverse = 1;
				}

				// if we have hit two identical points,
				// then stop the search here.
				if (min == 0)
					break;
			}

		serial_segment_t* const s = *min_seg;
		if (!s)
			break;

		const int x0 = reverse ? s->x1 : s->x0;
		const int y0 = reverse ? s->y1 : s->y0;
		const int x1 = reverse ? s->x0 : s->x1;
		const int y1 = reverse ? s->y0 : s->y1;

		
		// if this is not a continuous segment,
		// we must add a transit command
		if (last_x != x0 || last_y != y0)
		{
			serial_draw_point(x0, y0, 0);
			int dx = x0 - last_x;
			int dy = y0 - last_y;
			this->m_vector_transit[0] += sqrt(dx * dx + dy * dy);
		}

		// transit to the new point
		int dx = x1 - x0;
		int dy = y1 - y0;
		int dist = sqrt(dx * dx + dy * dy);

		serial_draw_point(x1, y1, s->intensity);
		last_x = x1;
		last_y = y1;

		if (s->intensity > vector_device_v_st_options::s_vector_bright)
			this->m_vector_transit[2] += dist;
		else
			this->m_vector_transit[1] += dist;

		// delete this segment from the list
		*min_seg = s->next;
		delete s;
	}

	// ensure that we erase our tracks
	if (this->m_serial_segments != NULL)
		fprintf(stderr, "errr?\n");
	this->m_serial_segments = NULL;
	this->m_serial_segments_tail = NULL;

	// add the "done" command to the message
	this->m_serial_buf[this->m_serial_offset++] = 1;
	this->m_serial_buf[this->m_serial_offset++] = 1;
	this->m_serial_buf[this->m_serial_offset++] = 1;
	this->m_serial_buf[this->m_serial_offset++] = 1;

	size_t offset = 0;
#ifdef MAME_DEBUG
	if (1 == 0) {
		printf("%zu vectors: off=%u on=%u bright=%u%s\n",
			this->m_serial_offset / 4,
			this->m_vector_transit[0],
			this->m_vector_transit[1],
			this->m_vector_transit[2],
			this->m_serial_drop_frame ? " !" : "");
	}
		
#endif
	static unsigned skip_frame=0;
	unsigned eagain = 0;

	if (this->m_serial_drop_frame || skip_frame++ % 2 != 0)
	{
		// we skipped a frame, don't skip the next one
		this->m_serial_drop_frame = 0;
	}
	else
		while (offset < this->m_serial_offset)
		{
			size_t wlen = this->m_serial_offset - offset;
			if (wlen > CHUNK_SIZE)
				wlen = CHUNK_SIZE;

			ssize_t rc = write(this->m_serial_fd, this->m_serial_buf.get() + offset, this->m_serial_offset - offset);
			if (rc <= 0)
			{
				eagain++;
				if (errno == EAGAIN)
					continue;
				perror(m_serial);
				close(this->m_serial_fd);
				this->m_serial_fd = -1;
				break;
			}

			offset += rc;
		}
#ifdef MAME_DEBUG
	printf("%d eagain.\n", eagain);
#endif
	if (eagain > 20)
		this->m_serial_drop_frame = 1;

	serial_reset();
	return 0;
}

#else
void vector_device_v_st::device_start()
{
	uint64_t size = 0;
	vector_device_v_st_options::init(machine().options());

	m_serial_segments = m_serial_segments_tail = NULL;
	m_serial_drop_frame = 0;
	m_serial_sort = 1;
	m_serial_buf = make_unique_clear<unsigned char[]>((MAX_POINTS + 2) * 4);

	auto filerr = osd_file::open(vector_device_v_st_options::s_vector_port,   OPEN_FLAG_WRITE, m_serial, size);
	if (filerr)
	{
		fprintf(stderr, "vector_device_vectrx2020: error: osd_file::open failed: %s on port %s\n", const_cast<char*>(filerr.message().c_str()), vector_device_v_st_options::s_vector_port);
		::exit(1);
	}
	serial_reset();
}

std::error_condition vector_device_v_st::serial_write(uint8_t* buf, int size)
{
	std::error_condition result;
	uint32_t written = 0, chunk;

	while (size)
	{
		chunk = std::min(size, CHUNK_SIZE);
		result = m_serial->write(buf, 0, chunk, written);
		if (written != chunk)
		{
			goto END;
		}
		buf += chunk;
		size -= chunk;
	}

END:
	return result;
}

int vector_device_v_st::serial_send()
{
	if (m_serial == nullptr)
		return 1;

	int last_x = -1;
	int last_y = -1;

	// find the next closest point to the last one.
	// greedy sorting algorithm reduces beam transit time
	// fairly significantly. doesn't matter for the
	// vectorscope, but makes a big difference for Vectrex
	// and other slower displays.
	while (m_serial_segments)
	{
		int reverse = 0;
		int min = 1e6;
		serial_segment_t** min_seg = &m_serial_segments;

		if (m_serial_sort)
			for (serial_segment_t** s = min_seg; *s; s = &(*s)->next)
			{
				int dx0 = (*s)->x0 - last_x;
				int dy0 = (*s)->y0 - last_y;
				int dx1 = (*s)->x1 - last_x;
				int dy1 = (*s)->y1 - last_y;
				int d0 = sqrt(dx0 * dx0 + dy0 * dy0);
				int d1 = sqrt(dx1 * dx1 + dy1 * dy1);

				if (d0 < min)
				{
					min_seg = s;
					min = d0;
					reverse = 0;
				}

				if (d1 < min)
				{
					min_seg = s;
					min = d1;
					reverse = 1;
				}
				// if we have hit two identical points,
				// then stop the search here.
				if (min == 0)
					break;
			}

		serial_segment_t* const s = *min_seg;
		if (!s)
			break;

		const int x0 = reverse ? s->x1 : s->x0;
		const int y0 = reverse ? s->y1 : s->y0;
		const int x1 = reverse ? s->x0 : s->x1;
		const int y1 = reverse ? s->y0 : s->y1;

		// if this is not a continuous segment,
		// we must add a transit command
		if (last_x != x0 || last_y != y0)
		{
			serial_draw_point(x0, y0, 0);
			int dx = x0 - last_x;
			int dy = y0 - last_y;
			m_vector_transit[0] += sqrt(dx * dx + dy * dy);
		}

		// transit to the new point
		int dx = x1 - x0;
		int dy = y1 - y0;
		int dist = sqrt(dx * dx + dy * dy);

		serial_draw_point(x1, y1, s->intensity);
		last_x = x1;
		last_y = y1;

		if (s->intensity > vector_device_v_st_options::s_vector_bright)
			m_vector_transit[2] += dist;
		else
			m_vector_transit[1] += dist;

		// delete this segment from the list
		*min_seg = s->next;
		delete s;
	}

	// ensure that we erase our tracks
	if (m_serial_segments != NULL)
		fprintf(stderr, "errr?\n");
	m_serial_segments = NULL;
	m_serial_segments_tail = NULL;

	// add the "done" command to the message
	m_serial_buf[m_serial_offset++] = 1;
	m_serial_buf[m_serial_offset++] = 1;
	m_serial_buf[m_serial_offset++] = 1;
	m_serial_buf[m_serial_offset++] = 1;


#ifdef MAME_DEBUG
		printf("%zu vectors: off=%u on=%u bright=%u%s\n",
			m_serial_offset / 4,
			m_vector_transit[0],
			m_vector_transit[1],
			m_vector_transit[2],
			m_serial_drop_frame ? " !" : "");
#endif
	static unsigned skip_frame;

	std::error_condition err;
	if (m_serial_drop_frame || skip_frame++ % 2 != 0)
	{
		// we skipped a frame, don't skip the next one
		m_serial_drop_frame = 0;
		return 1;
	}
	else
	{
		err = serial_write(&m_serial_buf[0], m_serial_offset);
	}
	serial_reset();

	return err.value();
}
#endif


 
void vector_device_v_st::add_point(int x, int y, rgb_t color, int intensity)
{
	// printf("%d %d: %d,%d,%d @ %d\n", x, y, color.r(), color.b(), color.g(), intensity);

// hack for the vectrex
// -- convert "128,128,128" @ 255 to "255,255,255" @ 127
	if (color.r() == 128 && color.b() == 128 && color.g() == 128 && intensity == 255)
	{
		color = rgb_t(255, 255, 255);
		intensity = 128;
	}
}

void vector_device_v_st::serial_draw_point(unsigned x, unsigned y, int intensity)
{
	// make sure that we are in range; should always be
	// due to clipping on the window, but just in case
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	if (x > VECTOR_SERIAL_MAX)
		x = VECTOR_SERIAL_MAX;
	if (y > VECTOR_SERIAL_MAX)
		y = VECTOR_SERIAL_MAX;

	// always flip the Y, since the vectorscope measures
	// 0,0 at the bottom left corner, but this coord uses
	// the top left corner.
	y = VECTOR_SERIAL_MAX - y;

	unsigned bright;
	if (intensity > vector_device_v_st_options::s_vector_bright)
		bright = 63;
	else if (intensity <= 0)
		bright = 0;
	else
		bright = (intensity * 64) / 256;

	if (bright > 63)
		bright = 63;

	if (vector_device_v_st_options::s_vector_rotate == 1)
	{
		// +90
		unsigned tmp = x;
		x = VECTOR_SERIAL_MAX - y;
		y = tmp;
	}
	else if (vector_device_v_st_options::s_vector_rotate == 2)
	{
		// +180
		x = VECTOR_SERIAL_MAX - x;
		y = VECTOR_SERIAL_MAX - y;
	}
	else if (vector_device_v_st_options::s_vector_rotate == 3)
	{
		// -90
		unsigned t = x;
		x = y;
		y = VECTOR_SERIAL_MAX - t;
	}

	uint32_t cmd = 0 | (2 << 30) | (bright & 0x3F) << 24 | (x & 0xFFF) << 12 | (y & 0xFFF) << 0;

	// printf("%08x %8d %8d %3d\n", cmd, x, y, intensity);

	this->m_serial_buf[this->m_serial_offset++] = cmd >> 24;
	this->m_serial_buf[this->m_serial_offset++] = cmd >> 16;
	this->m_serial_buf[this->m_serial_offset++] = cmd >> 8;
	this->m_serial_buf[this->m_serial_offset++] = cmd >> 0;

	// todo: check for overflow;
	// should always have enough points
}


 
void vector_device_v_st::device_stop()
{

}

void vector_device_v_st::device_reset()
{

}


uint32_t vector_device_v_st::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	//if (vector_device_t::m_vector_index)
	//{
		serial_send();
	//	
	//}
	return 0;
}



