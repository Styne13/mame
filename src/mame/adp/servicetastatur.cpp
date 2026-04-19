// license:BSD-3-Clause
// copyright-holders:stonedDiscord
/*

ADP
Profitech 3000 Servicetastatur

Hardware:
- CPU: 80C52 @ 11.0592MHz
- LCD: LCD4002A
- Memory: 27C256 EPROM (32KB), 24CS16 I2C EEPROM (2KB)

Key Matrix Layout:
Col 0 (P1.0): OK, F4, UP
Col 1 (P1.1): RIGHT, LEFT, DOWN
Col 2 (P1.2): F3, F1, F2

GSG pinout to machine:
GND
Data Out
Enable
Data Clock
Data In
5V

Output is done by 74HC165.
Input is done by the 2 74HC4094.
D7 is connected to QP0 and D0 to QP7.
U19 has D1-D3 reversed from this.

    _____________________________________
   | 11.059     24CS16           TL7705  |
___| XTAL  80C31          +KEYPAD+       |__
|74HC00                               +    |
|          74HC165 74HC4094           G    |
|27C128                               S    |
|74LS573   74HC238 74HC4094           G    |
|___   +DISPLAY+  MC34063             + ___|
   |___________________________________|
*/

#include "emu.h"
#include "cpu/mcs51/i80c51.h"
#include "machine/i2cmem.h"
#include "video/hd44780.h"
#include "emupal.h"
#include "screen.h"
#include "servicetastatur.h"

#include "servicet.lh"

namespace {

enum
{
	PORT_1_COL0,
	PORT_1_COL1,
	PORT_1_COL2,
	PORT_1_NC3,
	PORT_1_ROW0,
	PORT_1_ROW1,
	PORT_1_ROW2,
	PORT_1_NC7
};

enum
{
	PORT_3_RXD,
	PORT_3_TXD,
	PORT_3_INT0,
	PORT_3_INT1,
	PORT_3_SDA,
	PORT_3_SCL,
	PORT_3_WR,
	PORT_3_RD
};

class servicet_state : public driver_device
{
public:
	servicet_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_kbd(*this, "kbd")
	{ }

	void servicet(machine_config &config) ATTR_COLD;

protected:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;

private:
	required_device<adp_servicet_device> m_kbd;
};

void servicet_state::machine_start()
{
	// Device handles everything
}

void servicet_state::machine_reset()
{
	// Device handles everything
}

void servicet_state::servicet(machine_config &config)
{
	// Instantiate the service keyboard device which handles all functionality
	ADP_SERVICET(config, "kbd", 11.0592_MHz_XTAL);
}

static INPUT_PORTS_START( servicet )
	// Input ports are provided by the kbd device
INPUT_PORTS_END

ROM_START( servicet )
	ROM_REGION( 0x8000, "maincpu", 0 )
	ROM_LOAD( "service_tastatur_v3.3.u3", 0x0000, 0x8000, CRC(8eb161c4) SHA1(d44f3b38e75e1095487893d8b30c4e3212c1a143) )

	ROM_REGION(0x800, "eeprom", ROMREGION_ERASEFF)
ROM_END

} // anonymous namespace

//  Device type definition and implementations

DEFINE_DEVICE_TYPE(ADP_SERVICET, adp_servicet_device, "adp_servicet", "ADP Service Keyboard")

adp_servicet_device::adp_servicet_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, ADP_SERVICET, tag, owner, clock),
	m_maincpu(*this, "maincpu"),
	m_i2cmem(*this, "eeprom"),
	m_lcd(*this, "hd44780"),
	m_io_keys(*this, "IN%u", 0U),
	m_port1(0xff),
	m_port3(0xff),
	m_lcd_data(0),
	m_gsg_data_in(0),
	m_gsg_data_out(0),
	m_gsg_clock(0),
	m_gsg_enable(0),
	m_gsg_rx_shift(0),
	m_gsg_tx_shift(0),
	m_gsg_rx_data(0),
	m_gsg_tx_data(0)
{
}

