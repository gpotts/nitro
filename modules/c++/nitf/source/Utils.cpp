/* =========================================================================
 * This file is part of NITRO
 * =========================================================================
 *
 * (C) Copyright 2004 - 2016, MDA Information Systems LLC
 *
 * NITRO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, If not,
 * see <http://www.gnu.org/licenses/>.
 *
 */


#include "nitf/Utils.hpp"

using namespace nitf;

bool Utils::isNumeric(std::string str)
{
    return (bool) nitf_Utils_isNumeric((char*) str.c_str());
}

bool Utils::isAlpha(std::string str)
{
    return (bool) nitf_Utils_isNumeric((char*) str.c_str());
}

void Utils::decimalToGeographic(double decimal, int* degrees, int* minutes,
                                double* seconds)
{
    nitf_Utils_decimalToGeographic(decimal, degrees, minutes, seconds);
}

double Utils::geographicToDecimal(int degrees, int minutes, double seconds)
{
    return nitf_Utils_geographicToDecimal(degrees, minutes, seconds);
}

char Utils::cornersTypeAsCoordRep(nitf::CornersType type)
{
    return nitf_Utils_cornersTypeAsCoordRep(type);
}

