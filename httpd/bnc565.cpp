// -*- C++ -*-
/**************************************************************************
** Copyright (C) 2017 Toshinobu Hondo, Ph.D.
** Copyright (C) 2017 MS-Cheminformatics LLC
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

#include "bnc565.hpp"
#include "log.hpp"
#include "serialport.hpp"
#include "dgprotocols.hpp" // handle json
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <array>
#include <atomic>
#include <fcntl.h>

#if !defined WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sstream>
#include <iostream>
#include <cstdint>

extern int __verbose_level__;
extern bool __debug_mode__;

namespace dg {

    std::atomic_flag lock = ATOMIC_FLAG_INIT;

    const uint32_t resolution = 10;

    inline uint32_t

    seconds_to_device( const double& t ) {
        return static_cast< uint32_t >( ( t * 1.0e9 ) / resolution + 0.5 );
    }

    inline double
    device_to_seconds( const uint32_t& t ) {
        return ( t * resolution ) * 1.0e-9;
    }
}

using namespace dg;

bnc565 *
bnc565::instance()
{
    static bnc565 __instance;
    return &__instance;
}

bnc565::operator bool () const
{
    return usb_->is_open();
}

bnc565::~bnc565()
{
    timer_.cancel();
    
    io_service_.stop();

    for ( auto& t: threads_ )
        t.join();
    
    log() << "memmap closed";
}

bnc565::bnc565() : timer_( io_service_ )
                 , tick_( 0 )
                 , deviceType_( NONE )
                 , deviceModelNumber_( 0 )
                 , deviceRevision_( -1 )
                 , states_( 8 )
{
    boost::system::error_code ec;
    timer_.expires_from_now( std::chrono::milliseconds( 1000 ) );
    timer_.async_wait( [this]( const boost::system::error_code& ec ){ on_timer(ec); } );

    threads_.push_back( std::thread( [=]{ io_service_.run(); } ) );
}

void
bnc565::on_timer( const boost::system::error_code& ec )
{
    if ( !ec ) {
        ++tick_;
        handler_( tick_ );
    }
    timer_.expires_from_now( std::chrono::milliseconds( 1000 ) );
    timer_.async_wait( [this]( const boost::system::error_code& ec ){ on_timer(ec); } );    
}

boost::signals2::connection
bnc565::register_handler( const tick_handler_t::slot_type & subscriber )
{
    return handler_.connect( subscriber );
}

std::pair<double, double>
bnc565::pulse( uint32_t channel ) const
{
    return std::make_pair(0, 0);
}

void
bnc565::setPulse( uint32_t channel, const std::pair<double, double>& t )
{
}

void
bnc565::commit( const dg::protocols<>& d )
{
    if ( usb_->is_open() ) {

        const auto& p = *d.begin();
        std::string reply;
        
        for ( int ch  = 1; ch <= p.size; ++ch ) {

            const auto& v = p[ ch - 1 ];
            
            _xsend( (boost::format(":PULSE%1%:STATE ON\r\n") % ch).str().c_str(), reply, "ok", 10 );
            
            if ( std::get< 2 >( v ) )
                _xsend( (boost::format(":PULSE%1%:POL INV\r\n") % ch).str().c_str(), reply, "ok", 10 );
            else
                _xsend( (boost::format(":PULSE%1%:POL NORM\r\n") % ch).str().c_str(), reply, "ok", 10 );

            _xsend( (boost::format(":PULSE%1%:DELAY %E\r\n") % ch % std::get< 0 >( v ) ).str().c_str(), reply, "ok", 10 );
            _xsend( (boost::format(":PULSE%1%:WIDTH %E\r\n") % ch % std::get< 1 >( v ) ).str().c_str(), reply, "ok", 10 );
            
        }
        _xsend( ":SYST:KLOCK OFF\r\n", reply, "ok", 10 );  // off keypad lock
        _xsend( ":PULSE0:STATE ON\r\n", reply, "ok", 10 ); // start trigger        
        
    }
}

bool
bnc565::fetch( dg::protocols<>& d )
{
    if ( usb_->is_open() ) {
        std::string reply;

        if ( _xsend( "*IDN?\r\n", reply ) && reply[0] != '?' ) // identify
            d.setIdn( reply );
        
        if ( _xsend( ":INST:FULL?\r\n", reply ) && reply[0] != '?' )
            d.setFull( reply );
        
        if ( _xsend( ":PULSE0:STATE?\r\n", reply ) ) {
            if ( reply[0] != '?' ) {
                try {
                    d.setState( boost::lexical_cast<int>(reply) );
                } catch ( std::exception& ex ) {
                    log( log::ERR ) << boost::format( "%1%:%2% %3% (%4%)" ) % __FILE__ % __LINE__ % ex.what() % reply;
                }
            }
        }

        auto& protocol = *d.begin();
    
        for ( int i = 0; i < int(states_.size()); ++i ) {
            try {
                if ( _xsend( ( boost::format( ":PULSE%1%:STATE?\r\n" ) % (i+1) ).str().c_str(), reply ) ) {
                    int value = boost::lexical_cast<int>(reply);
                    std::get< 2 >( protocol[ i ] ) = ( std::get< 2 >( protocol[ i ] ) & 0xffff0000 ) | ( value << 16 );
                }
                if ( _xsend( ( boost::format( ":PULSE%1%:WIDTH?\r\n" ) % (i+1) ).str().c_str(), reply ) ) {
                    double value = boost::lexical_cast<double>(reply);
                    std::get< 1 >( protocol[ i ] ) = value;
                }
                if ( _xsend( ( boost::format( ":PULSE%1%:DELAY?\r\n" ) % (i+1) ).str().c_str(), reply ) ) {
                    double value = boost::lexical_cast<double>(reply);
                    std::get< 0 >( protocol[ i ] ) = value;
                }
            } catch ( boost::bad_lexical_cast& ex ) {
                log( log::ERR ) << boost::format( "%1%:%2% %3% (%4%)" ) % __FILE__ % __LINE__ % ex.what() % reply;
            }
        }
    } else {
        // fill debug data
        d.setIdn( "debug::IDN" );
        d.setFull( "debug::inst::full" );

        auto& protocol = *d.begin();
        for ( int i = 0; i < protocol.size; ++i ) {
            std::get< 0 >( protocol[ i ] ) = i * 1.0e-6 + 0.1e-6; // 1.1us
            std::get< 1 >( protocol[ i ] ) = (i + 1) * 0.10 * 1.0e-6;  // 100ns
            std::get< 2 >( protocol[ i ] ) = ( i & 01 ) ? true : false;
        }
    }

    return true;
}

uint32_t
bnc565::revision_number() const
{
    return deviceRevision_;
}

const std::string&
bnc565::serialDevice() const
{
    return ttyname_;
}

bool
bnc565::peripheral_terminate()
{
    if ( usb_ ) {
        usb_->close();
        usb_.reset();
    }

    io_service_.stop();

    for ( auto& t: threads_ )
        t.join();

    threads_.clear();

    return true;
}

bool
bnc565::peripheral_query_device_data( bool verbose )
{
    std::lock_guard< std::mutex > lock( xlock_ );

    std::string reply;

    if ( _xsend( "*IDN?\r\n", reply ) && reply[0] != '?' ) { // identify
        idn_ = reply;
        protocols_.setIdn( reply );
    }

    if ( verbose )
        std::cout << "*IDN? : " << reply << std::endl;

    if ( _xsend( ":INST:FULL?\r\n", reply ) && reply[0] != '?' ) {
        inst_full_ = reply;
        protocols_.setFull( reply );
    }
    
    if ( verbose )
        std::cout << "INST:FULL? : << reply " << std::endl;

    // To status (on|off)
    if ( _xsend( ":PULSE0:STATE?\r\n", reply ) ) {
        if ( reply[0] != '?' ) {
            try {
                int value = boost::lexical_cast<int>(reply);
                state0_ = value;
                protocols_.setState( value );
            } catch ( std::exception& ex ) {
                log( log::ERR ) << boost::format( "%1%:%2% %3% (%4%)" ) % __FILE__ % __LINE__ % ex.what() % reply;
            }
        }
    }
    if ( verbose )
        std::cout << ":PULSE0:STATE? : " << reply << std::endl;

    auto& protocol = *protocols_.begin();
    
    for ( int i = 0; i < int(states_.size()); ++i ) {
        const char * loc = "";
        try {
            if ( _xsend( ( boost::format( ":PULSE%1%:STATE?\r\n" ) % (i+1) ).str().c_str(), reply ) ) {
                loc = "STATE";
                int value = boost::lexical_cast<int>(reply);
                states_[ i ].state = value;
                std::get< 2 >( protocol[ i ] ) = ( std::get< 2 >( protocol[ i ] ) & 0xffff0000 ) | ( value << 16 );
            }
            if ( _xsend( ( boost::format( ":PULSE%1%:WIDTH?\r\n" ) % (i+1) ).str().c_str(), reply ) ) {
                loc = "WIDTH";
                double value = boost::lexical_cast<double>(reply);
                states_[ i ].width = value;
                std::get< 1 >( protocol[ i ] ) = value;
            }
            if ( _xsend( ( boost::format( ":PULSE%1%:DELAY?\r\n" ) % (i+1) ).str().c_str(), reply ) ) {
                loc = "DELAY";
                double value = boost::lexical_cast<double>(reply);
                states_[ i ].delay = value;
                std::get< 0 >( protocol[ i ] ) = value;
            }
            if ( verbose )
                std::cout << boost::format( ":PULSE%1%" ) % (i+1)
                          << boost::format( ", STATE=%1%" ) % states_[ i ].state
                          << boost::format( ", WIDTH=%1%" ) % states_[ i ].width
                          << boost::format( ", DELAY=%1%" ) % states_[ i ].delay
                          << std::endl;
        } catch ( boost::bad_lexical_cast& ex ) {
            log( log::ERR ) << boost::format( "%1%:%2% %3% (%4%) %5%" ) % __FILE__ % __LINE__ % ex.what() % reply % loc;
        }
    }

    return true;
}

///
void
bnc565::handle_receive( const char * data, std::size_t length )
{
    std::lock_guard< std::mutex > lock( mutex_ );

    while ( length-- ) {
        if ( std::isprint( *data ) ) {
            receiving_data_ += (*data);
        } else if ( *data == '\r' ) {
            ;
        } else if ( *data == '\n' ) {
            que_.push_back( receiving_data_ );
            receiving_data_.clear();
            cond_.notify_one();
        }
        ++data;
    }
}

///
bool
bnc565::xsend( const char * data, std::string& reply )
{
    bool res = _xsend( data, reply );

    std::string text( data );
    std::size_t pos = text.find_first_of( "\r" );
    if ( pos != std::string::npos )
        text.replace( text.begin() + pos, text.end(), "" );

    return res;
}

bool
bnc565::_xsend( const char * data, std::string& reply )
{
    reply.clear();

    if ( usb_ ) {

        std::unique_lock< std::mutex > lock( mutex_ );

        if ( usb_->write( data, std::strlen( data ), 20000 ) ) { // write with timeout(us)
            xsend_timeout_c_ = 0;

            if ( cond_.wait_for( lock, std::chrono::microseconds( 200000 ) ) != std::cv_status::timeout ) {
                reply_timeout_c_ = 0;
                if ( que_.empty() ) {
                    std::cout << "Error: cond.wait return with empty que"; 
                    return false;
                }

                reply = que_.front();
                if ( que_.size() > 1 ) {
                    std::cout << "got " << que_.size() << " replies" << std::endl;
                    for ( const auto& s: que_ )
                        std::cout << "\t" << s << std::endl;
                }
                que_.clear();
                return true;
            } else {
                reply_timeout_c_++;
            }
        } else {
            xsend_timeout_c_++;
        }

        return false;
    }
    // debug
    std::cout << __FILE__ << ":" << __LINE__ << " " << data << std::endl;
    return false;
}

bool
bnc565::_xsend( const char * data, std::string& reply, const std::string& expect, size_t ntry )
{
    while ( ntry-- ) {
        if ( _xsend( data, reply ) && reply == expect )
            return true;
    }
    return false;
}

bool
bnc565::initialize( const std::string& ttyname, int baud )
{
    xsend_timeout_c_ = 0;
    reply_timeout_c_ = 0;

    usb_ = std::make_unique< serialport >( io_service_ );

    if ( ! usb_->open( ttyname.c_str(), baud ) ) {
        auto ec = usb_->error_code();
        log( dg::log::WARN ) << boost::format( "Error: %1% for tty device '%2%'" )
            % ec.message() % ttyname;
        return false;
    }

    ttyname_ = ttyname;

    usb_->async_reader( [=]( const char * send, std::size_t length ){ handle_receive( send, length ); } );
    usb_->start();

    threads_.push_back( std::thread( boost::bind( &boost::asio::io_service::run, &io_service_ ) ) );

    std::string reply;

    if ( _xsend( "*IDN?\r\n", reply ) && reply[0] != '?' ) { // identify
        idn_ = reply;
    }

    if ( _xsend( ":INST:FULL?\r\n", reply ) && reply[0] != '?' ) {
        inst_full_ = reply;
    }

    peripheral_query_device_data( false );

    return true;
}

bool
bnc565::reset()
{
    xsend_timeout_c_ = 0;
    reply_timeout_c_ = 0;

    // adportable::debug(__FILE__, __LINE__) << "BNC555::reset";

    std::string reply;

    if ( _xsend( "*RST\r\n", reply, "ok", 10 ) ) {

        for ( int ch  = 1; ch <= 8; ++ch ) {
            
            _xsend( (boost::format(":PULSE%1%:WIDTH %2%\r\n") % ch % 1.0e-6).str().c_str(), reply, "ok", 10 );
            _xsend( (boost::format(":PULSE%1%:DELAY %2%\r\n") % ch % (ch * 1.0e-5)).str().c_str(), reply, "ok", 10 );
            _xsend( (boost::format(":PULSE%1%:POL NORM\r\n") % ch).str().c_str(), reply, "ok", 10 );
            _xsend( (boost::format(":PULSE%1%:STATE ON\r\n") % ch).str().c_str(), reply, "ok", 10 );
        }
        
        if ( _xsend( ":PULSE5:SYNC CHA\r\n", reply, "ok", 10 ) &&
             _xsend( ":PULSE5:WIDTH 0.000001\r\n", reply, "ok", 10 ) && // 1us pulse for AP240 TRIG-IN
             _xsend( ":PULSE0:STATE ON\r\n", reply, "ok", 10 ) ) { // start trigger

             // simulated ion peak pulse
            _xsend( ":PULSE6:SYNC CHD\r\n", reply, "ok", 10 );
            _xsend( ":PULSE6:STATE ON\r\n", reply, "ok", 10 );
            _xsend( ":PULSE6:POL INV\r\n",  reply, "ok", 10 );
            _xsend( ":PULSE6:WIDTH 4.0E-9\r\n", reply, "ok", 10 ); // 4.0ns
            _xsend( ":PULSE6:DELAY 1.8E-6\r\n", reply, "ok", 10 ); // EXIT + 1.8us
            //_xsend( ":PULSE6:OUTP:MODE ADJ\r\n", reply, "ok", 10 ); // Adjustable
            //_xsend( ":PULSE6:OUTP:AMPL 2.3\r\n", reply, "ok", 10 ); // 2.3V
            //_xsend( ":SYST:KLOCK ON\r\n", reply, "ok", 10 ); // Locks the keypad
            _xsend( ":SYST:KLOCK OFF\r\n", reply, "ok", 10 );

            return true;
        }
    }
    return false;
}

void
bnc565::setInterval( double )
{
}
