using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;

using Sbx.Managed.Interop;

namespace Sbx.Managed
{

  internal enum MessageLevel
  {
    Info = 1,
    Warning = 2,
    Error = 4
  }

  internal static class Host
  {
    private static unsafe delegate*<NativeString, void> _exceptionCallback;

    private static unsafe delegate*<NativeString, MessageLevel, void> _messageCallback;

    [UnmanagedCallersOnly]
    private static unsafe void Initialize(delegate*<NativeString, MessageLevel, void> InMessageCallback, delegate*<NativeString, void> InExceptionCallback)
    {
      _messageCallback = InMessageCallback;
      _exceptionCallback = InExceptionCallback;
    }

    internal static void LogMessage(string InMessage, MessageLevel InLevel)
    {
      unsafe
      {
        using NativeString message = InMessage;
        _messageCallback(message, InLevel);
      }
    }

    internal static void HandleException(Exception InException)
    {
      unsafe
      {
        if (_exceptionCallback == null)
          return;

        using NativeString message = InException.ToString();
        _exceptionCallback(message);
      }
    }

  }

} // namespace Sbx.Managed