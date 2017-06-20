// This is a -*- C++ -*- header.
/**************************************************************************
** Copyright (C) 2016-2017 Toshinobu Hondo, Ph.D.
** Copyright (C) 2016-2017 MS-Cheminformatics LLC
*
** Contact: info@ms-cheminfo.com
**
** Commercial Usage
**
** Licensees holding valid MS-Cheminformatics commercial licenses may use this file in
** accordance with the MS-Cheminformatics Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and MS-Cheminformatics.
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

#include "dgprotocols.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/exception/all.hpp>
#include <boost/format.hpp>
#include <iostream>
#include <ratio>

static void
print( const boost::property_tree::ptree& pt )
{
    using boost::property_tree::ptree;

    ptree::const_iterator end = pt.end();
    for (ptree::const_iterator it = pt.begin(); it != end; ++it) {
        std::cout << it->first << ": " << it->second.get_value<std::string>() << std::endl;
        print(it->second);
    }    
}


// using namespace adportable::dg;

namespace dg {
    
    template<> bool
    protocols< protocol<> >::read_json( std::istream& json, protocols< protocol<> >& protocols )
    {
        protocols.protocols_.clear();
        
        boost::property_tree::ptree pt;
        
        try {
            boost::property_tree::read_json( json, pt );

            if ( auto interval = pt.get_optional< double >( "protocols.interval" ) )
                protocols.interval_ = interval.get() / std::micro::den; // us -> seconds
            // protocols.interval_ = std::stod( pt.get_child( "protocols.interval" ).data() ) * 1.0e-6; // us -> seconds
                
            for ( const auto& v : pt.get_child( "protocols.protocol" ) ) {

                protocol<delay_pulse_count> data;
                int index = 0;
                if ( auto value = v.second.get_optional< int >( "index" ) )
                    index = value.get();

                size_t ch(0);
                for ( const auto& pulse: v.second.get_child( "pulses" ) ) {
                    auto v = std::make_tuple( 0, 0, false );
                    if ( ch < protocol<>::size ) {
                        if ( auto delay = pulse.second.get_optional< double >( "delay" ) )
                            std::get< 0 >( v ) = delay.value();
                        if ( auto width = pulse.second.get_optional< double >( "width" ) )
                            std::get< 1 >( v ) = width.value();
                        if ( auto inv = pulse.second.get_optional< bool >( "inv" ) )
                            std::get< 2 >( v ) = inv.value();
                        data[ int(ch) ] = v;
                    }
                    ++ch;
                    std::cout << __FILE__ << __LINE__ << "----- ch : " << ch << " delay: "
                              << std::get<0>(v) << " width: " << std::get<1>(v) << " inv=" << std::get<2>(v) << std::endl;
                }
                protocols.protocols_.emplace_back( data );
            }

            return true;
        
        } catch ( std::exception& e ) {
            // std::cout << boost::diagnostic_information( e );
            throw e;
        }
        return false;
    }

    /////////////////////
        
    template<> bool
    protocols< protocol<> >::write_json( std::ostream& o, const protocols< protocol<> >& protocols )
    {
        boost::property_tree::ptree pt;

        pt.put( "idn", protocols.idn() );
        pt.put( "inst_full", protocols.full() );
    
        pt.put( "protocols.interval", protocols.interval_ ); // seconds --> us
    
        boost::property_tree::ptree pv;
    
        int protocolIndex( 0 );
    
        for ( const auto& protocol: protocols.protocols_ ) {
        
            boost::property_tree::ptree xproto;
        
            xproto.put( "index", protocolIndex++ );
        
            boost::property_tree::ptree xpulses;
        
            for ( const auto& pulse: protocol.pulses() ) {
                boost::property_tree::ptree xpulse;

                xpulse.put( "delay", ( boost::format( "%g" ) % ( std::get<0>(pulse) * std::micro::den ) ).str() );
                xpulse.put( "width", ( boost::format( "%g" ) % ( std::get<1>(pulse) * std::micro::den ) ).str() );
                xpulse.put( "inv", std::get<2>(pulse) );
                
                xpulses.push_back( std::make_pair( "", xpulse ) );
            }
        
            xproto.add_child( "pulses", xpulses );

            pv.push_back( std::make_pair( "", xproto ) );
        }
    
        pt.add_child( "protocols.protocol", pv );

        boost::property_tree::write_json( o, pt );
    
        return true;
    }

}

