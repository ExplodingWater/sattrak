#pragma once

#include <numbers>

namespace aero::constants {

// Physical & Astrodynamic Constants (SI Units)
inline constexpr double MU_EARTH       = 3.986004418e14;  // Earth standard gravitational parameter [m^3/s^2]
inline constexpr double EARTH_RADIUS   = 6.378137e6;      // Earth equatorial radius [m]
inline constexpr double EARTH_OMEGA    = 7.2921150e-5;    // Earth rotation rate [rad/s]

// Thermodynamics & Gas Dynamics Constants
inline constexpr double GAMMA_AIR      = 1.4;             // Ratio of specific heats for diatomic gas
inline constexpr double R_SPECIFIC_AIR = 287.058;         // Specific gas constant for dry air [J/(kg*K)]
inline constexpr double CP_AIR         = 1004.7;          // Specific heat at constant pressure [J/(kg*K)]
inline constexpr double CV_AIR         = 717.6;           // Specific heat at constant volume [J/(kg*K)]

// Standard Sea-Level Atmospheric Conditions (ISA)
inline constexpr double P_STD          = 101325.0;        // Sea-level standard atmospheric pressure [Pa]
inline constexpr double T_STD          = 288.15;          // Sea-level standard temperature [K]
inline constexpr double RHO_STD        = 1.225;           // Sea-level standard air density [kg/m^3]

// Numerical Tolerances
inline constexpr double EPSILON        = 1e-12;
inline constexpr double PI             = 3.14159265358979323846;

} // namespace aero::constants
