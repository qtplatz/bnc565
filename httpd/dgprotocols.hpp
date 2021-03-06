// This is a -*- C++ -*- header.
/**************************************************************************
** Copyright (C) 2016 Toshinobu Hondo, Ph.D.
** Copyright (C) 2016 MS-Cheminformatics LLC
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

#pragma once

#include "dgprotocol.hpp"
#include <string>
#include <vector>

namespace dg {
    
    template< typename protocol_type = protocol< delay_pulse_count > >
    class protocols {
    public:
        protocols() : protocols_( 1 )
                    , interval_( 1.0e-3 )
                    , state_( 0 ) {
        }
        
        protocols( const protocols& t ) : interval_( t.interval_ )
                                        , protocols_( t.protocols_ ) {
        }
        
        static bool read_json( std::istream&, protocols<protocol<> >& );
        static bool write_json( std::ostream&, const protocols<protocol<> >& );
        
        double interval() const {
            return interval_;
        }
        
        void setInterval( double interval ) {
            interval_ = interval;
        }
        
        const protocol_type& operator [] ( int idx ) const {
            return protocols_[ idx ];
        }

        protocol_type& operator [] ( int idx ) {
            return protocols_[ idx ];
        }        

        const size_t size() const {
            return protocols_.size();
        }
        
        void resize( size_t sz ) {
            protocols_.resize( sz );
        }

        const std::string& idn() const { return idn_; }
        void setIdn( const std::string& v ) { idn_ = v; }

        const std::string& full() const { return inst_full_; }
        void setFull( const std::string& v ) { inst_full_ = v; }

        int state() const { return state_; }
        void setState( int v ) { state_ = v; }
        
        typedef typename std::vector< protocol_type >::const_iterator const_iterator;
        typedef typename std::vector< protocol_type >::iterator iterator;

        const_iterator begin() const { return protocols_.begin(); }
        const_iterator end() const { return protocols_.end(); }

        iterator begin() { return protocols_.begin(); }
        iterator end() { return protocols_.end(); }

    private:
        std::string idn_;
        std::string inst_full_;
        double interval_;
        int state_;
        std::vector< protocol_type > protocols_;
    };
        
}
