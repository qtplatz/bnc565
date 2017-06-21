// -*- C++ -*-
/**************************************************************************
** Copyright (C) 2010-2017 Toshinobu Hondo, Ph.D.
** Copyright (C) 2013-2017 MS-Cheminformatics LLC
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

#include "config.h"
#include "dgctl.hpp"
#include "bnc565.hpp"
#include "log.hpp"
#include "pugixml.hpp"
#include "dgprotocols.hpp"
#include <boost/exception/all.hpp>
#include <boost/format.hpp>
#include <sstream>
#include <iostream>
#include <fstream>

namespace dg {

    struct time {
        static double scale_to_ms( double t ) { return t * 1.0e6; }
    };
    
}

extern bool __debug_mode__;

using namespace dg;

dgctl::dgctl() : is_active_( false )
               , is_dirty_( false )
               , pulser_interval_( 0.001 ) // 0.001s
{
    update();
    
    bnc565::instance()->register_handler( [&]( size_t tick ){
            std::string msg = std::to_string( tick );
            std::for_each( event_handlers_.begin(), event_handlers_.end()
                           , [msg]( std::function<void( const std::string& data, const std::string& id, const std::string& ev )> f ) {
				                 f( msg, "", "tick" );
                           } );
            
        });
}

dgctl::~dgctl()
{
}

dgctl *
dgctl::instance()
{
    static dgctl __instance;
    return &__instance;
}

void
dgctl::update()
{
    int channel = 0;

    for ( auto& pulse: pulses_ )
        pulse = bnc565::instance()->pulse( channel++ );
    
    //pulser_interval_ = bnc565::instance()->interval();
    //uint32_t trig = bnc565::instance()->trigger();
}

size_t
dgctl::size() const
{
    return pulses_.size();
}

const dgctl::value_type&
dgctl::pulse( size_t idx ) const
{
    return pulses_[ idx ];
}
    
void
dgctl::pulse( size_t idx, const value_type& v )
{
    pulses_[ idx ] = v;
    is_dirty_ = true;    
}

dgctl::iterator
dgctl::begin()
{
    return pulses_.begin();
}

dgctl::iterator
dgctl::end()
{
    return pulses_.end();    
}

dgctl::const_iterator
dgctl::begin() const
{
    return pulses_.begin();    
}

dgctl::const_iterator
dgctl::end() const
{
    return pulses_.end();        
}

double
dgctl::pulser_interval() const
{
    return bnc565::instance()->interval();
}

void
dgctl::pulser_interval( double v )
{
    bnc565::instance()->setInterval( v );
}

void
dgctl::commit()
{
    if ( *(bnc565::instance()) )
        return;
    
    int channel = 0;

    for ( auto& pulse: pulses_ )
        bnc565::instance()->setPulse( channel++, pulse );

    bnc565::instance()->setInterval( pulser_interval_ );

    //bnc565::instance()->commit();
}

bool
dgctl::activate_trigger()
{
    //bnc565::instance()->activate_trigger();
    is_active_ = true;
    return true;
}

bool
dgctl::deactivate_trigger()
{
    //bnc565::instance()->deactivate_trigger();
    is_active_ = false;
    return true;
}

bool
dgctl::is_active() const
{
    return is_active_;
}

bool
dgctl::http_request( const std::string& method, const std::string& request_path, std::string& rep )
{
    std::ostringstream o;

    std::cout << __FILE__ << __LINE__ << "http_request: " << method << " : " << request_path << std::endl;

    if ( request_path == "/dg/ctl?status.json" ) {

        dg::protocols<> p;
        if ( bnc565::instance()->fetch( p ) ) {
            if ( dg::protocols<>::write_json( o, p ) )
                rep += o.str();

            // dg::protocols<>::write_json( std::cout, p );
        }

    } else if ( request_path == "/dg/ctl?banner" ) {

        o << "<h2>BNC 565 V" << PACKAGE_VERSION " Rev. " << bnc565::instance()->idn() << "</h2>";
        rep += o.str();

    } else if ( request_path.compare( 0, 20, "/dg/ctl?commit.json=", 20 ) == 0 ) {

        std::stringstream payload( request_path.substr( 20 ) );
        dg::protocols<> protocols;
        
        try {
            if ( dg::protocols<>::read_json( payload, protocols ) ) {

                // dg::protocols<>::write_json( std::cout, protocols );

                bnc565::instance()->commit( protocols );
                o << "COMMIT SUCCESS; " << ( is_active() ? "(trigger is active)" : ( "trigger is not active" ) );
                rep = o.str();
            }
        } catch ( std::exception& e ) {
            log() << boost::diagnostic_information( e );
        }

    } else if ( request_path.compare( 0, 20, "/dg/ctl?submit.text=", 20 ) == 0 ) {

        std::string payload( request_path.substr( 20 ) );
        std::cout << payload << std::endl;

    } else if ( request_path == "/dg/ctl?fsm=start" ) {

        activate_trigger();
        rep = "TRIGGER ACTIVATED";
        
    } else if ( request_path == "/dg/ctl?fsm=stop" ) {        

        deactivate_trigger();
        rep = "TRIGGER DEACTIVATED";                            

    } else if ( request_path.compare( 0, 10, "/dg/ctl?q=", 10 ) == 0 ) {

        auto colon = request_path.find_last_of( ';' );

        if ( colon != std::string::npos ) {

            double us = std::stod( request_path.substr( 10, colon - 10 ) );

            std::string item = "error";
            switch( request_path[ colon + 1 ] ) {
            case 'D': item = "delay"; break;
            case 'W': item = "width"; break;
            case 'T' : item = "interval"; break;
            };
            o << boost::format("%s : %.4g&mu;s %.4gms %.4gs")
                % item % us % (us * 1.0e-3) % ( us * 1.0e-6 );
            rep = o.str();
        }

    } else if ( request_path == "/dg/ctl?events" ) {

        rep = "SSE";
        
    } else {
        
        o << "dgctl -- unknown request(" << method << ", " << request_path << ")";
        rep = o.str();

    }
    return true;
}

void
dgctl::register_sse_handler( std::function< void( const std::string&, const std::string&, const std::string& ) > f )
{
    event_handlers_.push_back( f );
}
