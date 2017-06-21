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

bool
bnc565::state() const
{
    if ( usb_->is_open() )
        return protocols_.state();
    return false;
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
    const auto& p = *d.begin();
    std::string reply;

    // dg::protocols<>::write_json( std::cout, d );

    _xsend( (boost::format(":PULSE0:PER %1%\r\n") % d.interval() ).str().c_str(), reply, "ok", 10 );
    
    for ( int ch = 1; ch <= p.size; ++ch ) {
        
        const auto& v = p[ ch - 1 ];
        const char * state = std::get< dg::pulse_state >( v ) ? "ON" : "OFF";
        const char * pol = std::get< dg::pulse_polarity >( v ) ? "NORM" : "INV";
        std::string delay = ( boost::format( "%g" ) % std::get< dg::pulse_delay >( v ) ).str();
        std::string width = ( boost::format( "%g" ) % std::get< dg::pulse_width >( v ) ).str();
        
        _xsend( (boost::format(":PULSE%1%:STATE %2%\r\n") % ch % state ).str().c_str(), reply, "ok", 10 );
        
        _xsend( (boost::format(":PULSE%1%:POL %2%\r\n")   % ch % pol ).str().c_str(), reply, "ok", 10 );
        
        _xsend( (boost::format(":PULSE%1%:DELAY %2%\r\n") % ch % delay ).str().c_str(), reply, "ok", 10 );
        
        _xsend( (boost::format(":PULSE%1%:WIDTH %2%\r\n") % ch % width ).str().c_str(), reply, "ok", 10 );
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
        if ( _xsend( ":PULSE0:PER?\r\n", reply ) ) {
            if ( reply[0] != '?' ) {
                try {
                    d.setInterval( boost::lexical_cast<double>(reply) );
                } catch ( std::exception& ex ) {
                    log( log::ERR ) << boost::format( "%1%:%2% %3% (%4%)" ) % __FILE__ % __LINE__ % ex.what() % reply;
                }
            }
        }

        auto& protocol = *d.begin();
    
        for ( size_t ch = 0; ch < protocol.size; ++ch ) {
            try {
                if ( _xsend( ( boost::format( ":PULSE%1%:STATE?\r\n" ) % (ch+1) ).str().c_str(), reply ) ) {
                    protocol.state( ch ) = boost::lexical_cast<int>(reply);
                }
                if ( _xsend( ( boost::format( ":PULSE%1%:WIDTH?\r\n" ) % (ch+1) ).str().c_str(), reply ) ) {
                    protocol.width( ch ) = boost::lexical_cast<double>(reply); // seconds
                }
                if ( _xsend( ( boost::format( ":PULSE%1%:DELAY?\r\n" ) % (ch+1) ).str().c_str(), reply ) ) {
                    protocol.delay( ch ) = boost::lexical_cast<double>(reply); // seconds
                }
                if ( _xsend( ( boost::format( ":PULSE%1%:POL?\r\n" ) % (ch+1) ).str().c_str(), reply ) ) {
                    protocol.polarity( ch ) = ( reply == "NORM" || reply == "HIGH" ) ? dg::positive_polarity : dg::negative_polarity;
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
    protocols_ = d;

    return true;
}

const std::string&
bnc565::idn() const
{
    return protocols_.idn();
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
        protocols_.setIdn( reply );
    }

    if ( verbose )
        std::cout << "*IDN? : " << reply << std::endl;

    if ( _xsend( ":INST:FULL?\r\n", reply ) && reply[0] != '?' ) {
        protocols_.setFull( reply );
    }
    
    if ( verbose )
        std::cout << "INST:FULL? : << reply " << std::endl;

    // To status (on|off)
    if ( _xsend( ":PULSE0:STATE?\r\n", reply ) ) {
        if ( reply[0] != '?' ) {
            try {
                int value = boost::lexical_cast<int>(reply);
                protocols_.setState( value );
            } catch ( std::exception& ex ) {
                log( log::ERR ) << boost::format( "%1%:%2% %3% (%4%)" ) % __FILE__ % __LINE__ % ex.what() % reply;
            }
        }
    }
    if ( verbose )
        std::cout << ":PULSE0:STATE? : " << reply << std::endl;

    auto& protocol = *protocols_.begin();
    
    for ( int i = 0; i < protocol.size; ++i ) {
        const char * loc = "";
        try {
            if ( _xsend( ( boost::format( ":PULSE%1%:STATE?\r\n" ) % (i+1) ).str().c_str(), reply ) ) {
                loc = "STATE";
                protocol.state( i ) = boost::lexical_cast<int>(reply);
            }
            if ( _xsend( ( boost::format( ":PULSE%1%:WIDTH?\r\n" ) % (i+1) ).str().c_str(), reply ) ) {
                loc = "WIDTH";
                protocol.width( i ) = boost::lexical_cast<double>(reply);
            }
            if ( _xsend( ( boost::format( ":PULSE%1%:DELAY?\r\n" ) % (i+1) ).str().c_str(), reply ) ) {
                loc = "DELAY";
                protocol.delay( i ) = boost::lexical_cast<double>(reply);
            }
            if ( _xsend( ( boost::format( ":PULSE%1%:POL?\r\n" ) % (i+1) ).str().c_str(), reply ) ) {
                loc = "POL";
                protocol.polarity( i ) = ( reply == "NORM" || reply == "HIGH" ) ? dg::positive_polarity : dg::negative_polarity;
            }
            
            if ( verbose )
                std::cout << boost::format( ":PULSE%1%" ) % (i+1)
                          << boost::format( ", STATE=%1%" ) % protocol.state( i )
                          << boost::format( ", WIDTH=%1%" ) % protocol.width( i )
                          << boost::format( ", DELAY=%1%" ) % protocol.delay( i )
                          << boost::format( ", POL=%1%" ) % protocol.polarity( i )
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

    if ( usb_->is_open() ) {

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
    return false;
}

bool
bnc565::_xsend( const char * data, std::string& reply, const std::string& expect, size_t ntry )
{
    if ( ! usb_->is_open() ) {
        // debug
        std::cout << __FILE__ << ":" << __LINE__ << " " << data << std::endl;
        return false;
    }
    
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
    baud_ = baud;

    usb_->async_reader( [=]( const char * send, std::size_t length ){ handle_receive( send, length ); } );
    usb_->start();

    threads_.push_back( std::thread( boost::bind( &boost::asio::io_service::run, &io_service_ ) ) );

    std::string reply;

    if ( _xsend( "*IDN?\r\n", reply ) && reply[0] != '?' ) { // identify
        protocols_.setIdn( reply );
    }

    if ( _xsend( ":INST:FULL?\r\n", reply ) && reply[0] != '?' ) {
        protocols_.setFull( reply );
    }

    peripheral_query_device_data( false );

    return true;
}

bool
bnc565::switch_connect( bool onoff, std::string& reply )
{
    if ( ! usb_->is_open() ) {
        if ( ! initialize( ttyname_, baud_ ) )
            reply = ( boost::format( "Error: %1% for tty device '%2%'; " ) % usb_->error_code() % ttyname_ ).str();
    }
    std::string rep;
    bool res = _xsend( (boost::format(":PULSE0:STATE %1%\r\n") % (onoff ? "ON" : "OFF")).str().c_str(), rep, "ok", 10 );
    reply += rep;
    return res;
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
bnc565::setInterval( double v )
{
    protocols_.setInterval( v );
}

double
bnc565::interval() const
{
    return protocols_.interval();
}
