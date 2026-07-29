#ifndef LIBSBX_UTILITY_FUNCTION_PTR_HPP_
#define LIBSBX_UTILITY_FUNCTION_PTR_HPP_

namespace sbx::utility {

template<typename Return, typename... Args>
using function_ptr = Return(*)(Args...);

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_FUNCTION_PTR_HPP_