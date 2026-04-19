// license:BSD-3-Clause
// copyright-holders:stonedDiscord, Styne13
/*

ADP Profitech 3000 Servicetastatur Device

This is the service keyboard used to configure and initialize
ADP/Stella fruit machines. It connects via the GSG protocol.

*/

#ifndef MAME_ADP_SERVICETASTATUR_H
#define MAME_ADP_SERVICETASTATUR_H

#pragma once

#include "cpu/mcs51/i80c51.h"
#include "machine/i2cmem.h"
#include "video/hd44780.h"

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> adp_servicet_device

class adp_servicet_device : public device_t
{
public:
	// construction/destruction
	adp_servicet_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// GSG protocol interface - active signals from/to the main machine
	void data_in_w(int state);
	int data_out_r();
	void enable_w(int state);
	void clock_w(int state);

protected:
	// device-level overrides
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;

private:
	// device callbacks
	uint8_t port1_r();
	void port1_w(uint8_t data);
	uint8_t port3_r();
	void port3_w(uint8_t data);
	uint8_t bus_r(offs_t offset);
	void bus_w(offs_t offset, uint8_t data);

	// address maps
	void servicet_data(address_map &map) ATTR_COLD;
	void servicet_map(address_map &map) ATTR_COLD;

	// sub-devices
	required_device<mcs51_cpu_device> m_maincpu;
	required_device<i2cmem_device> m_i2cmem;
	required_device<hd44780_device> m_lcd;
	required_ioport_array<3> m_io_keys;

	// internal state
	uint8_t m_port1;
	uint8_t m_port3;
	uint8_t m_lcd_data;

	// GSG protocol state
	int m_gsg_data_in;       // current data-in bit from machine
	int m_gsg_data_out;      // current data-out bit to machine
	int m_gsg_clock;         // previous clock state (for edge detection)
	int m_gsg_enable;        // previous enable state (for edge detection)
	uint8_t m_gsg_rx_shift;  // shift register for receiving data from machine
	uint8_t m_gsg_tx_shift;  // shift register for sending data to machine
	uint8_t m_gsg_rx_data;   // latched received byte (available to CPU)
	uint8_t m_gsg_tx_data;   // byte to send (loaded by CPU)
};

// device type declaration
DECLARE_DEVICE_TYPE(ADP_SERVICET, adp_servicet_device)

#endif // MAME_ADP_SERVICETASTATUR_H
