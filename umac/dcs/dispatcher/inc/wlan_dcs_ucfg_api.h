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

/**
 * DOC: This file has the dcs dispatcher API which is exposed to outside of dcs
 * component.
 */
#ifndef _WLAN_DCS_UCFG_API_H_
#define _WLAN_DCS_UCFG_API_H_

#include <qdf_status.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_dcs_public_structs.h>

/**
 * @brief List of DCS capabilities that can be set or unset
 *      dynamically
 * @see UMAC auto channel selection document for details on each feature
 *
 */
#define CAP_DCS_CWIM   0x1
#define CAP_DCS_WLANIM 0x2
#define CAP_DCS_MASK   (CAP_DCS_CWIM | CAP_DCS_WLANIM)

/**
 * typedef dcs_callback() - DCS callback
 * @psoc: Pointer to psoc
 * @pdev_id: pdev id
 * @interference_type: interference type
 * @arg: list of arguments
 */
typedef void (*dcs_callback)(
		struct wlan_objmgr_psoc *psoc,
		uint8_t pdev_id,
		uint8_t interference_type,
		void *arg);

#ifdef DCS_INTERFERENCE_DETECTION
/**
 * ucfg_dcs_register_cb() - API to register dcs callback
 * @psoc: pointer to psoc object
 * @cbk: dcs callback to be registered
 * @arg: argument
 *
 * This function gets called to register dcs callback function
 *
 * Return: None
 */
void ucfg_dcs_register_cb(
		struct wlan_objmgr_psoc *psoc,
		dcs_callback cbk,
		void *arg);

/**
 * ucfg_dcs_register_user_cb() - API to register dcs user callback
 * @psoc: pointer to psoc object
 * @pdev_id: pdev id
 * @vdev_id: vdev id
 * @cb: dcs user callback to be registered
 *
 * Return: None
 */
void ucfg_dcs_register_user_cb(struct wlan_objmgr_psoc *psoc,
			 uint8_t pdev_id, uint8_t vdev_id,
			 void (*cb)(uint8_t vdev_id,
				    struct wlan_host_dcs_im_user_stats *stats,
				    int status));

/**
 * ucfg_wlan_dcs_cmd(): API to send dcs command
 * @psoc: pointer to psoc object
 * @pdev_id: pdev_id
 * @is_host_pdev_id: pdev_id is host id or not
 *
 * This function gets called to send dcs command
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS
ucfg_wlan_dcs_cmd(struct wlan_objmgr_psoc *psoc,
		  uint32_t pdev_id,
		  bool is_host_pdev_id);

/**
 * ucfg_config_dcs_enable() - API to config dcs enable
 * @psoc: pointer to psoc object
 * @pdev_id: pdev id
 * @interference_type: CAP_DCS_CWIM, CAP_DCS_WLANIM, CAP_DCS_MASK
 *
 * This function gets called to config dcs enable
 *
 * Return: None
 */
void ucfg_config_dcs_enable(struct wlan_objmgr_psoc *psoc,
			    uint32_t pdev_id,
			    uint8_t interference_type);

/**
 * ucfg_config_dcs_disable() - API to config dcs disable
 * @psoc: pointer to psoc object
 * @pdev_id: pdev id
 * @interference_type: CAP_DCS_CWIM, CAP_DCS_WLANIM, CAP_DCS_MASK
 *
 * This function gets called to config dcs disable
 *
 * Return: None
 */
void ucfg_config_dcs_disable(struct wlan_objmgr_psoc *psoc,
			     uint32_t pdev_id,
			     uint8_t interference_type);

/**
 * ucfg_get_dcs_enable() - API to get dcs enable
 * @psoc: pointer to psoc object
 * @pdev_id: pdev id
 *
 * This function gets called to get current host
 * config dcs enable/disable status
 *
 * Return: WLANIM/CWIM enable status
 */
uint8_t ucfg_get_dcs_enable(struct wlan_objmgr_psoc *psoc, uint8_t pdev_id);

/**
 * ucfg_dcs_clear() - API to clear dcs related information
 * @psoc: pointer to psoc object
 * @pdev_id: pdev id
 *
 * This function gets called to clear dcs related information
 *
 * Return: None
 */
void ucfg_dcs_clear(struct wlan_objmgr_psoc *psoc, uint32_t pdev_id);

/**
 * ucfg_config_dcs_event_data() - config dcs event data to do algorithm
 * process or not
 * @psoc: psoc pointer
 * @pdev_id: pdev_id
 * @dcs_algorithm_process: dcs algorithm process or not
 *
 * The function gets called to config dcs event data to do algorithm
 * process or not
 *
 * Return: None
 */
void ucfg_config_dcs_event_data(struct wlan_objmgr_psoc *psoc, uint32_t pdev_id,
				bool dcs_algorithm_process);

/*
 * ucfg_dcs_reset_user_stats() - API to reset dcs user stats
 * @psoc: pointer to psoc object
 * @pdev_id: pdev id
 *
 * Return: None
 */
void ucfg_dcs_reset_user_stats(struct wlan_objmgr_psoc *psoc, uint8_t pdev_id);

/**
 * ucfg_dcs_set_user_request() - API to set dcs user stats request counter
 * @psoc: pointer to psoc object
 * @pdev_id: pdev id
 * @user_request_count: user stats request counter
 *
 * Return: None
 */
void ucfg_dcs_set_user_request(struct wlan_objmgr_psoc *psoc, uint8_t pdev_id,
			       uint32_t user_request_count);
#else
static inline void
ucfg_dcs_register_cb(struct wlan_objmgr_psoc *psoc, dcs_callback cbk, void *arg)
{
}

static inline void
ucfg_dcs_register_user_cb(struct wlan_objmgr_psoc *psoc,
			  uint8_t pdev_id, uint8_t vdev_id,
			  void (*cb)(uint8_t vdev_id,
				     struct wlan_host_dcs_im_user_stats *stats,
				     int status))
{
}

static inline QDF_STATUS
ucfg_wlan_dcs_cmd(struct wlan_objmgr_psoc *psoc, uint32_t pdev_id,
		  bool is_host_pdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
ucfg_config_dcs_enable(struct wlan_objmgr_psoc *psoc, uint32_t pdev_id,
		       uint8_t interference_type)
{
}

static inline void
ucfg_config_dcs_disable(struct wlan_objmgr_psoc *psoc, uint32_t pdev_id,
			uint8_t interference_type)
{
}

static inline uint8_t
ucfg_get_dcs_enable(struct wlan_objmgr_psoc *psoc, uint8_t pdev_id)
{
	return 0;
}

static inline void
ucfg_dcs_clear(struct wlan_objmgr_psoc *psoc, uint32_t pdev_id)
{
}

static inline void
ucfg_config_dcs_event_data(struct wlan_objmgr_psoc *psoc, uint32_t pdev_id,
			   bool dcs_algorithm_process)
{
}

static inline void
ucfg_dcs_reset_user_stats(struct wlan_objmgr_psoc *psoc, uint8_t pdev_id)
{
}

static inline void
ucfg_dcs_set_user_request(struct wlan_objmgr_psoc *psoc, uint8_t pdev_id,
			  uint32_t user_request_count)
{
}
#endif
#endif /* _WLAN_DCS_UCFG_API_H_ */
