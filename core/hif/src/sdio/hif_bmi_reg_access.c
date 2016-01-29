/*
 * Copyright (c) 2013-2016 The Linux Foundation. All rights reserved.
 *
 ***Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#include "athdefs.h"
#include "a_types.h"
#include "a_osapi.h"
#define ATH_MODULE_NAME hif
#include "a_debug.h"
#define ATH_DEBUG_BMI  ATH_DEBUG_MAKE_MODULE_MASK(0)
#include "hif.h"
#include "bmi.h"
#include "htc_api.h"
#include "if_sdio.h"
#include "regtable_sdio.h"

#define BMI_COMMUNICATION_TIMEOUT       100000

static A_BOOL pending_events_func_check;
static A_UINT32 command_credits;
static A_UINT32 *p_bmi_cmd_credits = &command_credits;
/* BMI Access routines */
static A_STATUS
bmi_buffer_send(struct hif_sdio_dev *device, A_UCHAR *buffer, A_UINT32 length)
{
	A_STATUS status;
	A_UINT32 timeout;
	A_UINT32 address;
	A_UINT32 mboxAddress[HTC_MAILBOX_NUM_MAX];

	hif_configure_device(device, HIF_DEVICE_GET_MBOX_ADDR,
			     &mboxAddress[0], sizeof(mboxAddress));

	*p_bmi_cmd_credits = 0;
	timeout = BMI_COMMUNICATION_TIMEOUT;

	while (timeout-- && !(*p_bmi_cmd_credits)) {
		/* Read the counter register to get the command credits */
		address =
		      COUNT_DEC_ADDRESS + (HTC_MAILBOX_NUM_MAX + ENDPOINT1) * 4;
		/* hit the credit counter with a 4-byte access, the first
		 * byte read will hit the counter and cause
		 * a decrement, while the remaining 3 bytes has no effect.
		 * The rationale behind this is to make all HIF accesses
		 * 4-byte aligned */
		status =
			hif_read_write(device, address,
				       (A_UINT8 *) p_bmi_cmd_credits, 4,
				       HIF_RD_SYNC_BYTE_INC, NULL);
		if (status != A_OK) {
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
			  ("%s:Unable to decrement the credit count register\n",
			  __func__));
			return A_ERROR;
		}
		/* the counter is only 8=bits, ignore anything in the
		 *upper 3 bytes */
		(*p_bmi_cmd_credits) &= 0xFF;
	}

	if (*p_bmi_cmd_credits) {
		address = mboxAddress[ENDPOINT1];
		status = hif_read_write(device, address, buffer, length,
					HIF_WR_SYNC_BYTE_INC, NULL);
		if (status != A_OK) {
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
			  ("%s:Unable to send the BMI data to the device\n",
			  __func__));
			return A_ERROR;
		}
	} else {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
			("%s:BMI Communication timeout - bmi_buffer_send\n",
			__func__));
		return A_ERROR;
	}

	return status;
}

