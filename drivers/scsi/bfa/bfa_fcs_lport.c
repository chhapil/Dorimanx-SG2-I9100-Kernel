/*
 * Copyright (c) 2005-2010 Brocade Communications Systems, Inc.
 * All rights reserved
 * www.brocade.com
 *
 * Linux driver for Brocade Fibre Channel Host Bus Adapter.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (GPL) Version 2 as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "bfad_drv.h"
#include "bfad_im.h"
#include "bfa_fcs.h"
#include "bfa_fcbuild.h"
#include "bfa_fc.h"

BFA_TRC_FILE(FCS, PORT);

/*
 * ALPA to LIXA bitmap mapping
 *
 * ALPA 0x00 (Word 0, Bit 30) is invalid for N_Ports. Also Word 0 Bit 31
 * is for L_bit (login required) and is filled as ALPA 0x00 here.
 */
static const u8 loop_alpa_map[] = {
	0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x0F, 0x10, /* Word 0 Bits 31..24 */
	0x17, 0x18, 0x1B, 0x1D, 0x1E, 0x1F, 0x23, 0x25, /* Word 0 Bits 23..16 */
	0x26, 0x27, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, /* Word 0 Bits 15..08 */
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x39, 0x3A, /* Word 0 Bits 07..00 */

	0x3C, 0x43, 0x45, 0x46, 0x47, 0x49, 0x4A, 0x4B, /* Word 1 Bits 31..24 */
	0x4C, 0x4D, 0x4E, 0x51, 0x52, 0x53, 0x54, 0x55, /* Word 1 Bits 23..16 */
	0x56, 0x59, 0x5A, 0x5C, 0x63, 0x65, 0x66, 0x67, /* Word 1 Bits 15..08 */
	0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x71, 0x72, /* Word 1 Bits 07..00 */

	0x73, 0x74, 0x75, 0x76, 0x79, 0x7A, 0x7C, 0x80, /* Word 2 Bits 31..24 */
	0x81, 0x82, 0x84, 0x88, 0x8F, 0x90, 0x97, 0x98, /* Word 2 Bits 23..16 */
	0x9B, 0x9D, 0x9E, 0x9F, 0xA3, 0xA5, 0xA6, 0xA7, /* Word 2 Bits 15..08 */
	0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xB1, 0xB2, /* Word 2 Bits 07..00 */

	0xB3, 0xB4, 0xB5, 0xB6, 0xB9, 0xBA, 0xBC, 0xC3, /* Word 3 Bits 31..24 */
	0xC5, 0xC6, 0xC7, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, /* Word 3 Bits 23..16 */
	0xCE, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD9, /* Word 3 Bits 15..08 */
	0xDA, 0xDC, 0xE0, 0xE1, 0xE2, 0xE4, 0xE8, 0xEF, /* Word 3 Bits 07..00 */
};

static void     bfa_fcs_lport_send_ls_rjt(struct bfa_fcs_lport_s *port,
					 struct fchs_s *rx_fchs, u8 reason_code,
					 u8 reason_code_expl);
static void     bfa_fcs_lport_plogi(struct bfa_fcs_lport_s *port,
			struct fchs_s *rx_fchs, struct fc_logi_s *plogi);
static void     bfa_fcs_lport_online_actions(struct bfa_fcs_lport_s *port);
static void     bfa_fcs_lport_offline_actions(struct bfa_fcs_lport_s *port);
static void     bfa_fcs_lport_unknown_init(struct bfa_fcs_lport_s *port);
static void     bfa_fcs_lport_unknown_online(struct bfa_fcs_lport_s *port);
static void     bfa_fcs_lport_unknown_offline(struct bfa_fcs_lport_s *port);
static void     bfa_fcs_lport_deleted(struct bfa_fcs_lport_s *port);
static void     bfa_fcs_lport_echo(struct bfa_fcs_lport_s *port,
			struct fchs_s *rx_fchs,
			struct fc_echo_s *echo, u16 len);
static void     bfa_fcs_lport_rnid(struct bfa_fcs_lport_s *port,
			struct fchs_s *rx_fchs,
			struct fc_rnid_cmd_s *rnid, u16 len);
static void     bfa_fs_port_get_gen_topo_data(struct bfa_fcs_lport_s *port,
			struct fc_rnid_general_topology_data_s *gen_topo_data);

static void	bfa_fcs_lport_fab_init(struct bfa_fcs_lport_s *port);
static void	bfa_fcs_lport_fab_online(struct bfa_fcs_lport_s *port);
static void	bfa_fcs_lport_fab_offline(struct bfa_fcs_lport_s *port);

static void	bfa_fcs_lport_n2n_init(struct bfa_fcs_lport_s *port);
static void	bfa_fcs_lport_n2n_online(struct bfa_fcs_lport_s *port);
static void	bfa_fcs_lport_n2n_offline(struct bfa_fcs_lport_s *port);

static void	bfa_fcs_lport_loop_init(struct bfa_fcs_lport_s *port);
static void	bfa_fcs_lport_loop_online(struct bfa_fcs_lport_s *port);
static void	bfa_fcs_lport_loop_offline(struct bfa_fcs_lport_s *port);

static struct {
	void		(*init) (struct bfa_fcs_lport_s *port);
	void		(*online) (struct bfa_fcs_lport_s *port);
	void		(*offline) (struct bfa_fcs_lport_s *port);
} __port_action[] = {
	{
	bfa_fcs_lport_unknown_init, bfa_fcs_lport_unknown_online,
			bfa_fcs_lport_unknown_offline}, {
	bfa_fcs_lport_fab_init, bfa_fcs_lport_fab_online,
			bfa_fcs_lport_fab_offline}, {
	bfa_fcs_lport_n2n_init, bfa_fcs_lport_n2n_online,
			bfa_fcs_lport_n2n_offline}, {
	bfa_fcs_lport_loop_init, bfa_fcs_lport_loop_online,
			bfa_fcs_lport_loop_offline},
	};

/*
 *  fcs_port_sm FCS logical port state machine
 */

enum bfa_fcs_lport_event {
	BFA_FCS_PORT_SM_CREATE = 1,
	BFA_FCS_PORT_SM_ONLINE = 2,
	BFA_FCS_PORT_SM_OFFLINE = 3,
	BFA_FCS_PORT_SM_DELETE = 4,
	BFA_FCS_PORT_SM_DELRPORT = 5,
	BFA_FCS_PORT_SM_STOP = 6,
};

static void     bfa_fcs_lport_sm_uninit(struct bfa_fcs_lport_s *port,
					enum bfa_fcs_lport_event event);
static void     bfa_fcs_lport_sm_init(struct bfa_fcs_lport_s *port,
					enum bfa_fcs_lport_event event);
static void     bfa_fcs_lport_sm_online(struct bfa_fcs_lport_s *port,
					enum bfa_fcs_lport_event event);
static void     bfa_fcs_lport_sm_offline(struct bfa_fcs_lport_s *port,
					enum bfa_fcs_lport_event event);
static void     bfa_fcs_lport_sm_deleting(struct bfa_fcs_lport_s *port,
					enum bfa_fcs_lport_event event);
static void	bfa_fcs_lport_sm_stopping(struct bfa_fcs_lport_s *port,
					enum bfa_fcs_lport_event event);

static void
bfa_fcs_lport_sm_uninit(
	struct bfa_fcs_lport_s *port,
	enum bfa_fcs_lport_event event)
{
