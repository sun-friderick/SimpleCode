/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 Nokia Corporation
   Original Author: Dmitry Kasatkin <dmitry.kasatkin@nokia.com>

   This code based on original code from David A. Hinds
   <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
   are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/* 
   $Id: btuart_cs.c,v 1.3 2004/02/24 16:08:28 kassatki Exp $

   btuart_cs - uart driver for pcmcia serial bluetooth cards
               based on serial_cs.c

   Fixes:	
   		Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>

/* Standard driver includes */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/major.h>
#include <linux/netdevice.h>
#include <asm/io.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/devfs_fs.h>
#include <asm/system.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ciscode.h>
#include <pcmcia/ds.h>
#include <pcmcia/cisreg.h>

#define FILEBIT	DBDRV
#include <affix/btdebug.h>
#include <affix/uart.h>


/*====================================================================*/
static dev_info_t	dev_info = "affix_uart_cs";
dev_link_t		*dev_list = NULL;


/* Parameters that can be set with 'insmod' */

/* Bit map of interrupts to choose from */
static u_int irq_mask = 0xdeb8;
//static u_int irq_mask = 0x0000;
static int irq_list[4] = { -1 };

/* Enable the speaker? */
static int do_sound = 1;

MODULE_PARM(irq_mask, "i");
MODULE_PARM(irq_list, "1-4i");
MODULE_PARM(do_sound, "i");


/* control structure for transport driver */
typedef struct {
	dev_link_t	link;
	dev_node_t	node;
	int		line;
	affix_uart_t	uart;
} btuart_cs_t;

void btuart_config(dev_link_t *link);
void btuart_remove(dev_link_t *link);
int btuart_event(event_t event, int priority, event_callback_args_t *args);

dev_link_t *btuart_attach(void);
void btuart_detach(dev_link_t *);

static struct pcmcia_driver btuart_driver = {
	.drv		= {
		.name	= "affix_uart_cs",
	},
	.attach		= btuart_attach,
	.detach		= btuart_detach,
	.owner		= THIS_MODULE,
};

/*======================================================================

    After a card is removed, btuart_release() will unregister the net
    device, and release the PCMCIA configuration.
    
======================================================================*/

void btuart_remove(dev_link_t *link)
{
	btuart_cs_t	*btuart = link->priv;
    
	DBFENTER;
	//DBPRT("irq?: %lu, int?: %lu\n", in_irq(), in_interrupt());
	link->state &= ~DEV_PRESENT;
	if (link->state & DEV_CONFIG) {
		if (!(link->state & DEV_CONFIG_PENDING))
			affix_uart_tty_detach(btuart->uart.name);
		unregister_serial(btuart->line);
		link->dev = NULL;
		pcmcia_release_configuration(link->handle);
		pcmcia_release_io(link->handle, &link->io);
		pcmcia_release_irq(link->handle, &link->irq);
		link->state &= ~DEV_CONFIG;
	}
	DBFEXIT;
}

/*======================================================================

    btuart_attach() creates an "instance" of the driver, allocating
    local data structures for one device.  The device is registered
    with Card Services.

======================================================================*/

int is_dtl(btuart_cs_t *btuart)
{
	if (btuart->uart.manfid == MANFID_NOKIA) {
		if (btuart->uart.prodid == PRODID_DTL1 || btuart->uart.prodid == PRODID_DTL4)
			return 1;
	} else if (btuart->uart.manfid == MANFID_SOCKET && btuart->uart.prodid == PRODID_SOCKET_BTCF1)
		return 1;
	return 0;
}