void adp_servicet_device::device_start()
{
	save_item(NAME(m_port1));
	save_item(NAME(m_port3));
	save_item(NAME(m_lcd_data));
	save_item(NAME(m_gsg_data_in));
	save_item(NAME(m_gsg_data_out));
	save_item(NAME(m_gsg_clock));
	save_item(NAME(m_gsg_enable));
	save_item(NAME(m_gsg_rx_shift));
	save_item(NAME(m_gsg_tx_shift));
	save_item(NAME(m_gsg_rx_data));
	save_item(NAME(m_gsg_tx_data));
}

void adp_servicet_device::device_reset()
{
	m_port1 = 0xff;
	m_port3 = 0xff;
	m_lcd_data = 0;
	m_gsg_data_in = 0;
	m_gsg_data_out = 0;
	m_gsg_clock = 0;
	m_gsg_enable = 0;
	m_gsg_rx_shift = 0;
	m_gsg_tx_shift = 0;
	m_gsg_rx_data = 0;
	m_gsg_tx_data = 0;
}

//-------------------------------------------------
//  GSG protocol handlers
//-------------------------------------------------

void adp_servicet_device::data_in_w(int state)
{
	m_gsg_data_in = state;
}

int adp_servicet_device::data_out_r()
{
	return m_gsg_data_out;
}

void adp_servicet_device::clock_w(int state)
{
	if (state && !m_gsg_clock) // rising edge
	{
		// Shift data in from machine (74HC4094 serial-in)
		m_gsg_rx_shift = (m_gsg_rx_shift << 1) | (m_gsg_data_in & 1);

		// Shift data out to machine (74HC165 serial-out), MSB first
		m_gsg_data_out = BIT(m_gsg_tx_shift, 7);
		m_gsg_tx_shift <<= 1;

		logerror("GSG CLK rise: DI=%d DO=%d rx_shift=%02X tx_shift=%02X\n",
			m_gsg_data_in, m_gsg_data_out, m_gsg_rx_shift, m_gsg_tx_shift);
	}
	m_gsg_clock = state;
}

void adp_servicet_device::enable_w(int state)
{
	if (state && !m_gsg_enable) // rising edge
	{
		// Latch the received shift register data for the CPU to read
		m_gsg_rx_data = m_gsg_rx_shift;
		m_gsg_rx_shift = 0;

		// Load next byte to shift out from what the CPU wrote
		m_gsg_tx_shift = m_gsg_tx_data;

		logerror("GSG ENABLE rise: rx_data=%02X tx_data=%02X\n",
			m_gsg_rx_data, m_gsg_tx_data);
	}
	m_gsg_enable = state;
}

//-------------------------------------------------
//  80C31 port handlers
//-------------------------------------------------

uint8_t adp_servicet_device::port1_r()
{
	uint8_t data = m_port1;

	for (int col = 0; col < 3; col++)
	{
		if (BIT(m_port1, col))
		{
			uint8_t const keys = m_io_keys[col]->read();
			data |= (keys & 0x70);
		}
	}

	for (int row = 0; row < 3; row++)
	{
		if (BIT(m_port1, row + 4))
		{
			for (int col = 0; col < 3; col++)
			{
				uint8_t const keys = m_io_keys[col]->read();
				if (BIT(keys, row + 4))
				{
					data |= (1 << col);
				}
			}
		}
	}

	return data;
}

void adp_servicet_device::port1_w(uint8_t data)
{
	m_port1 = data;
}

uint8_t adp_servicet_device::port3_r()
{
	uint8_t data = m_port3;

	uint8_t const sda = m_i2cmem->read_sda();
	data = (data & ~(1 << 4)) | (sda ? (1 << 4) : 0);

	return data;
}

void adp_servicet_device::port3_w(uint8_t data)
{
	m_port3 = data;
	m_i2cmem->write_sda(BIT(data, 4));
	m_i2cmem->write_scl(BIT(data, 5));
}

//-------------------------------------------------
//  External data bus handlers
//-------------------------------------------------

uint8_t adp_servicet_device::bus_r(offs_t offset)
{
	uint8_t data = 0xff;

	if ((offset & 0x70) == 0x70)
	{
		bool rs = BIT(offset, 1);
		bool rw = BIT(offset, 0);

		if (rw)
		{
			m_lcd->rs_w(rs);
			m_lcd->rw_w(1);
			m_lcd->e_w(1);
			data = m_lcd->db_r();
			m_lcd->e_w(0);
		}
		else
		{
			data = m_lcd_data;
		}
	}
	else if (offset == 0x4000)
	{
		// GSG read - data received from machine
		data = m_gsg_rx_data;
		logerror("GSG bus read: %02X\n", data);
	}

	return data;
}

