// license:BSD-3-Clause
// copyright-holders:hap
/**********************************************************************

    Videopac C7010 Chess Module emulation

**********************************************************************/

#ifndef MAME_BUS_ODYSSEY2_CHESS_H
#define MAME_BUS_ODYSSEY2_CHESS_H

#pragma once

#include "slot.h"
#include "cpu/z80/z80.h"
#include "machine/gen_latch.h"


// ======================> o2_chess_device

class o2_chess_device : public device_t,
						public device_o2_cart_interface
{
public:
	// construction/destruction
	o2_chess_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_add_mconfig(machine_config &config) override;

	virtual void cart_init() override;

	virtual u8 read_rom04(offs_t offset) override { return m_rom[offset + 0x2000]; }
	virtual u8 read_rom0c(offs_t offset) override { return m_rom[offset + 0x2000]; }

	virtual void write_p1(u8 data) override;
	virtual void io_write(offs_t offset, u8 data) override;
	virtual u8 io_read(offs_t offset) override;

private:
	required_device<nsc800_device> m_cpu;
	required_device_array<generic_latch_8_device, 2> m_latch;

	u8 internal_rom_r(offs_t offset) { return m_rom[offset]; }

	void chess_io(address_map &map);
	void chess_mem(address_map &map);

	u8 m_control = 0;
};

// device type definition
DECLARE_DEVICE_TYPE(O2_ROM_CHESS, o2_chess_device)

#endif // MAME_BUS_ODYSSEY2_CHESS_H