dev_link_t *btuart_attach(void)
{
	btuart_cs_t	*btuart;
	client_reg_t 	client_reg;
	dev_link_t 	*link;
	int 		i, ret;
    
	DBFENTER;

	/* Create new btuart device */
	btuart = kmalloc(sizeof(*btuart), GFP_KERNEL);
	if (!btuart)
		return NULL;
	memset(btuart, 0, sizeof(*btuart));
	link = &btuart->link;
	link->priv = btuart;

	link->io.Attributes1 = IO_DATA_PATH_WIDTH_8;
	link->io.NumPorts1 = 8;
	link->irq.Attributes = IRQ_TYPE_EXCLUSIVE;
	link->irq.IRQInfo1 = IRQ_INFO2_VALID|IRQ_LEVEL_ID;
	if (irq_list[0] == -1)
		link->irq.IRQInfo2 = irq_mask;
	else
		for (i = 0; i < 4; i++)
			link->irq.IRQInfo2 |= 1 << irq_list[i];
	link->conf.Attributes = CONF_ENABLE_IRQ;
	link->conf.Vcc = 50;
	if (do_sound) {
		link->conf.Attributes |= CONF_ENABLE_SPKR;
		link->conf.Status = CCSR_AUDIO_ENA;
	}
	link->conf.IntType = INT_MEMORY_AND_IO;
    
	/* Register with Card Services */
	link->next = dev_list;
	dev_list = link;
	client_reg.dev_info = &dev_info;
	client_reg.Attributes = INFO_IO_CLIENT | INFO_CARD_SHARE;
	client_reg.EventMask =
		CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
		CS_EVENT_RESET_PHYSICAL | CS_EVENT_CARD_RESET |
		CS_EVENT_PM_SUSPEND | CS_EVENT_PM_RESUME;
	client_reg.event_handler = &btuart_event;
	client_reg.Version = 0x0210;
	client_reg.event_callback_args.client_data = link;
	ret = pcmcia_register_client(&link->handle, &client_reg);
	if (ret != CS_SUCCESS) {
		cs_error(link->handle, RegisterClient, ret);
		btuart_detach(link);
		return NULL;
	}
	DBFEXIT;
	return link;
}

/*======================================================================

    This deletes a driver "instance".  The device is de-registered
    with Card Services.  If it has been released, all local data
    structures are freed.  Otherwise, the structures will be freed
    when the device is released.

======================================================================*/

void btuart_detach(dev_link_t *link)
{
	btuart_cs_t	*btuart = link->priv;
	dev_link_t	**linkp;
	int 		ret;

	DBFENTER;
	//DBPRT("irq?: %lu, int?: %lu\n", in_irq(), in_interrupt());
	/* Locate device structure */
	for (linkp = &dev_list; *linkp; linkp = &(*linkp)->next)
		if (*linkp == link)
			break;

	if (*linkp == NULL)
		return;

	/*
	 * Ensure any outstanding scheduled tasks are completed.
	 */
	flush_scheduled_work();

	//affix_uart_tty_detach(btuart->uart.name);
	
	btuart_remove(link);

	if (link->handle) {
		ret = pcmcia_deregister_client(link->handle);
		if (ret != CS_SUCCESS)
			cs_error(link->handle, DeregisterClient, ret);
	}
    
	/* Unlink device structure, free bits */
	*linkp = link->next;

	kfree(btuart);
	DBFEXIT;
}

/*====================================================================*/
int setup_btuart(btuart_cs_t *btuart, ioaddr_t port, int irq)
{
	struct serial_struct	serial;
	int 			line;

	DBFENTER;

	memset(&serial, 0, sizeof(serial));
	serial.port = port;
	serial.irq = irq;
	serial.flags = UPF_SKIP_TEST | UPF_SHARE_IRQ;
	line = register_serial(&serial);
	if (line < 0) {
		BTERROR("affix_uart_cs: register_serial() at 0x%04x, "
				"irq %d failed\n", serial.port, serial.irq);
		return -1;
	}
	btuart->line = line;
	// setup dev_node_t structure
	sprintf(btuart->node.dev_name, "ttyS%d", line);
	btuart->node.major = TTY_MAJOR;
	btuart->node.minor = 0x40 + line;
	// setup serial structure
	btuart->uart.count = &btuart->link.open;
#ifdef CONFIG_DEVFS_FS
        sprintf(btuart->uart.name, "/dev/tts/%d", line);
#else 
	/* no devfs */
	sprintf(btuart->uart.name, "/dev/ttyS%d", line);
#endif
	btuart->uart.owner = THIS_MODULE;

	DBFEXIT;
	return 0;
}

