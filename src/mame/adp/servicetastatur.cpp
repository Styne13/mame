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
#include "servicetastatur.h"

#include "emupal.h"
#include "screen.h"

namespace {

// 80C31 port 1
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

// 80C31 port 3
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

private:
	required_device<adp_servicet_device> m_kbd;
};

void servicet_state::servicet(machine_config &config)
{
	// everything lives in the device so the same keyboard can be plugged into a machine
	ADP_SERVICET(config, m_kbd);
}

static INPUT_PORTS_START( servicet )
INPUT_PORTS_END

ROM_START( servicet )
ROM_END

} // anonymous namespace


//**************************************************************************
//  GSG slot
//**************************************************************************

DEFINE_DEVICE_TYPE(ADP_GSG_PORT, adp_gsg_port_device, "adp_gsg_port", "ADP GSG Port")

adp_gsg_port_device::adp_gsg_port_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, ADP_GSG_PORT, tag, owner, clock),
	device_slot_interface(mconfig, *this),
	m_dev(nullptr)
{
}

void adp_gsg_port_device::device_config_complete()
{
	m_dev = get_card_device() ? dynamic_cast<device_adp_gsg_interface *>(get_card_device()) : nullptr;
}

void adp_gsg_port_device::device_start()
{
}

void adp_gsg_port_device::data_w(int state)
{
	if (m_dev)
		m_dev->data_in_w(state);
}

void adp_gsg_port_device::clock_pulse()
{
	if (m_dev)
	{
		m_dev->clock_w(1);
		m_dev->clock_w(0);
	}
}

void adp_gsg_port_device::enable_w(int state)
{
	if (m_dev)
		m_dev->enable_w(state);
}

int adp_gsg_port_device::data_r()
{
	return m_dev ? m_dev->data_out_r() : 0;
}

device_adp_gsg_interface::device_adp_gsg_interface(const machine_config &mconfig, device_t &device) :
	device_interface(device, "adpgsg")
{
}

device_adp_gsg_interface::~device_adp_gsg_interface()
{
}

void adp_gsg_devices(device_slot_interface &device)
{
	device.option_add("servicet", ADP_SERVICET).clock(11'059'200);
}


//**************************************************************************
//  Service keyboard
//**************************************************************************

DEFINE_DEVICE_TYPE(ADP_SERVICET, adp_servicet_device, "adp_servicet", "ADP Profitech 3000 Service Keyboard")

adp_servicet_device::adp_servicet_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, ADP_SERVICET, tag, owner, clock),
	device_adp_gsg_interface(mconfig, *this),
	m_maincpu(*this, "maincpu"),
	m_i2cmem(*this, "eeprom"),
	m_lcd(*this, "hd44780"),
	m_io_keys(*this, "IN%u", 0U),
	m_port1(0xff),
	m_port3(0xff),
	m_lcd_data(0),
	m_data_in(0),
	m_clock(0),
	m_enable(0),
	m_rx_shift(0),
	m_rx_latch(0),
	m_tx_data(0),
	m_tx_shift(0)
{
}

void adp_servicet_device::device_start()
{
	save_item(NAME(m_port1));
	save_item(NAME(m_port3));
	save_item(NAME(m_lcd_data));
	save_item(NAME(m_data_in));
	save_item(NAME(m_clock));
	save_item(NAME(m_enable));
	save_item(NAME(m_rx_shift));
	save_item(NAME(m_rx_latch));
	save_item(NAME(m_tx_data));
	save_item(NAME(m_tx_shift));
}

void adp_servicet_device::device_reset()
{
	m_port1 = 0xff;
	m_port3 = 0xff;
	m_lcd_data = 0;
	m_data_in = 0;
	m_clock = 0;
	m_enable = 0;
	m_rx_shift = 0;
	m_rx_latch = 0;
	m_tx_data = 0;
	m_tx_shift = 0;
}


//-------------------------------------------------
//  GSG link
//-------------------------------------------------

void adp_servicet_device::data_in_w(int state)
{
	m_data_in = state;
}

// two chained 74HC4094 take the frame from the machine: the first bit ends up
// in the last chip, and the keyboard reads D0 = QP7 / D7 = QP0 of each chip
void adp_servicet_device::clock_w(int state)
{
	if (state && !m_clock)
	{
		m_rx_shift = (m_rx_shift << 1) | (m_data_in & 1);

		// 74HC165 shifts the byte the CPU left in the latch out to the machine
		m_tx_shift <<= 1;
	}
	m_clock = state;
}

// EnST stays high while the machine exchanges its 16 bit frame. The 74HC165
// takes the byte the CPU left in the latch when it rises, the 74HC4094s
// freeze their outputs when it falls, and that is also when INT1 fires.
void adp_servicet_device::enable_w(int state)
{
	if (state && !m_enable)
		m_tx_shift = m_tx_data;

	if (!state && m_enable)
	{
		m_rx_latch = m_rx_shift;

		m_maincpu->set_input_line(MCS51_INT1_LINE, ASSERT_LINE);
		m_maincpu->set_input_line(MCS51_INT1_LINE, CLEAR_LINE);
	}
	m_enable = state;
}

int adp_servicet_device::data_out_r()
{
	return BIT(m_tx_shift, 7);
}


//-------------------------------------------------
//  80C31 port handlers
//-------------------------------------------------

