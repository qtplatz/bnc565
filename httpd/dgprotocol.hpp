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

#include <array>
#include <tuple>

namespace dg {

    typedef std::tuple< double, double, bool, int > delay_pulse_type;

    enum { pulse_delay, pulse_width, pulse_polarity, pulse_state };

    enum { positive_polarity, negative_polarity }; // false = pos, true = neg

    size_t constexpr delay_pulse_count = 8;

    template< size_t _size = delay_pulse_count >
    class protocol {
    public:
        static size_t constexpr size = _size;

        protocol() {}
            
        protocol( const protocol& t ) : pulses_( t.pulses_ ) {
        }

        delay_pulse_type& operator []( int index ) {
            return pulses_ [ index ];
        }
            
        const delay_pulse_type& operator []( int index ) const {
            return pulses_ [ index ];
        }

        const std::array< delay_pulse_type, size >& pulses() const {
            return pulses_;
        }

        double& delay( int ch )  { return std::get< pulse_delay > ( pulses_[ ch ] ); }
        double& width( int ch )  { return std::get< pulse_width > ( pulses_[ ch ] ); }
        bool& polarity( int ch ) { return std::get< pulse_polarity > ( pulses_[ ch ] ); }
        int& state( int ch )     { return std::get< pulse_state > ( pulses_[ ch ] ); }
        
    private:
        std::array< delay_pulse_type, _size > pulses_;  // delay, width, inv, state
    };
}
