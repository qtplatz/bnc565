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

    size_t constexpr delay_pulse_count = 8;

    template< size_t _size = delay_pulse_count >
    class protocol {
    public:
        static size_t constexpr size = _size;

        protocol() {}
            
        protocol( const protocol& t ) : pulses_( t.pulses_ ) {
        }

        std::tuple< double, double, bool >& operator []( int index ) {
            return pulses_ [ index ];
        }
            
        const std::tuple< double, double, bool >& operator []( int index ) const {
            return pulses_ [ index ];
        }

        const std::array< std::tuple< double, double, bool >, size >& pulses() const {
            return pulses_;
        }
            
    private:
        std::array< std::tuple< double, double, bool >, _size > pulses_;  // delay, width, inv = true
    };
}