static A_STATUS
bmi_buffer_receive(struct hif_sdio_dev *device,
		   A_UCHAR *buffer, A_UINT32 length, A_BOOL want_timeout)
{
	A_STATUS status;
	A_UINT32 address;
	A_UINT32 mboxAddress[HTC_MAILBOX_NUM_MAX];
	struct _HIF_PENDING_EVENTS_INFO hifPendingEvents;
	static HIF_PENDING_EVENTS_FUNC getPendingEventsFunc;

	if (!pending_events_func_check) {
		/* see if the HIF layer implements an alternative
		 * function to get pending events
		 * do this only once! */
		hif_configure_device(device,
				     HIF_DEVICE_GET_PENDING_EVENTS_FUNC,
				     &getPendingEventsFunc,
				     sizeof(getPendingEventsFunc));
		pending_events_func_check = true;
	}

	hif_configure_device(device, HIF_DEVICE_GET_MBOX_ADDR,
			     &mboxAddress[0], sizeof(mboxAddress));

	/*
	 * During normal bootup, small reads may be required.
	 * Rather than issue an HIF Read and then wait as the Target
	 * adds successive bytes to the FIFO, we wait here until
	 * we know that response data is available.
	 *
	 * This allows us to cleanly timeout on an unexpected
	 * Target failure rather than risk problems at the HIF level.  In
	 * particular, this avoids SDIO timeouts and possibly garbage
	 * data on some host controllers.  And on an interconnect
	 * such as Compact Flash (as well as some SDIO masters) which
	 * does not provide any indication on data timeout, it avoids
	 * a potential hang or garbage response.
	 *
	 * Synchronization is more difficult for reads larger than the
	 * size of the MBOX FIFO (128B), because the Target is unable
	 * to push the 129th byte of data until AFTER the Host posts an
	 * HIF Read and removes some FIFO data.  So for large reads the
	 * Host proceeds to post an HIF Read BEFORE all the data is
	 * actually available to read.  Fortunately, large BMI reads do
	 * not occur in practice -- they're supported for debug/development.
	 *
	 * So Host/Target BMI synchronization is divided into these cases:
	 *  CASE 1: length < 4
	 *        Should not happen
	 *
	 *  CASE 2: 4 <= length <= 128
	 *        Wait for first 4 bytes to be in FIFO
	 *        If CONSERVATIVE_BMI_READ is enabled, also wait for
	 *        a BMI command credit, which indicates that the ENTIRE
	 *        response is available in the the FIFO
	 *
	 *  CASE 3: length > 128
	 *        Wait for the first 4 bytes to be in FIFO
	 *
	 * For most uses, a small timeout should be sufficient and we will
	 * usually see a response quickly; but there may be some unusual
	 * (debug) cases of BMI_EXECUTE where we want an larger timeout.
	 * For now, we use an unbounded busy loop while waiting for
	 * BMI_EXECUTE.
	 *
	 * If BMI_EXECUTE ever needs to support longer-latency execution,
	 * especially in production, this code needs to be enhanced to sleep
	 * and yield.  Also note that BMI_COMMUNICATION_TIMEOUT is currently
	 * a function of Host processor speed.
	 */
	if (length >= 4) {      /* NB: Currently, always true */
		/*
		 * NB: word_available is declared static for esoteric reasons
		 * having to do with protection on some OSes.
		 */
		static A_UINT32 word_available;
		A_UINT32 timeout;

		word_available = 0;
		timeout = BMI_COMMUNICATION_TIMEOUT;
		while ((!want_timeout || timeout--) && !word_available) {

			if (getPendingEventsFunc != NULL) {
				status = getPendingEventsFunc(device,
							      &hifPendingEvents,
							      NULL);
				if (status != A_OK) {
					AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
					  ("%s:Failed to get pending events\n",
					  __func__));
					break;
				}

				if (hifPendingEvents.available_recv_bytes >=
				    sizeof(A_UINT32)) {
					word_available = 1;
				}
				continue;
			}
#if defined(SDIO_3_0)
			status =
				hif_read_write(device, HOST_INT_STATUS_ADDRESS,
					       (A_UINT8 *) &word_available,
					       sizeof(word_available),
					       HIF_RD_SYNC_BYTE_INC, NULL);
			if (status != A_OK) {
				AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
					("%s:Unable to read int status reg\n",
					__func__));
				return A_ERROR;
			}
#else
			status =
			      hif_read_write(device, RX_LOOKAHEAD_VALID_ADDRESS,
					       (A_UINT8 *) &word_available,
					       sizeof(word_available),
					       HIF_RD_SYNC_BYTE_INC, NULL);
			if (status != A_OK) {
				AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
					("%s:Unable to read rx lookahead reg\n",
					__func__));
				return A_ERROR;
			}
#endif
#if defined(SDIO_3_0)
			word_available =
				(HOST_INT_STATUS_MBOX_DATA_GET(word_available) &
				 (1 << ENDPOINT1));
#else
			word_available &= (1 << ENDPOINT1);
#endif

		}

		if (!word_available) {
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
				("%s:BMI Communication timeout FIFO empty\n",
				__func__));
			return A_ERROR;
		}
	}
