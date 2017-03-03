/*
 * ping.h
 *
 *  Created on: 28/02/2017
 *      Author: vlad
 */

#ifndef PING_H_
#define PING_H_

//
// ping.cpp
// ~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <istream>
#include <iostream>
#include <ostream>

#include "icmp_header.hpp"
#include "ipv4_header.hpp"

using boost::asio::ip::icmp;
using boost::asio::deadline_timer;
namespace posix_time = boost::posix_time;

class pinger
{
public:
  pinger(std::shared_ptr<boost::asio::io_service> io_service, const char* destination)
    : resolver_(*io_service), socket_(*io_service, icmp::v4()),
      timer_(*io_service), sequence_number_(0), num_replies_(0),
      io_service_(io_service)
  {
    icmp::resolver::query query(icmp::v4(), destination, "");
    destination_ = *resolver_.resolve(query);

    start_send();
    start_receive();
  }
  std::size_t getReplies() { return num_replies_; }

private:
  void start_send();
  void handle_timeout();
  void start_receive();
  void handle_receive(std::size_t length);
  unsigned short get_identifier();

  icmp::resolver resolver_;
  icmp::endpoint destination_;
  icmp::socket socket_;
  deadline_timer timer_;
  unsigned short sequence_number_;
  posix_time::ptime time_sent_;
  boost::asio::streambuf reply_buffer_;
  std::size_t num_replies_;
  std::shared_ptr<boost::asio::io_service> io_service_;
};

#endif /* PING_H_ */
