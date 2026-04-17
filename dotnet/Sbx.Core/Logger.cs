namespace Sbx.Core
{

  public static class Logger
  {
    internal enum Level
    {
      Trace = 1 << 0,
      Debug = 1 << 1,
      Info = 1 << 2,
      Warn = 1 << 3,
      Error = 1 << 4,
      Critical = 1 << 5
    }

    public static void Trace(string format, params object?[] parameters)
		{
			unsafe { InternalCalls.Log_LogMessage(Level.Trace, string.Format(format, parameters)); }
		}

		public static void Debug(string format, params object?[] parameters)
		{
			unsafe { InternalCalls.Log_LogMessage(Level.Debug, string.Format(format, parameters)); }
		}

		public static void Info(string format, params object?[] parameters)
		{
			unsafe { InternalCalls.Log_LogMessage(Level.Info, string.Format(format, parameters)); }
		}

		public static void Warn(string format, params object?[] parameters)
		{
			unsafe { InternalCalls.Log_LogMessage(Level.Warn, string.Format(format, parameters)); }
		}

		public static void Error(string format, params object?[] parameters)
		{
			unsafe { InternalCalls.Log_LogMessage(Level.Error, string.Format(format, parameters)); }
		}

		public static void Critical(string format, params object?[] parameters)
		{
			unsafe { InternalCalls.Log_LogMessage(Level.Critical, string.Format(format, parameters)); }
		}

		public static void Trace(object value)
		{
			unsafe { InternalCalls.Log_LogMessage(Level.Trace, value != null ? value.ToString() : "[null]"); }
		}

		public static void Debug(object value)
		{
			unsafe { InternalCalls.Log_LogMessage(Level.Debug, value != null ? value.ToString() : "[null]"); }
		}

		public static void Info(object value)
		{
			unsafe { InternalCalls.Log_LogMessage(Level.Info, value != null ? value.ToString() : "[null]"); }
		}

		public static void Warn(object value)
		{
			unsafe { InternalCalls.Log_LogMessage(Level.Warn, value != null ? value.ToString() : "[null]"); }
		}

		public static void Error(object value)
		{
			unsafe { InternalCalls.Log_LogMessage(Level.Error, value != null ? value.ToString() : "[null]"); }
		}

		public static void Critical(object value)
		{
			unsafe { InternalCalls.Log_LogMessage(Level.Critical, value != null ? value.ToString() : "[null]"); }
		}
  }

} // namespace Sbx.Core

