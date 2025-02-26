// net.h - support for networking

#pragma once

extern "C" {
#include <base/stddef.h>
#include <net/ip.h>
#include <runtime/tcp.h>
#include <runtime/udp.h>
}

#include <cstring>
#include <numeric>
#include <span>

namespace rt {
namespace detail {

template <bool Scatter, std::size_t Extent>
inline void general_scatter_gather(uint8_t *buf, std::size_t offset,
                                   std::span<const iovec, Extent> iov) {
  if constexpr (Extent) {
    auto iovec = iov.front();
    if constexpr (Scatter) {
      memcpy(iovec.iov_base, &buf[offset], iovec.iov_len);
    } else {
      memcpy(&buf[offset], iovec.iov_base, iovec.iov_len);
    }
    general_scatter_gather<Scatter, Extent - 1>(
        buf, offset + iovec.iov_len, iov.template last<Extent - 1>());
  }
}

template <std::size_t Extent>
inline void scatter_to(uint8_t *buf, std::span<const iovec, Extent> iov) {
  static_assert(Extent != std::dynamic_extent);
  general_scatter_gather<true, Extent>(buf, 0, iov);
}

template <std::size_t Extent>
inline void gather_from(uint8_t *buf, std::span<const iovec, Extent> iov) {
  static_assert(Extent != std::dynamic_extent);
  general_scatter_gather<false, Extent>(buf, 0, iov);
}

} // detail

class NetConn {
 public:
   virtual ~NetConn(){};
   virtual ssize_t Read(void *buf, size_t len, bool nt = false,
                        bool poll = false) = 0;
   virtual ssize_t Write(const void *buf, size_t len, bool nt = false,
                         bool poll = false) = 0;
};

// UDP Connections.
class UdpConn : public NetConn {
 public:
  // The maximum possible payload size (with the maximum MTU).
  static constexpr size_t kMaxPayloadSize = UDP_MAX_PAYLOAD_SIZE;

  ~UdpConn() { udp_close(c_); }

  // Creates a UDP connection between a local and remote address.
  static UdpConn *Dial(netaddr laddr, netaddr raddr) {
    udpconn_t *c;
    int ret = udp_dial(laddr, raddr, &c);
    if (ret) return nullptr;
    return new UdpConn(c);
  }

  // Creates a UDP connection that receives all packets on a local port.
  static UdpConn *Listen(netaddr laddr) {
    udpconn_t *c;
    int ret = udp_listen(laddr, &c);
    if (ret) return nullptr;
    return new UdpConn(c);
  }

  // Gets the MTU-limited payload size.
  static size_t PayloadSize() {
    return static_cast<size_t>(udp_payload_size);
  }

  // Gets the local UDP address.
  netaddr LocalAddr() const { return udp_local_addr(c_); }
  // Gets the remote UDP address.
  netaddr RemoteAddr() const { return udp_remote_addr(c_); }

  // Adjusts the length of buffer limits.
  int SetBuffers(int read_mbufs, int write_mbufs) {
    return udp_set_buffers(c_, read_mbufs, write_mbufs);
  }

  // Reads a datagram and gets from remote address.
  ssize_t ReadFrom(void *buf, size_t len, netaddr *raddr) {
    return udp_read_from(c_, buf, len, raddr);
  }

  // Writes a datagram and sets to remote address.
  ssize_t WriteTo(const void *buf, size_t len, const netaddr *raddr) {
    return udp_write_to(c_, buf, len, raddr);
  }

  // Reads a datagram.
  ssize_t Read(void *buf, size_t len, bool nt = false, bool poll = false) {
    BUG_ON(nt || poll); // Not implemented.
    return udp_read(c_, buf, len);
  }

  // Writes a datagram.
  ssize_t Write(const void *buf, size_t len, bool nt = false, bool poll = false) {
    BUG_ON(nt || poll); // Not implemented.
    return udp_write(c_, buf, len);
  }

  // Shutdown the socket (no more receives).
  void Shutdown() { udp_shutdown(c_); }

