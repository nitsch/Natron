//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>

#include "ImageParamsSerialization.h"

// explicit template instantiations


template void Natron::ImageParams::serialize<boost::archive::binary_iarchive>(boost::archive::binary_iarchive & ar,
                                                                              const unsigned int file_version);
template void Natron::ImageParams::serialize<boost::archive::binary_oarchive>(boost::archive::binary_oarchive & ar,
                                                                              const unsigned int file_version);