/*====================================================================*/

static int first_tuple(client_handle_t handle, tuple_t * tuple, cisparse_t * parse)
{
	int i;
	i = pcmcia_get_first_tuple(handle, tuple);
	if (i != CS_SUCCESS)
		return CS_NO_MORE_ITEMS;
	i = pcmcia_get_tuple_data(handle, tuple);
	if (i != CS_SUCCESS)
		return i;
	return pcmcia_parse_tuple(handle, tuple, parse);
}

static int next_tuple(client_handle_t handle, tuple_t * tuple, cisparse_t * parse)
{
	int i;
	i = pcmcia_get_next_tuple(handle, tuple);
	if (i != CS_SUCCESS)
		return CS_NO_MORE_ITEMS;
	i = pcmcia_get_tuple_data(handle, tuple);
	if (i != CS_SUCCESS)
		return i;
	return pcmcia_parse_tuple(handle, tuple, parse);
}

/*====================================================================

    btuart_config() is scheduled to run after a CARD_INSERTION event
    is received, to configure the PCMCIA socket, and to make the
    serial device available to the system.

======================================================================*/

void btuart_config(dev_link_t *link)
{
	client_handle_t handle = link->handle;
	btuart_cs_t *btuart = link->priv;
	tuple_t tuple;
	u_short buf[128];
	cisparse_t parse;
	cistpl_cftable_entry_t *cf = &parse.cftable_entry;
	int i, j, last_ret, last_fn, try;
	static ioaddr_t base[5] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8, 0x0 };
	config_info_t config;
	
	DBFENTER;
    
	tuple.TupleData = (cisdata_t *)buf;
	tuple.TupleOffset = 0;
	tuple.TupleDataMax = 255;
	tuple.Attributes = 0;
	/* Get configuration register information */
	tuple.DesiredTuple = CISTPL_CONFIG;
	last_ret = first_tuple(handle, &tuple, &parse);
	if (last_ret != CS_SUCCESS) {
		last_fn = ParseTuple;
		goto cs_failed;
	}
	link->conf.ConfigBase = parse.config.base;
	link->conf.Present = parse.config.rmask[0];
    
	/* Configure card */
	link->state |= DEV_CONFIG;

	/* read card info */
	tuple.DesiredTuple = CISTPL_MANFID;
	if (first_tuple(handle, &tuple, &parse) == CS_SUCCESS) {
		btuart->uart.manfid = __le16_to_cpu(buf[0]);
		btuart->uart.prodid = __le16_to_cpu(buf[1]);
	}

	/* If the card is already configured, look up the port and irq */
	i = pcmcia_get_configuration_info(handle, &config);
	link->conf.Vcc = config.Vcc;
    
	/* First pass: look for a config entry that looks normal. */
	tuple.TupleData = (cisdata_t *)buf;
	tuple.TupleOffset = 0;
	tuple.TupleDataMax = 255;
	tuple.Attributes = 0;
	tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
	/* Two tries: without IO aliases, then with aliases */
	for (try = 0; try < 4; try++) {
		i = first_tuple(handle, &tuple, &parse);
		while (i != CS_NO_MORE_ITEMS) {
			if (i != CS_SUCCESS)
				goto next_entry;
			DBPRT("try: %d, index: %d, nwin: %d, base: %#x, len: %d, lines: %d\n", 
					try, cf->index, cf->io.nwin, cf->io.win[0].base, 
					cf->io.win[0].len, cf->io.flags & CISTPL_IO_LINES_MASK);
			if (cf->vpp1.present & (1<<CISTPL_POWER_VNOM))
				link->conf.Vpp1 = link->conf.Vpp2 =
					cf->vpp1.param[CISTPL_POWER_VNOM]/10000;
			i = CS_NO_MORE_ITEMS;
			if (cf->io.nwin > 0) {
#if 1
				if (!(try & 0x02) && (cf->io.win[0].len >= 8) && (cf->io.win[0].base != 0)) {
					DBPRT("first pass\n");
					link->conf.ConfigIndex = cf->index;
					link->io.BasePort1 = cf->io.win[0].base;
					link->io.NumPorts1 = cf->io.win[0].len;	// NEW in 2.0.0
					link->io.IOAddrLines = (try & 0x01) ? cf->io.flags & CISTPL_IO_LINES_MASK : 16;
					i = pcmcia_request_io(link->handle, &link->io);
				}
#endif
				if ((try & 0x02) && (cf->io.win[0].base == 0)) {
					DBPRT("second pass\n");
					/* dinamically allocate base address - by Dmitry Kasatkin */
					link->conf.ConfigIndex = cf->index;
					link->io.BasePort1 = cf->io.win[0].base;
					if (is_dtl(btuart)) {
						link->io.NumPorts1 = 8; /*yo*/
					} else
						link->io.NumPorts1 = cf->io.win[0].len;
					link->io.IOAddrLines = (try & 0x01) ? 16 : cf->io.flags & CISTPL_IO_LINES_MASK;
					i = pcmcia_request_io(link->handle, &link->io);
				}
			}
			if (i == CS_SUCCESS)
				goto found_port;
	next_entry:
			i = next_tuple(handle, &tuple, &parse);
		}
	}
    
	/* Second pass: try to find an entry that isn't picky about
	   its base address, then try to grab any standard serial port
	   address, and finally try to get any free port. */
	i = first_tuple(handle, &tuple, &parse);
	while (i != CS_NO_MORE_ITEMS) {
		DBPRT("index: %d, nwin: %d, base: %#x, len: %d, lines: %d\n", 
				cf->index, cf->io.nwin, cf->io.win[0].base, 
				cf->io.win[0].len, cf->io.flags & CISTPL_IO_LINES_MASK);
		if ((i == CS_SUCCESS) && (cf->io.nwin > 0) &&
		    ((cf->io.flags & CISTPL_IO_LINES_MASK) <= 3)) {
			link->conf.ConfigIndex = cf->index;
			for (j = 0; j < 5; j++) {
				link->io.BasePort1 = base[j];
				link->io.IOAddrLines = base[j] ? 16 : 3;
				i = pcmcia_request_io(link->handle, &link->io);
				if (i == CS_SUCCESS)
					goto found_port;
			}
		}
		i = next_tuple(handle, &tuple, &parse);
	}