#define CONSERVATIVE_BMI_READ 0
#if CONSERVATIVE_BMI_READ
	/*
	 * This is an extra-conservative CREDIT check.  It guarantees
	 * that ALL data is available in the FIFO before we start to
	 * read from the interconnect.
	 *
	 * This credit check is useless when firmware chooses to
	 * allow multiple outstanding BMI Command Credits, since the next
	 * credit will already be present.  To restrict the Target to one
	 * BMI Command Credit, see HI_OPTION_BMI_CRED_LIMIT.
	 *
	 * And for large reads (when HI_OPTION_BMI_CRED_LIMIT is set)
	 * we cannot wait for the next credit because the Target's FIFO
	 * will not hold the entire response.  So we need the Host to
	 * start to empty the FIFO sooner.  (And again, large reads are
	 * not used in practice; they are for debug/development only.)
	 *
	 * For a more conservative Host implementation (which would be
	 * safer for a Compact Flash interconnect):
	 *   Set CONSERVATIVE_BMI_READ (above) to 1
	 *   Set HI_OPTION_BMI_CRED_LIMIT and
	 *   reduce BMI_DATASZ_MAX to 32 or 64
	 */
	if ((length > 4) && (length < 128)) {
		A_UINT32 timeout;

		*p_bmi_cmd_credits = 0;
		timeout = BMI_COMMUNICATION_TIMEOUT;
		while ((!want_timeout || timeout--) &&
				!(*p_bmi_cmd_credits)) {
			/* Read the credit counter register */
			address =
					COUNT_ADDRESS +
					(HTC_MAILBOX_NUM_MAX + ENDPOINT1) * 1;
			/* read the counter using a 4-byte read.  Since
			 * the counter is NOT auto-decrementing, we can
			 * read this counter multiple times using a
			 * non-incrementing address mode. The rationale
			 * here is to make all HIF accesses a multiple of
			 * 4 bytes */
			status =
					hif_read_write(device, address,
					(A_UINT8 *) p_bmi_cmd_credits,
					 sizeof(*p_bmi_cmd_credits),
					 HIF_RD_SYNC_BYTE_FIX, NULL);
			if (status != A_OK) {
				AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
				("%s:Unable to read the credit count\n",
				__func__));
				return A_ERROR;
			}
			/* we did a 4-byte read to the same count
			* register so mask off upper bytes */
			(*p_bmi_cmd_credits) &= 0xFF;
		}

			if (!(*p_bmi_cmd_credits)) {
				AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
					("%s:BMI Communication timeout\n",
					__func__));
				return A_ERROR;
			}
		}
#endif

	address = mboxAddress[ENDPOINT1];
	status =
		hif_read_write(device, address, buffer, length,
			       HIF_RD_SYNC_BYTE_INC, NULL);
	if (status != A_OK) {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
			("%s:Unable to read the BMI data from the device\n",
			__func__));
		return A_ERROR;
	}

	return A_OK;
}

CDF_STATUS hif_reg_based_get_target_info(struct hif_sdio_dev *device,
					struct bmi_target_info
					*targ_info) {
	A_STATUS status; A_UINT32 cid;
	AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
			("BMI Get Target Info: Enter (device: 0x%p)\n",
			 device));
	cid = BMI_GET_TARGET_INFO;
	status =
		bmi_buffer_send(device, (A_UCHAR *) &cid, sizeof(cid));
	if (status != A_OK) {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
				("%s:Unable to write to the device\n",
				__func__));
		return A_ERROR;
	}

	status =
		bmi_buffer_receive(device,
				   (A_UCHAR *) &targ_info->target_ver,
				   sizeof(targ_info->target_ver), true);
	if (status != A_OK) {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
			("%s:Unable to read Target Version from the device\n",
			__func__));
		return A_ERROR;
	}

	if (targ_info->target_ver == TARGET_VERSION_SENTINAL) {
		/* Determine how many bytes are in the Target's targ_info */
		status =
			bmi_buffer_receive(device,
					   (A_UCHAR *) &targ_info->
					   target_info_byte_count,
					   sizeof(targ_info->
						 target_info_byte_count), true);
		if (status != A_OK) {
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
				("%s:Unable to read target Info\n",
				__func__));
			return A_ERROR;
		}

		/*
		 * The Target's targ_info doesn't match the Host's targ_info.
		 * We need to do some backwards compatibility work to make this
		 * OK.*/
		A_ASSERT(targ_info->target_info_byte_count ==
			 sizeof(*targ_info));
		/* Read the remainder of the targ_info */
		status = bmi_buffer_receive(device,
					    ((A_UCHAR *) targ_info) +
					    sizeof(targ_info->
						   target_info_byte_count),
					    sizeof(*targ_info) -
					    sizeof(targ_info->
						   target_info_byte_count),
					    true); if (status != A_OK) {
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
				("%s:Unable to read Target Info (%d bytes)\n",
				__func__, targ_info->target_info_byte_count));
			return A_ERROR;
		}
	} else {
		/*
		 * Target must be an AR6001 whose firmware does not
		 * support BMI_GET_TARGET_INFO.  Construct the data
		 * that it would have sent.
		 */
		targ_info->target_info_byte_count = sizeof(*targ_info);
		targ_info->target_type = TARGET_TYPE_AR6001;
	}

	AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
			("BMI Get Target Info: Exit (ver: 0x%x type: 0x%x)\n",
			 targ_info->target_ver,
			 targ_info->target_type));

	return A_OK;
}