 private:
  UdpConn(udpconn_t *c) : c_(c) {}

  // disable move and copy.
  UdpConn(const UdpConn &) = delete;
  UdpConn &operator=(const UdpConn &) = delete;

  udpconn_t *c_;
};

// TCP connections.
class TcpConn : public NetConn {
  friend class TcpQueue;

 public:
  ~TcpConn() { tcp_close(c_); }

  // Creates a TCP connection between a local and remote address.
  static TcpConn *Dial(netaddr laddr, netaddr raddr,
                       uint8_t dscp = DEFAULT_DSCP, bool poll = false) {
    tcpconn_t *c;
    int ret = tcp_dial_dscp(laddr, raddr, &c, dscp, poll);
    if (ret) return nullptr;
    return new TcpConn(c);
  }

  // Creates a TCP connection with affinity to a CPU index.
  static TcpConn *DialAffinity(unsigned int cpu, netaddr raddr,
                               uint8_t dscp = DEFAULT_DSCP) {
    tcpconn_t *c;
    int ret = tcp_dial_affinity_dscp(cpu, raddr, &c, dscp);
    if (ret) return nullptr;
    return new TcpConn(c);
  }

  // Creates a new TCP connection with affinity to another TCP connection.
  static TcpConn *DialAffinity(TcpConn *cin, netaddr raddr,
                               uint8_t dscp = DEFAULT_DSCP) {
    tcpconn_t *c;
    int ret = tcp_dial_conn_affinity_dscp(cin->c_, raddr, &c, dscp);
    if (ret) return nullptr;
    return new TcpConn(c);
  }

  // Gets the local TCP address.
  netaddr LocalAddr() const { return tcp_local_addr(c_); }
  // Gets the remote TCP address.
  netaddr RemoteAddr() const { return tcp_remote_addr(c_); }

  // Reads from the TCP stream.
  ssize_t Read(void *buf, size_t len, bool nt = false, bool poll = false) {
    return __tcp_read(c_, buf, len, nt, poll);
  };
  // Writes to the TCP stream.
  ssize_t Write(const void *buf, size_t len, bool nt = false,
                bool poll = false) {
    return __tcp_write(c_, buf, len, nt, poll);
  }
  // Reads a vector from the TCP stream.
  ssize_t Readv(const iovec *iov, int iovcnt, bool nt = false,
                bool poll = false) {
    return __tcp_readv(c_, iov, iovcnt, nt, poll);
  }
  // Writes a vector to the TCP stream.
  ssize_t Writev(const iovec *iov, int iovcnt, bool nt = false,
                 bool poll = false) {
    return __tcp_writev(c_, iov, iovcnt, nt, poll);
  }

  // Reads exactly @len bytes from the TCP stream.
  ssize_t ReadFull(void *buf, size_t len, bool nt = false, bool poll = false) {
    char *pos = reinterpret_cast<char *>(buf);
    size_t n = 0;
    while (n < len) {
      ssize_t ret = Read(pos + n, len - n, nt, poll);
      if (ret <= 0) return ret;
      n += ret;
    }
    assert(n == len);
    return n;
  }

  // Writes exactly @len bytes to the TCP stream.
  ssize_t WriteFull(const void *buf, size_t len, bool nt = false,
                    bool poll = false) {
    const char *pos = reinterpret_cast<const char *>(buf);
    size_t n = 0;
    while (n < len) {
      ssize_t ret = Write(pos + n, len - n, nt, poll);
      if (ret < 0) return ret;
      assert(ret > 0);
      n += ret;
    }
    assert(n == len);
    return n;
  }

