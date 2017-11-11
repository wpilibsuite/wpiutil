#pragma once


#include <utility>
#include <memory>
#include <string>
#include <uv.h>
#include "llvm/SmallString.h"
#include "llvm/StringRef.h"
#include "request.hpp"
#include "util.hpp"
#include "loop.hpp"


namespace uvw {


/**
 * @brief AddrInfoEvent event.
 *
 * It will be emitted by GetAddrInfoReq according with its functionalities.
 */
struct AddrInfoEvent {
    using Deleter = void(*)(addrinfo *);

    AddrInfoEvent(std::unique_ptr<addrinfo, Deleter> addr)
        : data{std::move(addr)}
    {}

    /**
     * @brief An initialized instance of `addrinfo`.
     *
     * See [getaddrinfo](http://linux.die.net/man/3/getaddrinfo) for further
     * details.
     */
    std::unique_ptr<addrinfo, Deleter> data;
};


/**
 * @brief NameInfoEvent event.
 *
 * It will be emitted by GetNameInfoReq according with its functionalities.
 */
struct NameInfoEvent {
    NameInfoEvent(const char *host, const char *serv)
        : hostname{host}, service{serv}
    {}

    /**
     * @brief A valid hostname.
     *
     * See [getnameinfo](http://linux.die.net/man/3/getnameinfo) for further
     * details.
     */
    const char * hostname;

    /**
     * @brief A valid service name.
     *
     * See [getnameinfo](http://linux.die.net/man/3/getnameinfo) for further
     * details.
     */
    const char * service;
};


/**
 * @brief The GetAddrInfoReq request.
 *
 * Wrapper for [getaddrinfo](http://linux.die.net/man/3/getaddrinfo).<br/>
 * It offers either asynchronous and synchronous access methods.
 *
 * To create a `GetAddrInfoReq` through a `Loop`, no arguments are required.
 */
class GetAddrInfoReq final: public Request<GetAddrInfoReq, uv_getaddrinfo_t> {
    static void addrInfoCallback(uv_getaddrinfo_t *req, int status, addrinfo *res) {
        auto ptr = reserve(req);

        if(status) {
            ptr->publish(ErrorEvent{status});
        } else {
            auto data = std::unique_ptr<addrinfo, void(*)(addrinfo *)>{
                res, [](addrinfo *addr){ uv_freeaddrinfo(addr); }};

            ptr->publish(AddrInfoEvent{std::move(data)});
        }
    }

    void nodeAddrInfo(const char *node, const char *service, addrinfo *hints = nullptr) {
        invoke(&uv_getaddrinfo, parent(), get(), &addrInfoCallback, node, service, hints);
    }

    auto nodeAddrInfoSync(const char *node, const char *service, addrinfo *hints = nullptr) {
        auto req = get();
        auto err = uv_getaddrinfo(parent(), req, nullptr, node, service, hints);
        auto data = std::unique_ptr<addrinfo, void(*)(addrinfo *)>{req->addrinfo, [](addrinfo *addr){ uv_freeaddrinfo(addr); }};
        return std::make_pair(!err, std::move(data));
    }

public:
    using Deleter = void(*)(addrinfo *);

    using Request::Request;

    /**
     * @brief Async [getaddrinfo](http://linux.die.net/man/3/getaddrinfo).
     * @param node Either a numerical network address or a network hostname.
     * @param hints Optional `addrinfo` data structure with additional address
     * type constraints.
     */
    void nodeAddrInfo(llvm::StringRef node, addrinfo *hints = nullptr) {
        llvm::SmallString<128> node_copy = node;
        nodeAddrInfo(node_copy.c_str(), nullptr, hints);
    }

    /**
     * @brief Sync [getaddrinfo](http://linux.die.net/man/3/getaddrinfo).
     *
     * @param node Either a numerical network address or a network hostname.
     * @param hints Optional `addrinfo` data structure with additional address
     * type constraints.
     *
     * @return A `std::pair` composed as it follows:
     * * A boolean value that is true in case of success, false otherwise.
     * * A `std::unique_ptr<addrinfo, Deleter>` containing the data requested.
     */
    std::pair<bool, std::unique_ptr<addrinfo, Deleter>>
    nodeAddrInfoSync(llvm::StringRef node, addrinfo *hints = nullptr) {
        llvm::SmallString<128> node_copy = node;
        return nodeAddrInfoSync(node_copy.c_str(), nullptr, hints);
    }

    /**
     * @brief Async [getaddrinfo](http://linux.die.net/man/3/getaddrinfo).
     * @param service Either a service name or a port number as a string.
     * @param hints Optional `addrinfo` data structure with additional address
     * type constraints.
     */
    void serviceAddrInfo(llvm::StringRef service, addrinfo *hints = nullptr) {
        llvm::SmallString<128> service_copy = service;
        nodeAddrInfo(nullptr, service_copy.c_str(), hints);
    }

    /**
     * @brief Sync [getaddrinfo](http://linux.die.net/man/3/getaddrinfo).
     *
     * @param service Either a service name or a port number as a string.
     * @param hints Optional `addrinfo` data structure with additional address
     * type constraints.
     *
     * @return A `std::pair` composed as it follows:
     * * A boolean value that is true in case of success, false otherwise.
     * * A `std::unique_ptr<addrinfo, Deleter>` containing the data requested.
     */
    std::pair<bool, std::unique_ptr<addrinfo, Deleter>>
    serviceAddrInfoSync(llvm::StringRef service, addrinfo *hints = nullptr) {
        llvm::SmallString<128> service_copy = service;
        return nodeAddrInfoSync(nullptr, service_copy.c_str(), hints);
    }

