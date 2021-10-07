#include "frontend/exp_value.hpp"

bool IRValue::assignable() const {
  return is_left_value && (!type.is_const) && (!type.is_array());
}