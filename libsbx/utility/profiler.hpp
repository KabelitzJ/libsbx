#ifndef LIBSBX_UTILITY_PROFILER_HPP_
#define LIBSBX_UTILITY_PROFILER_HPP_

#if defined(SBX_PROFILING_ENABLED)

#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>

#define SBX_PROFILE_SCOPE(name) ZoneScopedN(name)
#define SBX_PROFILE_SCOPE_NAMED(token, name) ZoneNamedN(token, name, true)
#define SBX_PROFILE_FUNCTION() ZoneScoped
#define SBX_PROFILE_SCOPE_COLORED(name, color) ZoneScopedNC(name, color)
#define SBX_PROFILE_SET_ZONE_TEXT(text, size) ZoneText(text, size)

#define SBX_PROFILE_SCOPE_START(token, name) TracyCZoneN(token, name, true)
#define SBX_PROFILE_SCOPE_END(token) TracyCZoneEnd(token)

#define SBX_PROFILE_FRAME_MARK() FrameMark
#define SBX_PROFILE_FRAME_MARK_NAMED(name) FrameMarkNamed(name)
#define SBX_PROFILE_FRAME_MARK_START(name) FrameMarkStart(name)
#define SBX_PROFILE_FRAME_MARK_END(name) FrameMarkEnd(name)

#define SBX_PROFILE_THREAD_NAME(name) tracy::SetThreadName(name)

#define SBX_PROFILE_PLOT(name, value) TracyPlot(name, value)
#define SBX_PROFILE_MESSAGE(text, size) TracyMessage(text, size)
#define SBX_PROFILE_MESSAGE_LITERAL(text) TracyMessageL(text)

#else

#define SBX_PROFILE_SCOPE(name) static_cast<void>(0)
#define SBX_PROFILE_SCOPE_NAMED(token, name) static_cast<void>(0)
#define SBX_PROFILE_FUNCTION() static_cast<void>(0)
#define SBX_PROFILE_SCOPE_COLORED(name, color) static_cast<void>(0)
#define SBX_PROFILE_SET_ZONE_TEXT(text, size) static_cast<void>(0)

#define SBX_PROFILE_SCOPE_START(token, name) static_cast<void>(0)
#define SBX_PROFILE_SCOPE_END(token) static_cast<void>(0)

#define SBX_PROFILE_FRAME_MARK() static_cast<void>(0)
#define SBX_PROFILE_FRAME_MARK_NAMED(name) static_cast<void>(0)
#define SBX_PROFILE_FRAME_MARK_START(name) static_cast<void>(0)
#define SBX_PROFILE_FRAME_MARK_END(name) static_cast<void>(0)

#define SBX_PROFILE_THREAD_NAME(name) static_cast<void>(0)

#define SBX_PROFILE_PLOT(name, value) static_cast<void>(0)
#define SBX_PROFILE_MESSAGE(text, size) static_cast<void>(0)
#define SBX_PROFILE_MESSAGE_LITERAL(text) static_cast<void>(0)

#endif // SBX_PROFILING_ENABLED

#endif // LIBSBX_UTILITY_PROFILER_HPP_
