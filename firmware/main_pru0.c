/*
 * Copyright (C) 2016 Zubeen Tolani <ZeekHuge - zeekhuge@gmail.com>
 *
 * The code is developed as a part of BeagleScope project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdint.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include <pru_virtqueue.h>
#include <pru_rpmsg.h>
#include <rsc_types.h>
#include "resource_table_pru0.h"
#include "common_pru_defs.h"

volatile register uint32_t __R30;
volatile register uint32_t __R31;

uint8_t msg_from_kernel[RPMSG_BUF_SIZE];
void main(void)
{
	struct pru_rpmsg_transport transport;
	uint16_t src, dst, len;
	volatile uint8_t *status;

	/* pointer to starting of 12KB shared RAM */
	int32_t *ptr_to_shared_mem;
	ptr_to_shared_mem = (int32_t *) SHARED_MEM_ADDR;

	/* allow OCP master port access by the PRU so the PRU can read external
	 * memories */
	CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;
	/* Writing to EISR regoster to enable interrupt */
	CT_INTC.EISR_bit.EN_SET_IDX = INT_P0_to_P1;
	/* Make sure the Linux drivers are ready for RPMsg communication */
	status = &resourceTable.rpmsg_vdev.status;
	while (!(*status & 4));
	/* Initialize pru_virtqueue */
	pru_virtqueue_init(&transport.virtqueue0, &resourceTable.rpmsg_vring0,
				INT_P0_to_ARM, INT_ARM_to_P0);
	pru_virtqueue_init(&transport.virtqueue1, &resourceTable.rpmsg_vring1,
				INT_P0_to_ARM, INT_ARM_to_P0);

	/* Writing data that is to be shared to PRU1, in shared
	   RAM */
	*(ptr_to_shared_mem) = 10000001;
	*(ptr_to_shared_mem + 1 ) = 1<<31 ;
	while (pru_rpmsg_channel(RPMSG_NS_CREATE, &transport, "rpmsg-pru",
				 "Channel 30",
				 30)!= PRU_RPMSG_SUCCESS);
	while (1) {
			/* Check bit 30 of register R31 to see if the ARM has kicked us */
			if(__R31 & (1<<HOST_ARM_TO_PRU0_CB)) {
				/* Clear the event status */
				CT_INTC.SICR_bit.STS_CLR_IDX = INT_ARM_to_P0;
				/* Receive all available messages, multiple messages can be sent per kick */
				if (pru_rpmsg_receive(&transport, &src,
						      &dst, msg_from_kernel,
						&len) == PRU_RPMSG_SUCCESS) {
					/* Generating system event INT_P0_to_P1 */
					__R31 = ( (1 << 5) | (INT_P0_to_P1 - 16));
				}
			}
		}

}