found_port:
	if (i != CS_SUCCESS) {
		cs_error(link->handle, RequestIO, i);
		goto failed;
	}
    
	//DBPRT("IRQ: %d\n", link->irq.AssignedIRQ);
	i = pcmcia_request_irq(link->handle, &link->irq);
	if (i != CS_SUCCESS) {
		cs_error(link->handle, RequestIRQ, i);
		link->irq.AssignedIRQ = 0;
	}

	i = pcmcia_request_configuration(link->handle, &link->conf);
	if (i != CS_SUCCESS) {
		cs_error(link->handle, RequestConfiguration, i);
		goto failed;
	}

	i = setup_btuart(btuart, link->io.BasePort1, link->irq.AssignedIRQ);
	if (i)
		goto failed;
    
	if (btuart->uart.manfid == MANFID_IBM) {
		conf_reg_t reg = { 0, CS_READ, 0x800, 0 };
		last_ret = pcmcia_access_configuration_register(link->handle, &reg);
		if (last_ret) {
			last_fn = AccessConfigurationRegister;
			goto cs_failed;
		}
		reg.Action = CS_WRITE;
		reg.Value = reg.Value | 1;
		last_ret = pcmcia_access_configuration_register(link->handle, &reg);
		if (last_ret) {
			last_fn = AccessConfigurationRegister;
			goto cs_failed;
		}
	}

	link->dev = &btuart->node;

	/* register uart device */
	last_ret = affix_uart_tty_attach(&btuart->uart);
	if (last_ret) {
		BTERROR("Can't register network device\n");
		goto failed;
	}
	link->state &= ~DEV_CONFIG_PENDING;
	DBFEXIT;
	return;
