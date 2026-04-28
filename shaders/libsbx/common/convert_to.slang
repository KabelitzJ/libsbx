#ifndef COMMON_CONVERT_TO_SLANG_
#define COMMON_CONVERT_TO_SLANG_

namespace sbx::common {

interface convert_to<T> {
  T to();
}; // interface convert_to

To to<To, From: convert_to<To>>(From from) {
  return from.to();
}

} // namespace sbx::common

#endif // COMMON_CONVERT_TO_SLANG_
