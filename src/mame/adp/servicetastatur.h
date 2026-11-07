// license:BSD-3-Clause
// copyright-holders:stonedDiscord
/*

ADP Profitech 3000 Servicetastatur

The service keyboard is connected to the control unit ("Steuereinheit 68000",
connector P6 "Service - Test Gerät") through the GSG link:

    DataST    (machine -> keyboard) shared with the 1MA data line
    nTaktST   (machine -> keyboard) shift clock, one edge per write to the data latch
    EnST      (machine -> keyboard) shared with EnANZ1, latches the 16 bit frame
    OutST     (keyboard -> machine) read back as bit 3 of the input port

This file provides the slot (adp_gsg_port_device) the machine plugs into and the
interface a card has to implement; the keyboard itself is adp_servicet_device.

*/

#ifndef MAME_ADP_SERVICETASTATUR_H
#define MAME_ADP_SERVICETASTATUR_H

#pragma once

#include "cpu/mcs51/i80c51.h"
#include "machine/i2cmem.h"
#include "video/hd44780.h"


class adp_gsg_port_device;

// ======================> device_adp_gsg_interface

class device_adp_gsg_interface : public device_interface
{
	friend class adp_gsg_port_device;

public:
	virtual ~device_adp_gsg_interface();

protected:
	device_adp_gsg_interface(const machine_config &mconfig, device_t &device);

	virtual void data_in_w(int state) = 0;
	virtual void clock_w(int state) = 0;
	virtual void enable_w(int state) = 0;
	virtual int data_out_r() = 0;
};


// ======================> adp_gsg_port_device

class adp_gsg_port_device : public device_t, public device_slot_interface
{
public:
	template <typename T>
	adp_gsg_port_device(const machine_config &mconfig, const char *tag, device_t *owner, T &&opts, const char *dflt) :
		adp_gsg_port_device(mconfig, tag, owner, 0)
	{
		option_reset();
		opts(*this);
		set_default_option(dflt);
		set_fixed(false);
	}

	adp_gsg_port_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	// signals from the machine; the clock is a single rising edge
	void data_w(int state);
	void clock_pulse();
	void enable_w(int state);

	// OutST, 0 when nothing is connected
	int data_r();

protected:
	virtual void device_config_complete() override;
	virtual void device_start() override ATTR_COLD;

private:
	device_adp_gsg_interface *m_dev;
};

DECLARE_DEVICE_TYPE(ADP_GSG_PORT, adp_gsg_port_device)

void adp_gsg_devices(device_slot_interface &device);


// ======================> adp_servicet_device

class adp_servicet_device : public device_t, public device_adp_gsg_interface
{
public:
	adp_servicet_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 11'059'200);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;

	// device_adp_gsg_interface implementation
	virtual void data_in_w(int state) override;
	virtual void clock_w(int state) override;
	virtual void enable_w(int state) override;
	virtual int data_out_r() override;

private:
	uint8_t port1_r();
	void port1_w(uint8_t data);
	uint8_t port3_r();
	void port3_w(uint8_t data);
	uint8_t bus_r(offs_t offset);
	void bus_w(offs_t offset, uint8_t data);
	uint8_t gsg_r(offs_t offset);
	void gsg_w(uint8_t data);

	void servicet_map(address_map &map) ATTR_COLD;
	void servicet_data(address_map &map) ATTR_COLD;

	required_device<mcs51_cpu_device> m_maincpu;
	required_device<i2cmem_device> m_i2cmem;
	required_device<hd44780_device> m_lcd;
	required_ioport_array<3> m_io_keys;

	uint8_t m_port1;
	uint8_t m_port3;
	uint8_t m_lcd_data;

	// GSG link
	int m_data_in;       // DataST level
	int m_clock;         // previous nTaktST level
	int m_enable;        // previous EnST level
	uint16_t m_rx_shift; // the two chained 74HC4094, first bit in the high byte
	uint16_t m_rx_latch; // their parallel outputs after the enable strobe
	uint8_t m_tx_data;   // 573 in front of the 74HC165, written by the CPU
	uint8_t m_tx_shift;  // 74HC165, shifts out MSB first
};

DECLARE_DEVICE_TYPE(ADP_SERVICET, adp_servicet_device)

#endif // MAME_ADP_SERVICETASTATUR_H
