/**
 *  Copyright Notice:
 *  Copyright 2021-2022 DMTF. All rights reserved.
 *  License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/spdm-emu/blob/main/LICENSE.md
 **/

#include "spdm_device_validator_sample.h"

#ifndef _WIN32
#include <fcntl.h>
#endif /* !_WIN32 */

uint8_t m_receive_buffer[LIBSPDM_MAX_SENDER_RECEIVER_BUFFER_SIZE];

extern SOCKET m_socket;
#ifndef _WIN32
extern int m_device_fd;
#endif /* !_WIN32 */

extern void *m_spdm_context;
extern void *m_scratch_buffer;

void *spdm_client_init(void);

libspdm_return_t pci_doe_init_request(void);

bool communicate_platform_data(SOCKET socket, uint32_t command,
                               const uint8_t *send_buffer, size_t bytes_to_send,
                               uint32_t *response,
                               size_t *bytes_to_receive,
                               uint8_t *receive_buffer);

bool platform_client_routine(uint16_t port_number)
{
    SOCKET platform_socket = INVALID_SOCKET;
    bool result;
    uint32_t response;
    size_t response_size;
    libspdm_return_t status;

#ifndef _WIN32
    if (m_use_device) {
        m_device_fd = open(m_device_path, O_RDWR);
        if (m_device_fd < 0) {
            EMU_ERR("Failed to open device %s - %d\n", m_device_path, errno);
            return false;
        }
        EMU_LOG("Opened SPDM device %s\n", m_device_path);
    }
    else
#endif /* !_WIN32 */
    {
        result = init_client(&platform_socket, port_number);
        if (!result) {
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }

        m_socket = platform_socket;
    }

    if (!m_use_device &&
        m_use_transport_layer != SOCKET_TRANSPORT_TYPE_NONE) {
        response_size = sizeof(m_receive_buffer);
        result = communicate_platform_data(
            platform_socket,
            SOCKET_SPDM_COMMAND_TEST,
            (uint8_t *)"Client Hello!",
            sizeof("Client Hello!"), &response,
            &response_size, m_receive_buffer);
        if (!result) {
            goto done;
        }
    }

    if (m_use_transport_layer == SOCKET_TRANSPORT_TYPE_PCI_DOE) {
        status = pci_doe_init_request ();
        if (LIBSPDM_STATUS_IS_ERROR(status)) {
            printf("pci_doe_init_request - %x\n", (uint32_t)status);
            goto done;
        }
    }

    /* Do test - begin*/

    m_spdm_context = spdm_client_init ();
    spdm_responder_conformance_test (m_spdm_context, &m_spdm_responder_validator_config);
    if (m_spdm_context != NULL) {
        free(m_spdm_context);
    }

    /* Do test - end*/

done:
#ifndef _WIN32
    if (m_use_device) {
        if (m_device_fd >= 0) {
            close(m_device_fd);
            m_device_fd = -1;
        }
        return true;
    }
#endif /* !_WIN32 */

    response_size = 0;
    result = communicate_platform_data(
        platform_socket, SOCKET_SPDM_COMMAND_SHUTDOWN - m_exe_mode,
        NULL, 0, &response, &response_size, NULL);

    closesocket(platform_socket);

#ifdef _WIN32
    WSACleanup();
#endif

    return true;
}

int main(int argc, char *argv[])
{
    printf("%s version 0.1\n", "spdm_device_validator_sample");
    srand((unsigned int)time(NULL));

    process_args("spdm_device_validator_sample", argc, argv);

    /* Use custom port if provided, otherwise default to 2323 */
    const uint16_t port = (m_custom_port != 0) ? m_custom_port : DEFAULT_SPDM_PLATFORM_PORT;
    platform_client_routine(port);
    printf("Client stopped\n");

    close_pcap_packet_file();
    return 0;
}
