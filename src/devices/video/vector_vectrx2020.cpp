#include "emu.h"
#include "video/vector.h"
#include "video/vector_vectrx2020.h"
#include "emuopts.h"

#define VECTOR_SERIAL_MAX 4095
#define MAX_POINTS 20000
DEFINE_DEVICE_TYPE(VECTOR_VECTRX2020, vector_vectrx2020_device, "vector_vectrx2020_device", "VECTOR_VECTRX2020")


 float vector_vectrx2020_options::s_vector_scale=0.0f;
 float vector_vectrx2020_options::s_vector_scale_x=0.0f;
 float vector_vectrx2020_options::s_vector_scale_y=0.0f;
 float vector_vectrx2020_options::s_vector_offset_x=0.0f;
 float vector_vectrx2020_options::s_vector_offset_y =0.0f;
 char * vector_vectrx2020_options::s_vector_port;
 int vector_vectrx2020_options::s_vector_rotate=0;
 int vector_vectrx2020_options::s_vector_bright=0;

void vector_vectrx2020_options::init(emu_options &options)
{
	const float scale = options.vector_scale();
	if (scale != 0.0)
	{	// user specified a scale on the command line
		vector_vectrx2020_options::s_vector_scale_x = vector_vectrx2020_options::s_vector_scale_y = scale;
	}
	else
	{	// use the per-axis scales
        vector_vectrx2020_options::s_vector_scale_x = options.vector_scale_x();
        vector_vectrx2020_options::s_vector_scale_y = options.vector_scale_y();
	}
    vector_vectrx2020_options::s_vector_offset_x = options.vector_offset_x();
    vector_vectrx2020_options::s_vector_offset_y = options.vector_offset_y();
    vector_vectrx2020_options::s_vector_port = const_cast<char *>(options.vector_port());
    vector_vectrx2020_options::s_vector_bright = options.vector_bright();
    vector_vectrx2020_options::s_vector_rotate = options.vector_rotate();
};

vector_vectrx2020_device::vector_vectrx2020_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) : vector_device(mconfig, tag, owner, clock){};

void vector_vectrx2020_device::serial_reset()
{
	m_serial_offset = 0;
	m_serial_buf[m_serial_offset++] = 0;
	m_serial_buf[m_serial_offset++] = 0;
	m_serial_buf[m_serial_offset++] = 0;
	m_serial_buf[m_serial_offset++] = 0;
	m_serial_buf[m_serial_offset++] = 0;
	m_serial_buf[m_serial_offset++] = 0;
	m_serial_buf[m_serial_offset++] = 0;
	m_serial_buf[m_serial_offset++] = 0;

	m_vector_transit[0] = 0;
	m_vector_transit[1] = 0;
	m_vector_transit[2] = 0;
};

// This will only be called with non-zero intensity lines.
// we keep a linked list of the vectors and sort them with
// a greedy insertion sort.

void vector_vectrx2020_device::add_line(float xf0, float yf0, float xf1, float yf1, int intensity)
{
	if (m_serial == nullptr)
		return;

	// scale and shift each of the axes.
	const int x0 = (xf0 * VECTOR_SERIAL_MAX - VECTOR_SERIAL_MAX / 2) * vector_vectrx2020_options::s_vector_scale_x + vector_vectrx2020_options::s_vector_offset_x;
	const int y0 = (yf0 * VECTOR_SERIAL_MAX - VECTOR_SERIAL_MAX / 2) * vector_vectrx2020_options::s_vector_scale_y + vector_vectrx2020_options::s_vector_offset_y;
	const int x1 = (xf1 * VECTOR_SERIAL_MAX - VECTOR_SERIAL_MAX / 2) * vector_vectrx2020_options::s_vector_scale_x + vector_vectrx2020_options::s_vector_offset_x;
	const int y1 = (yf1 * VECTOR_SERIAL_MAX - VECTOR_SERIAL_MAX / 2) * vector_vectrx2020_options::s_vector_scale_y + vector_vectrx2020_options::s_vector_offset_y;

	serial_segment_t *const new_segment = new serial_segment_t(x0, y0, x1, y1, intensity);

	if (this->m_serial_segments_tail)
        this->m_serial_segments_tail->next = new_segment;
	else
        this->m_serial_segments = new_segment;

    this->m_serial_segments_tail = new_segment;
};