    /**
     * @brief Async [getaddrinfo](http://linux.die.net/man/3/getaddrinfo).
     * @param node Either a numerical network address or a network hostname.
     * @param service Either a service name or a port number as a string.
     * @param hints Optional `addrinfo` data structure with additional address
     * type constraints.
     */
    void addrInfo(llvm::StringRef node, llvm::StringRef service, addrinfo *hints = nullptr) {
        llvm::SmallString<128> node_copy = node;
        llvm::SmallString<128> service_copy = service;
        nodeAddrInfo(node_copy.c_str(), service_copy.c_str(), hints);
    }

    /**
     * @brief Sync [getaddrinfo](http://linux.die.net/man/3/getaddrinfo).
     *
     * @param node Either a numerical network address or a network hostname.
     * @param service Either a service name or a port number as a string.
     * @param hints Optional `addrinfo` data structure with additional address
     * type constraints.
     *
     * @return A `std::pair` composed as it follows:
     * * A boolean value that is true in case of success, false otherwise.
     * * A `std::unique_ptr<addrinfo, Deleter>` containing the data requested.
     */
    std::pair<bool, std::unique_ptr<addrinfo, Deleter>>
    addrInfoSync(llvm::StringRef node, llvm::StringRef service, addrinfo *hints = nullptr) {
        llvm::SmallString<128> node_copy = node;
        llvm::SmallString<128> service_copy = service;
        return nodeAddrInfoSync(node_copy.c_str(), service_copy.c_str(), hints);
    }
};


/**
 * @brief The GetNameInfoReq request.
 *
 * Wrapper for [getnameinfo](http://linux.die.net/man/3/getnameinfo).<br/>
 * It offers either asynchronous and synchronous access methods.
 *
 * To create a `GetNameInfoReq` through a `Loop`, no arguments are required.
 */
class GetNameInfoReq final: public Request<GetNameInfoReq, uv_getnameinfo_t> {
    static void nameInfoCallback(uv_getnameinfo_t *req, int status, const char *hostname, const char *service) {
        auto ptr = reserve(req);
        if(status) { ptr->publish(ErrorEvent{status}); }
        else { ptr->publish(NameInfoEvent{hostname, service}); }
    }

public:
    using Request::Request;

    /**
     * @brief Async [getnameinfo](http://linux.die.net/man/3/getnameinfo).
     * @param ip A valid IP address.
     * @param port A valid port number.
     * @param flags Optional flags that modify the behavior of `getnameinfo`.
     */
    template<typename I = IPv4>
    void nameInfo(llvm::StringRef ip, unsigned int port, int flags = 0) {
        llvm::SmallString<128> ip_copy = ip;
        typename details::IpTraits<I>::Type addr;
        details::IpTraits<I>::addrFunc(ip_copy.c_str(), port, &addr);
        auto saddr = reinterpret_cast<const sockaddr *>(&addr);
        invoke(&uv_getnameinfo, parent(), get(), &nameInfoCallback, saddr, flags);
    }

    /**
     * @brief Async [getnameinfo](http://linux.die.net/man/3/getnameinfo).
     * @param addr A valid instance of Addr.
     * @param flags Optional flags that modify the behavior of `getnameinfo`.
     */
    template<typename I = IPv4>
    void nameInfo(Addr addr, int flags = 0) {
        nameInfo<I>(addr.ip, addr.port, flags);
    }

    /**
     * @brief Sync [getnameinfo](http://linux.die.net/man/3/getnameinfo).
     *
     * @param ip A valid IP address.
     * @param port A valid port number.
     * @param flags Optional flags that modify the behavior of `getnameinfo`.
     *
     * @return A `std::pair` composed as it follows:
     * * A boolean value that is true in case of success, false otherwise.
     * * A `std::pair` composed as it follows:
     *   * A `const char *` containing a valid hostname.
     *   * A `const char *` containing a valid service name.
     */
    template<typename I = IPv4>
    std::pair<bool, std::pair<const char *, const char *>>
    nameInfoSync(llvm::StringRef ip, unsigned int port, int flags = 0) {
        llvm::SmallString<128> ip_copy = ip;
        typename details::IpTraits<I>::Type addr;
        details::IpTraits<I>::addrFunc(ip_copy.c_str(), port, &addr);
        auto req = get();
        auto saddr = reinterpret_cast<const sockaddr *>(&addr);
        auto err = uv_getnameinfo(parent(), req, nullptr, saddr, flags);
        return std::make_pair(!err, std::make_pair(req->host, req->service));
    }

    /**
     * @brief Sync [getnameinfo](http://linux.die.net/man/3/getnameinfo).
     *
     * @param addr A valid instance of Addr.
     * @param flags Optional flags that modify the behavior of `getnameinfo`.
     *
     * @return A `std::pair` composed as it follows:
     * * A boolean value that is true in case of success, false otherwise.
     * * A `std::pair` composed as it follows:
     *   * A `const char *` containing a valid hostname.
     *   * A `const char *` containing a valid service name.
     */
    template<typename I = IPv4>
    std::pair<bool, std::pair<const char *, const char *>>
    nameInfoSync(Addr addr, int flags = 0) {
        return nameInfoSync<I>(addr.ip, addr.port, flags);
    }
};


}