cs_failed:
	cs_error(link->handle, last_fn, last_ret);
failed:
	btuart_remove(link);

}

/*======================================================================

    The card status event handler.  Mostly, this schedules other
    stuff to run after an event is received.  A CARD_REMOVAL event
    also sets some flags to discourage the serial drivers from
    talking to the ports.
    
======================================================================*/

int btuart_event(event_t event, int priority, event_callback_args_t *args)
{
	dev_link_t	*link = args->client_data;
	btuart_cs_t 	*btuart = link->priv;

	DBFENTER;
	//DBPRT("irq?: %lu, int?: %lu\n", in_irq(), in_interrupt());
	DBPRT("comm: %s, lock_depth: %d\n", current->comm, current->lock_depth);
	switch (event) {
	case CS_EVENT_CARD_REMOVAL:
		DBPRT("CS_EVENT_CARD_REMOVAL\n");
		btuart_remove(link);
		break;
	case CS_EVENT_CARD_INSERTION:
		DBPRT("CS_EVENT_CARD_INSERTTION\n");
		link->state |= DEV_PRESENT | DEV_CONFIG_PENDING;
		btuart_config(link);
		break;
	case CS_EVENT_PM_SUSPEND:
		DBPRT("CS_EVENT_PM_SUSPEND\n");
		link->state |= DEV_SUSPEND;
		/* Fall through... */
	case CS_EVENT_RESET_PHYSICAL:
		DBPRT("CS_EVENT_RESET_PHYSICAL\n");
		if (link->state & DEV_CONFIG) {
			affix_uart_tty_suspend(btuart->uart.name);
			pcmcia_release_configuration(link->handle);
			DBPRT("CS_EVENT_RESET_PHYSICAL\n");
		}
		break;
	case CS_EVENT_PM_RESUME:
		DBPRT("CS_EVENT_PM_RESUME\n");
		link->state &= ~DEV_SUSPEND;
		/* Fall through... */
	case CS_EVENT_CARD_RESET:
		DBPRT("CS_EVENT_CARD_RESET\n");
		if (DEV_OK(link)) {
			pcmcia_request_configuration(link->handle, &link->conf);
			affix_uart_tty_resume(btuart->uart.name);
			DBPRT("CS_EVENT_CARD_RESET\n");
		}
		break;
	}

	DBFEXIT;
	return 0;
}

/*====================================================================*/

int __init init_btuart_cs(void)
{
	int	        err;
	servinfo_t 	serv;

	DBFENTER;
	pcmcia_get_card_services_info(&serv);
	if (serv.Revision != CS_RELEASE_CODE) {
		printk(KERN_NOTICE "affix_uart_cs: Card Services release does not match!\n");
		return -1;
	}
	err = pcmcia_register_driver(&btuart_driver);
	if (err)
		return err;
	DBFEXIT;
	return 0;
}

void __exit exit_btuart_cs(void)
{
	DBFENTER;
	pcmcia_unregister_driver(&btuart_driver);
	while (dev_list != NULL)
		btuart_detach(dev_list);
	DBFEXIT;
}

/*  If we are resident in kernel we want to call init_btuart_cs manually. */
module_init(init_btuart_cs);
module_exit(exit_btuart_cs);

MODULE_AUTHOR("Dmitry Kasatkin <dmitry.kasatkin@nokia.com>");
MODULE_DESCRIPTION("PCMCIA UART driver for Bluetooth Cards");
MODULE_LICENSE("GPL");

