/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _WLAN_IOT_SIM_UTILS_API_H_
#define _WLAN_IOT_SIM_UTILS_API_H_

#include <wlan_objmgr_cmn.h>
#include <wlan_lmac_if_def.h>

/* Forward Declarations */
struct wmi_iot_sim_cmd_ops;

/*
 * wlan_iot_sim_init() - API to init iot_sim component
 *
 * This API is invoked from dispatcher init during all component init.
 * This API will register all required handlers for pdev and peer object
 * create/delete notification.
 *
 * Return: SUCCESS,
 *         Failure
 */
QDF_STATUS wlan_iot_sim_init(void);

/*
 * wlan_iot_sim_deinit() - API to deinit iot_sim component
 *
 * This API is invoked from dispatcher deinit during all component deinit.
 * This API will unregister all registered handlers for pdev and peer object
 * create/delete notification.
 *
 * Return: SUCCESS,
 *         Failure
 */
QDF_STATUS wlan_iot_sim_deinit(void);

/*
 * wlan_register_wmi_iot_sim_cmd_ops() - Register operations related to wmi
 * commands on iot_sim parameters
 * @pdev    - the physical device object
 * @cmd_ops - pointer to the structure holding the operations
 *	     related to wmi commands on iot_sim parameters
 *
 * API to register operations related to wmi commands on iot_sim parameters
 *
 * Return: None
 */
void
wlan_register_wmi_iot_sim_cmd_ops(struct wlan_objmgr_pdev *pdev,
				  struct wmi_iot_sim_cmd_ops *cmd_ops);

#endif /* _WLAN_IOT_SIM_UTILS_API_H_*/