std::error_condition vector_vectrx2020_device::serial_write(uint8_t *buf, int size)
{
	std::error_condition result;
	uint32_t written = 0, chunk;

	while (size)
	{
		chunk = std::min(size, 64);
		result = this->m_serial->write(buf, 0, chunk, written);
		if (written != chunk)
		{
			goto END;
		}
		buf += chunk;
		size -= chunk;
	}

END:
	return result;
};
int vector_vectrx2020_device::serial_send()
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
		serial_segment_t **min_seg = &m_serial_segments;

		if (m_serial_sort)
			for (serial_segment_t **s = min_seg; *s; s = &(*s)->next)
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

		serial_segment_t *const s = *min_seg;
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

		if (s->intensity > vector_vectrx2020_options::s_vector_bright)
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


	if (1==0)
		printf("%zu vectors: off=%u on=%u bright=%u%s\n",
			   m_serial_offset / 4,
			   m_vector_transit[0],
			   m_vector_transit[1],
			   m_vector_transit[2],
			   m_serial_drop_frame ? " !" : "");

	static unsigned skip_frame;

	std::error_condition err;
	if (m_serial_drop_frame || skip_frame++ % 2 != 0)
	{
		// we skipped a frame, don't skip the next one
		m_serial_drop_frame = 0;
		return 1;
	}
	else
		err = serial_write(&m_serial_buf[0], m_serial_offset);

	serial_reset();

	return err.value();
};

uint32_t vector_vectrx2020_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	vector_device::screen_update(screen, bitmap, cliprect);
	if (vector_device::m_vector_index == 0)
	{
		return 0;
	}
	serial_send();
	return 0;
};
void vector_vectrx2020_device::clear_list()
{
	vector_device::clear_list();
};
void vector_vectrx2020_device::add_point(int x, int y, rgb_t color, int intensity)
{
	// printf("%d %d: %d,%d,%d @ %d\n", x, y, color.r(), color.b(), color.g(), intensity);

	// hack for the vectrex
	// -- convert "128,128,128" @ 255 to "255,255,255" @ 127
	if (color.r() == 128 && color.b() == 128 && color.g() == 128 && intensity == 255)
	{
		color = rgb_t(255, 255, 255);
		intensity = 128;
	}

	vector_device::add_point(x, y, color, intensity);
};

void vector_vectrx2020_device::serial_draw_point(unsigned x, unsigned y, int intensity)
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
	if (intensity > vector_vectrx2020_options::s_vector_bright)
		bright = 63;
	else if (intensity <= 0)
		bright = 0;
	else
		bright = (intensity * 64) / 256;

	if (bright > 63)
		bright = 63;

	if (vector_vectrx2020_options::s_vector_rotate == 1)
	{
		// +90
		unsigned tmp = x;
		x = VECTOR_SERIAL_MAX - y;
		y = tmp;
	}
	else if (vector_vectrx2020_options::s_vector_rotate == 2)
	{
		// +180
		x = VECTOR_SERIAL_MAX - x;
		y = VECTOR_SERIAL_MAX - y;
	}
	else if (vector_vectrx2020_options::s_vector_rotate == 3)
	{
		// -90
		unsigned t = x;
		x = y;
		y = VECTOR_SERIAL_MAX - t;
	}

	uint32_t cmd = 0 | (2 << 30) | (bright & 0x3F) << 24 | (x & 0xFFF) << 12 | (y & 0xFFF) << 0;

	// printf("%08x %8d %8d %3d\n", cmd, x, y, intensity);

	m_serial_buf[m_serial_offset++] = cmd >> 24;
	m_serial_buf[m_serial_offset++] = cmd >> 16;
	m_serial_buf[m_serial_offset++] = cmd >> 8;
	m_serial_buf[m_serial_offset++] = cmd >> 0;

	// todo: check for overflow;
	// should always have enough points
};
// device-level overrides
void vector_vectrx2020_device::device_start()
{
	vector_device::device_start();
	uint64_t size = 0;
    vector_vectrx2020_options::init(machine().options());
	std::error_condition filerr = osd_file::open(vector_vectrx2020_options::s_vector_port, OPEN_FLAG_READ | OPEN_FLAG_WRITE, m_serial, size);

	if (filerr)
	{
		fprintf(stderr, "vector_device_vectrx2020: error: osd_file::open failed: %s on port %s\n", const_cast<char *>(filerr.message().c_str()), vector_vectrx2020_options::s_vector_port);
		::exit(1);
	}
	m_serial_segments = m_serial_segments_tail = NULL;
	m_serial_drop_frame = 0;
	m_serial_sort = 1;
	m_serial_buf = make_unique_clear<unsigned char[]>((MAX_POINTS + 2) * 4);

	serial_reset();
};
