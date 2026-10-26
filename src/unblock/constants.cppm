export module unblock.constants;

import core.types;

// Domain-test verdict thresholds, lifted out of the removed pch.h so both
// the testing unit and Unblock itself can see them without a cycle.
export constexpr u32 MAX_SUCCESS_CONECTION = 90;
export constexpr u32 MAX_ERROR_CONECTION   = 100 - MAX_SUCCESS_CONECTION;
