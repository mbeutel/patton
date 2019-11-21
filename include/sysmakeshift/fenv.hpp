
#ifndef INCLUDED_SYSMAKESHIFT_FENV_HPP_
#define INCLUDED_SYSMAKESHIFT_FENV_HPP_


#include <gsl/gsl-lite.hpp> // for Expects(), gsl_NODISCARD


namespace sysmakeshift
{


    //ᅟ
    // Sets hardware exception traps for the floating-point exceptions specified by the given mask value.
    // The admissible mask values are defined as `FE_*` in standard header <cfenv>.
    // If an exception flag bit is on, the corresponding exception will be trapped; if the bit is clear, the exception will be masked.
    //
gsl_NODISCARD bool try_set_trapping_fe_exceptions(int excepts) noexcept;

    //ᅟ
    // Disables hardware exception traps for the floating-point exceptions specified by the given mask value.
    // The admissible mask values are defined as `FE_*` in standard header <cfenv>.
    //
inline void set_trapping_fe_exceptions(int excepts)
{
    bool succeeded = try_set_trapping_fe_exceptions(excepts);
    Expects(succeeded);
}

    //ᅟ
    // Returns the bitmask of all floating-point exceptions for which trapping is currently enabled.
    // The admissible mask values are defined as `FE_*` in standard header <cfenv>.
    //
int get_trapping_fe_exceptions(void) noexcept;


} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_FENV_HPP_