  // Reads exactly a vector of bytes from the TCP stream.
  template <std::size_t Extent>
  ssize_t ReadvFull(std::span<const iovec, Extent> iov, bool nt = false,
                    bool poll = false) {
    if constexpr (Extent != std::dynamic_extent) {
      if constexpr (iov.size() == 1) {
        return ReadFull(iov[0].iov_base, iov[0].iov_len, nt, poll);
      }
      auto total_len = std::accumulate(
          iov.begin(), iov.end(), static_cast<std::size_t>(0),
          [](std::size_t s, const iovec a) { return s + a.iov_len; });
      if (total_len <= kIOVCopyThresh) {
        auto ret = ReadFull(buf_, total_len, nt, poll);
        detail::scatter_to(buf_, iov);
        return ret;
      }
    }
    return ReadvFullRaw(iov, nt, poll);
  }

  // Writes exactly a vector of bytes to the TCP stream.
  template <std::size_t Extent>
  ssize_t WritevFull(std::span<const iovec, Extent> iov, bool nt = false,
                     bool poll = false) {
    if constexpr (Extent != std::dynamic_extent) {
      if constexpr (iov.size() == 1) {
        return WriteFull(iov[0].iov_base, iov[0].iov_len, nt, poll);
      }
      auto total_len = std::accumulate(
          iov.begin(), iov.end(), static_cast<std::size_t>(0),
          [](std::size_t s, const iovec a) { return s + a.iov_len; });
      if (total_len <= kIOVCopyThresh) {
        detail::gather_from(buf_, iov);
        return WriteFull(buf_, total_len, nt, poll);
      }
    }
    return WritevFullRaw(iov, nt, poll);
  }

  // Reads exactly a vector of bytes from the TCP stream.
  ssize_t ReadvFull(const iovec *iov, int iovcnt, bool nt = false,
                    bool poll = false) {
    return ReadvFull(std::span{iov, static_cast<size_t>(iovcnt)}, nt, poll);
  }

  // Writes exactly a vector of bytes to the TCP stream.
  ssize_t WritevFull(const iovec *iov, int iovcnt, bool nt = false,
                     bool poll = false) {
    return WritevFull(std::span{iov, static_cast<size_t>(iovcnt)}, nt, poll);
  }

  // If the connection has pending data to read.
  bool HasPendingDataToRead() { return tcp_has_pending_data_to_read(c_); }

  // Block until there is any data to read.
  // Return false if any exception happens.
  bool WaitForRead() { return tcp_wait_for_read(c_); }

  // Gracefully shutdown the TCP connection.
  int Shutdown(int how) { return tcp_shutdown(c_, how); }
  // Ungracefully force the TCP connection to shutdown.
  void Abort() { tcp_abort(c_); }

 private:
  constexpr static std::size_t kIOVCopyThresh = 128;

  TcpConn(tcpconn_t *c) : c_(c) {}

  // disable move and copy.
  TcpConn(const TcpConn &) = delete;
  TcpConn &operator=(const TcpConn &) = delete;

  ssize_t WritevFullRaw(std::span<const iovec> iov, bool nt, bool poll);
  ssize_t ReadvFullRaw(std::span<const iovec> iov, bool nt, bool poll);

  tcpconn_t *c_;
  uint8_t buf_[kIOVCopyThresh];
};

// TCP listener queues.
class TcpQueue {
 public:
  ~TcpQueue() { tcp_qclose(q_); }

  // Creates a TCP listener queue.
  static TcpQueue *Listen(netaddr laddr, int backlog,
                          uint8_t dscp = DEFAULT_DSCP) {
    tcpqueue_t *q;
    int ret = tcp_listen_dscp(laddr, backlog, &q, dscp);
    if (ret) return nullptr;
    return new TcpQueue(q);
  }

  // Accept a connection from the listener queue.
  TcpConn *Accept() {
    tcpconn_t *c;
    int ret = tcp_accept(q_, &c);
    if (ret) return nullptr;
    return new TcpConn(c);
  }

  // Shutdown the listener queue; any blocked Accept() returns a nullptr.
  void Shutdown() { tcp_qshutdown(q_); }

 private:
  TcpQueue(tcpqueue_t *q) : q_(q) {}

  // disable move and copy.
  TcpQueue(const TcpQueue &) = delete;
  TcpQueue &operator=(const TcpQueue &) = delete;

  tcpqueue_t *q_;
};

}  // namespace rt
