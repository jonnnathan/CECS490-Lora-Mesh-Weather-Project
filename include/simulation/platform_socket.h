/**
 * @file platform_socket.h
 * @brief Cross-platform socket abstraction for simulation.
 *
 * Provides a unified interface for socket operations across:
 * - Windows (Winsock2)
 * - POSIX systems (Linux, macOS)
 *
 * @note This file is only used in native (non-Arduino) builds.
 */

#ifndef PLATFORM_SOCKET_H
#define PLATFORM_SOCKET_H

#ifndef ARDUINO  // Only for native builds

// ============================================================================
// Platform-Specific Includes
// ============================================================================

#ifdef _WIN32
    // Windows
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")

    typedef SOCKET socket_t;
    #define SOCKET_INVALID INVALID_SOCKET
    #define SOCKET_ERROR_VAL SOCKET_ERROR
    #define CLOSE_SOCKET(s) closesocket(s)
    #define GET_SOCKET_ERROR() WSAGetLastError()

#else
    // POSIX (Linux, macOS)
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <string.h>

    typedef int socket_t;
    #define SOCKET_INVALID (-1)
    #define SOCKET_ERROR_VAL (-1)
    #define CLOSE_SOCKET(s) close(s)
    #define GET_SOCKET_ERROR() errno

#endif

#include <cstdint>

namespace platform {

// ============================================================================
// Initialization / Cleanup
// ============================================================================

/**
 * @brief Initialize the socket subsystem.
 *
 * Must be called before any socket operations.
 * On Windows, initializes Winsock. On POSIX, this is a no-op.
 *
 * @return true if initialization succeeded
 */
inline bool initSockets() {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return result == 0;
#else
    return true;  // No initialization needed on POSIX
#endif
}

/**
 * @brief Cleanup the socket subsystem.
 *
 * Should be called when done with sockets.
 * On Windows, calls WSACleanup(). On POSIX, this is a no-op.
 */
inline void cleanupSockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// ============================================================================
// Socket Configuration
// ============================================================================

/**
 * @brief Set socket to non-blocking mode.
 *
 * @param sock The socket to configure
 * @return true if successful
 */
inline bool setNonBlocking(socket_t sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

/**
 * @brief Enable address reuse on socket.
 *
 * Allows binding to a port that was recently used.
 *
 * @param sock The socket to configure
 * @return true if successful
 */
inline bool setReuseAddr(socket_t sock) {
    int optval = 1;
#ifdef _WIN32
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                      (const char*)&optval, sizeof(optval)) == 0;
#else
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                      &optval, sizeof(optval)) == 0;
#endif
}

/**
 * @brief Set receive timeout on socket.
 *
 * @param sock The socket to configure
 * @param timeoutMs Timeout in milliseconds (0 = no timeout)
 * @return true if successful
 */
inline bool setReceiveTimeout(socket_t sock, uint32_t timeoutMs) {
#ifdef _WIN32
    DWORD timeout = timeoutMs;
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                      (const char*)&timeout, sizeof(timeout)) == 0;
#else
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                      &tv, sizeof(tv)) == 0;
#endif
}

// ============================================================================
// Multicast Support
// ============================================================================

/**
 * @brief Join a multicast group.
 *
 * @param sock The socket to configure
 * @param multicastAddr Multicast group address (e.g., "239.0.0.1")
 * @return true if successful
 */
inline bool joinMulticastGroup(socket_t sock, const char* multicastAddr) {
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(multicastAddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

#ifdef _WIN32
    return setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                      (const char*)&mreq, sizeof(mreq)) == 0;
#else
    return setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                      &mreq, sizeof(mreq)) == 0;
#endif
}

/**
 * @brief Leave a multicast group.
 *
 * @param sock The socket to configure
 * @param multicastAddr Multicast group address
 * @return true if successful
 */
inline bool leaveMulticastGroup(socket_t sock, const char* multicastAddr) {
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(multicastAddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

#ifdef _WIN32
    return setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                      (const char*)&mreq, sizeof(mreq)) == 0;
#else
    return setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                      &mreq, sizeof(mreq)) == 0;
#endif
}

/**
 * @brief Set multicast TTL (time-to-live / hop count).
 *
 * @param sock The socket to configure
 * @param ttl TTL value (1 = local network only)
 * @return true if successful
 */
inline bool setMulticastTTL(socket_t sock, uint8_t ttl) {
#ifdef _WIN32
    DWORD ttlVal = ttl;
    return setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
                      (const char*)&ttlVal, sizeof(ttlVal)) == 0;
#else
    unsigned char ttlVal = ttl;
    return setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
                      &ttlVal, sizeof(ttlVal)) == 0;
#endif
}

/**
 * @brief Enable/disable multicast loopback.
 *
 * When enabled, packets sent to multicast group are received locally.
 *
 * @param sock The socket to configure
 * @param enable true to enable loopback
 * @return true if successful
 */
inline bool setMulticastLoopback(socket_t sock, bool enable) {
#ifdef _WIN32
    DWORD loop = enable ? 1 : 0;
    return setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,
                      (const char*)&loop, sizeof(loop)) == 0;
#else
    unsigned char loop = enable ? 1 : 0;
    return setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,
                      &loop, sizeof(loop)) == 0;
#endif
}

// ============================================================================
// Error Handling
// ============================================================================

/**
 * @brief Check if last error was "would block" (non-blocking socket).
 *
 * @return true if the last socket operation would have blocked
 */
inline bool wouldBlock() {
#ifdef _WIN32
    int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

/**
 * @brief Get human-readable error message for last socket error.
 *
 * @param buffer Buffer to store error message
 * @param bufferSize Size of buffer
 */
inline void getErrorMessage(char* buffer, size_t bufferSize) {
#ifdef _WIN32
    int err = WSAGetLastError();
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buffer, (DWORD)bufferSize, NULL);
#else
    strncpy(buffer, strerror(errno), bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
#endif
}

}  // namespace platform

#endif // ARDUINO
#endif // PLATFORM_SOCKET_H