uint8_t adp_servicet_device::port1_r()
{
	uint8_t data = m_port1; // start with what was written to port 1

	// key matrix scanning is bidirectional: the CPU drives lines high and checks
	// if connected lines follow

	// columns (P1.0-P1.2) driven high
	for (int col = 0; col < 3; col++)
	{
		if (BIT(m_port1, col))
			data |= m_io_keys[col]->read() & 0x70; // row bits (4,5,6)
	}

	// rows (P1.4-P1.6) driven high
	for (int row = 0; row < 3; row++)
	{
		if (BIT(m_port1, row + 4))
		{
			for (int col = 0; col < 3; col++)
			{
				if (BIT(m_io_keys[col]->read(), row + 4))
					data |= 1 << col;
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
	// SDA comes from the EEPROM
	return (m_port3 & ~(1 << PORT_3_SDA)) | (m_i2cmem->read_sda() ? (1 << PORT_3_SDA) : 0);
}

void adp_servicet_device::port3_w(uint8_t data)
{
	m_port3 = data;

	m_i2cmem->write_sda(BIT(data, PORT_3_SDA));
	m_i2cmem->write_scl(BIT(data, PORT_3_SCL));
}


//-------------------------------------------------
//  external data bus
//-------------------------------------------------

uint8_t adp_servicet_device::gsg_r(offs_t offset)
{
	// the chips are read bit reversed (D0 = QP7), 0x40 holds the byte sent first
	return bitswap<8>(BIT(offset, 4) ? (m_rx_latch & 0xff) : (m_rx_latch >> 8), 0, 1, 2, 3, 4, 5, 6, 7);
}

void adp_servicet_device::gsg_w(uint8_t data)
{
	m_tx_data = data;
}

uint8_t adp_servicet_device::bus_r(offs_t offset)
{
	uint8_t data = 0xff;

	switch (offset & 0x70)
	{
	case 0x40:
	case 0x50:
		data = gsg_r(offset);
		break;

	case 0x70:
		// LCD: RS = A1, RW = A0
		if (BIT(offset, 0))
		{
			m_lcd->rs_w(BIT(offset, 1));
			m_lcd->rw_w(1);
			m_lcd->e_w(1);
			data = m_lcd->db_r();
			m_lcd->e_w(0);
		}
		else
		{
			data = m_lcd_data;
		}
		break;
	}

	return data;
}

void adp_servicet_device::bus_w(offs_t offset, uint8_t data)
{
	switch (offset & 0x70)
	{
	case 0x60:
		gsg_w(data);
		break;

	case 0x70:
		if (!BIT(offset, 0))
		{
			m_lcd_data = data;
			m_lcd->rs_w(BIT(offset, 1));
			m_lcd->rw_w(0);
			m_lcd->db_w(data);
			m_lcd->e_w(1);
			m_lcd->e_w(0);
		}
		break;
	}
}

void adp_servicet_device::servicet_map(address_map &map)
{
	map(0x0000, 0x7fff).rom();
}

void adp_servicet_device::servicet_data(address_map &map)
{
	map(0x0000, 0xffff).rw(FUNC(adp_servicet_device::bus_r), FUNC(adp_servicet_device::bus_w));
}

static INPUT_PORTS_START( adp_servicet )
	PORT_START("IN0") // P1.0
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("OK") PORT_CODE(KEYCODE_ENTER)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("F4") PORT_CODE(KEYCODE_F4)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP)    PORT_4WAY

	PORT_START("IN1") // P1.1
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT) PORT_4WAY
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT)  PORT_4WAY
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN)  PORT_4WAY

	PORT_START("IN2") // P1.2
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("F3") PORT_CODE(KEYCODE_F3)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("F1") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("F2") PORT_CODE(KEYCODE_F2)
INPUT_PORTS_END

ioport_constructor adp_servicet_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(adp_servicet);
}

ROM_START( adp_servicet )
	ROM_REGION( 0x8000, "maincpu", 0 )
	ROM_LOAD( "service_tastatur_v3.3.u3", 0x0000, 0x8000, CRC(8eb161c4) SHA1(d44f3b38e75e1095487893d8b30c4e3212c1a143) )

	ROM_REGION( 0x800, "eeprom", ROMREGION_ERASEFF )
ROM_END

const tiny_rom_entry *adp_servicet_device::device_rom_region() const
{
	return ROM_NAME(adp_servicet);
}

void adp_servicet_device::device_add_mconfig(machine_config &config)
{
	I80C31(config, m_maincpu, DERIVED_CLOCK(1, 1));
	m_maincpu->set_addrmap(AS_PROGRAM, &adp_servicet_device::servicet_map);
	m_maincpu->set_addrmap(AS_DATA, &adp_servicet_device::servicet_data);

	m_maincpu->port_in_cb<1>().set(FUNC(adp_servicet_device::port1_r));
	m_maincpu->port_out_cb<1>().set(FUNC(adp_servicet_device::port1_w));
	m_maincpu->port_in_cb<3>().set(FUNC(adp_servicet_device::port3_r));
	m_maincpu->port_out_cb<3>().set(FUNC(adp_servicet_device::port3_w));

	// 24C16 on P3.4 (SDA) and P3.5 (SCL)
	I2C_24C16(config, m_i2cmem);

	// LCD4002A
	screen_device &screen(SCREEN(config, "screen").set_lcd());
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

GAME( 1992, servicet, 0, servicet, servicet, servicet_state, empty_init, ROT0, "ADP", u8"Merkur Service Testgerät", MACHINE_NOT_WORKING | MACHINE_NO_SOUND_HW )
