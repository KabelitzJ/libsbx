using System;

namespace Sbx.Managed
{

  public static class ExtensionMethods
  {
    public static bool IsDelegate(this Type InType)
    {
      return typeof(MulticastDelegate).IsAssignableFrom(InType.BaseType);
    }
  }

} // namespace Sbx.Managed