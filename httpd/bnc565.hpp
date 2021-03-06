// -*- C++ -*-
/**************************************************************************
** Copyright (C) 2010-2011 Toshinobu Hondo, Ph.D.
** Copyright (C) 2013-2015 MS-Cheminformatics LLC
*
** Contact: toshi.hondo@scienceliaison.com
**
** Commercial Usage
**
** Licensees holding valid ScienceLiaison commercial licenses may use this
** file in accordance with the ScienceLiaison Commercial License Agreement
** provided with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and ScienceLiaison.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.TXT included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
**************************************************************************/

#pragma once

#include "dgprotocols.hpp"
#include <atomic>
#include <memory>
#include <utility>
#include <cstdint>
#include <thread>
#include <vector>
#include <functional>
#include <condition_variable>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/signals2.hpp>

class serialport;

namespace dg {

    class bnc565 { // : public std::enable_shared_from_this< bnc565 > {
        bnc565();
    public:
        ~bnc565();

        enum DeviceType { NONE, HELIO, DE0 };

        static bnc565 * instance();

        operator bool () const;
        bool state() const;

        void setSerialDevice( const char * tty );
        const std::string& serialDevice() const;

        void setPulse( uint32_t channel, const std::pair< double, double >& );
        std::pair<double, double> pulse( uint32_t channel ) const;
        
        typedef boost::signals2::signal< void( size_t ) > tick_handler_t;

        boost::signals2::connection register_handler( const tick_handler_t::slot_type& );

        const std::string& idn() const;

        void commit( const dg::protocols<>& );

        bool fetch( dg::protocols<>& );

        inline boost::asio::io_service& io_service() { return io_service_; }

        //bool peripheral_initialize();
        bool peripheral_terminate();
        bool peripheral_query_device_data( bool verbose = false );

        void setInterval( double );
        double interval() const;

        bool xsend( const char * data, std::string& );
        bool initialize( const std::string&, int baud );
        bool reset();
        bool switch_connect( bool, std::string& );
        
    private:
        DeviceType deviceType_;
        size_t tick_;
        tick_handler_t handler_; // tick handler
        boost::asio::io_service io_service_;
        boost::asio::steady_timer timer_; // interrupts simulator
        std::vector< std::thread > threads_;
        void on_timer( const boost::system::error_code& ec );
        //
        std::unique_ptr< serialport > usb_;
        std::condition_variable cond_;
        std::mutex mutex_;
        std::mutex xlock_;
        std::string receiving_data_;
        std::vector< std::string > que_;
        std::string ttyname_;
        int baud_;
        std::atomic< size_t > xsend_timeout_c_;
        std::atomic< size_t > reply_timeout_c_;

        dg::protocols<> protocols_;

        bool _xsend( const char * data, std::string& );
        bool _xsend( const char * data, std::string&, const std::string& expect, size_t ntry );
        void handle_receive( const char * data, std::size_t length );
    };
}

