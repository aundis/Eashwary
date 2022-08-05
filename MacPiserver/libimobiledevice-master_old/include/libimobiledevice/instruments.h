/**
 * @file libimobiledevice/instruments.h
 * @brief Start a instruments service 
 * @note Requires a mounted developer image.
 * \internal
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef IINSTRUMENTS_H
#define IINSTRUMENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

#define INSTRUMENTS_SERVICE_NAME "com.apple.instruments.remoteserver"

/** @name Error Codes */
/*@{*/
#define INSTRUMENTS_E_SUCCESS                0
#define INSTRUMENTS_E_INVALID_ARG           -1
#define INSTRUMENTS_E_PLIST_ERROR           -2
#define INSTRUMENTS_E_MUX_ERROR             -3
#define INSTRUMENTS_E_BAD_VERSION           -4
#define INSTRUMENTS_E_NO_MEM                -5
#define INSTRUMENTS_E_OP_HEADER_INVALID     -6
#define INSTRUMENTS_E_IO_ERROR              -7
#define INSTRUMENTS_E_NOT_ENOUGH_DATA       -8
#define INSTRUMENTS_E_LAUNCH_FAILED_ERROR   -9
#define INSTRUMENTS_E_APP_DIED_ERROR	   -10

#define INSTRUMENTS_E_UNKNOWN_ERROR       -256
/*@}*/

/** Represents an error code. */
typedef int16_t instruments_error_t;

typedef struct instruments_client_private instruments_client_private;
typedef instruments_client_private *instruments_client_t; /**< The client handle. */

typedef int (*fptrCommandResponseQ) (char*, int);
struct QData
{
	char* data;
	unsigned int len;
	int packetNo;
};


/**
 * Connects to the instruments service on the specified device.
 *
 * @param device The device to connect to.
 * @param service The service descriptor returned by lockdownd_start_service.
 * @param client Pointer that will be set to a newly allocated
 *     instruments_client_t upon successful return.
 *
 * @note This service is only available if a developer disk image has been
 *     mounted.
 *
 * @return INSTRUMENTS_E_SUCCESS on success, INSTRUMENTS_E_INVALID ARG if one
 *     or more parameters are invalid, or INSTRUMENTS_E_CONN_FAILED if the
 *     connection to the device could not be established.
 */
instruments_error_t instruments_client_new(idevice_t device, lockdownd_service_descriptor_t service, instruments_client_t * client);

/**
 * Starts a new instruments service on the specified device and connects to it.
 *
 * @param device The device to connect to.
 * @param client Pointer that will point to a newly allocated
 *     instruments_client_t upon successful return. Must be freed using
 *     instruments_client_free() after use.
 * @param label The label to use for communication. Usually the program name.
 *  Pass NULL to disable sending the label in requests to lockdownd.
 *
 * @return INSTRUMENTS_E_SUCCESS on success, or an INSTRUMENTS_E_* error
 *     code otherwise.
 */
instruments_error_t instruments_client_start_service(idevice_t device, instruments_client_t* client, const char* label);

/**
 * Disconnects a instruments client from the device and frees up the
 * instruments client data.
 *
 * @param client The instruments client to disconnect and free.
 *
 * @return INSTRUMENTS_E_SUCCESS on success, or INSTRUMENTS_E_INVALID_ARG
 *     if client is NULL.
 */
instruments_error_t instruments_client_free(instruments_client_t client);

instruments_error_t instruments_send_channel_request(instruments_client_t client, const char* service, uint32_t channel);

instruments_error_t instruments_config_launch_env(instruments_client_t client, uint32_t channel, const char* uia_result_path, const char* uia_script);

instruments_error_t instruments_launch_app(instruments_client_t client, uint32_t channel, const char* app_path, const char* package_name, uint64_t *val);

instruments_error_t instruments_pid_ops(instruments_client_t client, uint32_t channel, uint64_t pid,const char* ops);

instruments_error_t instruments_start_agent(instruments_client_t client, uint32_t channel, uint64_t pid);

instruments_error_t instruments_set_max(instruments_client_t client, uint32_t channel);

typedef void (*instruments_receive_cb_t)(char **std_out, void *user_data, int please_exit);

instruments_error_t instruments_start_script(instruments_client_t client, uint32_t channel, uint64_t pid, instruments_receive_cb_t callback, void* user_data , char* packagepath );
instruments_error_t instruments_start_script2(instruments_client_t client, uint32_t channel, uint64_t pid, instruments_receive_cb_t callback, void* user_data );
instruments_error_t instruments_start_script_dummy(instruments_client_t client, uint32_t channel, uint64_t pid, instruments_receive_cb_t callback, void* user_data );

instruments_error_t instruments_send_stdout(instruments_client_t client, uint32_t channel, uint32_t packet_num, const char *std_out );
instruments_error_t instruments_send_timeout(instruments_client_t client, uint32_t channel, uint32_t packet_num);

void *instruments_worker(void *arg);
void *commandline_worker(void *arg);

instruments_error_t instruments_get_pid(instruments_client_t client, uint32_t channel, const char* package_name, uint64_t *pid);
instruments_error_t instruments_wait_for_callback(instruments_client_t client);
instruments_error_t instruments_stop_script(instruments_client_t client, uint32_t channel);
instruments_error_t instruments_stop_worker(instruments_client_t client);
instruments_error_t instruments_get_response(instruments_client_t client);

#ifdef __cplusplus
}
#endif

#endif
