#pragma once

#include <boost/multiprecision/cpp_dec_float.hpp>

#include <QString>

#include <algorithm>
#include <cmath>
#include <ios>
#include <string>

// This exceeds the application's maximum 2048-bit physics setting
// (approximately 617 decimal digits), so coordinate generation is never the
// lower-precision stage.
using PreciseDecimal = boost::multiprecision::number<
    boost::multiprecision::cpp_dec_float<700>>;

inline PreciseDecimal preciseDecimal(const QString& text) {
    return PreciseDecimal(text.toStdString());
}

inline PreciseDecimal preciseDecimal(double value) {
    return PreciseDecimal(value);
}

inline QString preciseString(const PreciseDecimal& value, int digits = 650) {
    return QString::fromStdString(
        value.str(digits, std::ios_base::fmtflags(0)));
}

inline QString preciseDisplayString(const PreciseDecimal& value) {
    return preciseString(value, 40);
}

inline double preciseDouble(const PreciseDecimal& value) {
    return value.convert_to<double>();
}

inline PreciseDecimal precisePixelCenter(
    const PreciseDecimal& first, const PreciseDecimal& last,
    int index, int count) {
    return first +
           (PreciseDecimal(index) + PreciseDecimal("0.5")) / count *
               (last - first);
}

inline QString precisePhysicsString(
    const PreciseDecimal& value, int precisionBits) {
    const int digits = std::clamp(
        int(std::ceil(std::max(1, precisionBits) * 0.3010299956639812)) + 6,
        25, 650);
    return preciseString(value, digits);
}