void adp_servicet_device::bus_w(offs_t offset, uint8_t data)
{
	if ((offset & 0x70) == 0x70)
	{
		bool const rs = BIT(offset, 1);
		bool const rw = BIT(offset, 0);

		if (!rw)
		{
			m_lcd_data = data;
			m_lcd->rs_w(rs);
			m_lcd->rw_w(0);
			m_lcd->db_w(data);
			m_lcd->e_w(1);
			m_lcd->e_w(0);
		}
	}
	else if (offset == 0x4000)
	{
		// GSG write - data to send to machine
		m_gsg_tx_data = data;
		logerror("GSG bus write: %02X\n", data);
	}
}

//-------------------------------------------------
//  Address maps
//-------------------------------------------------

void adp_servicet_device::servicet_map(address_map &map)
{
	map(0x0000, 0x7fff).rom();
}

void adp_servicet_device::servicet_data(address_map &map)
{
	map(0x0000, 0xffff).rw(FUNC(adp_servicet_device::bus_r), FUNC(adp_servicet_device::bus_w));
}

//-------------------------------------------------
//  Input ports
//-------------------------------------------------

static INPUT_PORTS_START( adp_servicet )
	PORT_START("IN0") // P1.0
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_ENTER)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_F4)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP)    PORT_4WAY

	PORT_START("IN1") // P1.1
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT) PORT_4WAY
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT)  PORT_4WAY
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN)  PORT_4WAY

	PORT_START("IN2") // P1.2
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_F3)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_F2)
INPUT_PORTS_END

ioport_constructor adp_servicet_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(adp_servicet);
}

//-------------------------------------------------
//  ROM region
//-------------------------------------------------

ROM_START( adp_servicet )
	ROM_REGION( 0x8000, "maincpu", 0 )
	ROM_LOAD( "service_tastatur_v3.3.u3", 0x0000, 0x8000, CRC(8eb161c4) SHA1(d44f3b38e75e1095487893d8b30c4e3212c1a143) )

	ROM_REGION(0x800, "eeprom", ROMREGION_ERASEFF)
ROM_END

const tiny_rom_entry *adp_servicet_device::device_rom_region() const
{
	return ROM_NAME(adp_servicet);
}

//-------------------------------------------------
//  Machine configuration
//-------------------------------------------------

void adp_servicet_device::device_add_mconfig(machine_config &config)
{
	I80C31(config, m_maincpu, DERIVED_CLOCK(1, 1));
	m_maincpu->set_addrmap(AS_PROGRAM, &adp_servicet_device::servicet_map);
	m_maincpu->set_addrmap(AS_DATA, &adp_servicet_device::servicet_data);

	m_maincpu->port_in_cb<1>().set(FUNC(adp_servicet_device::port1_r));
	m_maincpu->port_out_cb<1>().set(FUNC(adp_servicet_device::port1_w));
	m_maincpu->port_in_cb<3>().set(FUNC(adp_servicet_device::port3_r));
	m_maincpu->port_out_cb<3>().set(FUNC(adp_servicet_device::port3_w));

	I2C_24C16(config, m_i2cmem);

	screen_device &screen(SCREEN(config, "screen", SCREEN_TYPE_LCD));
	screen.set_color(rgb_t(6, 120, 245));
	screen.set_physical_aspect(7*40, 10*2);
	screen.set_refresh_hz(72);
	screen.set_size(6*40, 9*2);
	screen.set_visarea_full();
	screen.set_screen_update("hd44780", FUNC(hd44780_device::screen_update));
	screen.set_palette("palette");

	PALETTE(config, "palette", palette_device::MONOCHROME_INVERTED);

	HD44780(config, m_lcd, 270'000);
	m_lcd->set_lcd_size(2, 40);
}

GAMEL( 1992, servicet, 0, servicet, servicet, servicet_state, empty_init, ROT0, "ADP", u8"Merkur Service Testgerät", MACHINE_NOT_WORKING | MACHINE_NO_SOUND_HW, layout_servicet )