CDF_STATUS hif_exchange_bmi_msg(struct ol_softc *scn,
			      A_UINT8 *pSendMessage,
			      A_UINT32 Length,
			      A_UINT8 *pResponseMessage,
			      A_UINT32 *pResponseLength,
			      A_UINT32 TimeoutMS) {
	struct hif_sdio_dev *device = (struct hif_sdio_dev *)scn->hif_hdl;
	A_STATUS status = A_OK; if (device == NULL) {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
			("%s:Null device argument\n",
			__func__));
		return A_EINVAL;
	}

	do {

		status = bmi_buffer_send(device, pSendMessage, Length);
		if (A_FAILED(status)) {
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
				("%s:Unable to Send Message to device\n",
				__func__));
			break;
		}

		if (pResponseMessage != NULL) {
			status =
				bmi_buffer_receive(device, pResponseMessage,
						   *pResponseLength,
						   TimeoutMS ? true : false);
			if (A_FAILED(status)) {
				AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
					("%s:Unable to read response\n",
					__func__));
				break;
			}
		}

	} while (false); return status;
}

A_STATUS
bmi_raw_write(struct hif_sdio_dev *device, A_UCHAR *buffer,
	      A_UINT32 length) {
	return bmi_buffer_send(device, buffer, length);
}

A_STATUS
bmi_raw_read(struct hif_sdio_dev *device, A_UCHAR *buffer,
	     A_UINT32 length, A_BOOL want_timeout)
{
	return bmi_buffer_receive(device, buffer, length,
				  want_timeout);
}

#ifdef BRINGUP_DEBUG
#define SDIO_SCRATCH_1_ADDRESS 0x864
/*Functions used for debugging*/
A_STATUS bmi_write_scratch_register(struct hif_sdio_dev *device,
				    uint32_t buffer) {
	A_STATUS status = A_OK;
	status =
		hif_read_write(device, SDIO_SCRATCH_1_ADDRESS,
			       (A_UINT8 *) &buffer, 4,
			       HIF_WR_SYNC_BYTE_INC, NULL);
	if (status != A_OK) {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
				("%s: Unable to write to 0x%x\n",
				 __func__, SDIO_SCRATCH_1_ADDRESS));
		return A_ERROR;
	} else {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
				("%s: wrote 0x%x to 0x%x\n", __func__,
				 buffer, SDIO_SCRATCH_1_ADDRESS));
	}

	return status;
}

A_STATUS bmi_read_scratch_register(struct hif_sdio_dev *device)
{
	A_STATUS status = A_OK;
	uint32_t buffer = 0;
	status =
		hif_read_write(device, SDIO_SCRATCH_1_ADDRESS,
			       (A_UINT8 *) &buffer, 4,
			       HIF_RD_SYNC_BYTE_INC, NULL);
	if (status != A_OK) {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
				("%s: Unable to read from 0x%x\n",
				 __func__, SDIO_SCRATCH_1_ADDRESS));
		return A_ERROR;
	} else {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
				("%s: read 0x%x from 0x%x\n", __func__,
				 buffer, SDIO_SCRATCH_1_ADDRESS));
	}

	return status;
}
#endif
