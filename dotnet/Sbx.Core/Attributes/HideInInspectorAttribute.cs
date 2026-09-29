using System;

namespace Sbx.Core.Attributes
{

  /// <summary>Keeps a public script field out of the Inspector.</summary>
  [AttributeUsage(AttributeTargets.Field)]
  public class HideInInspectorAttribute : Attribute
  {

  } // class HideInInspectorAttribute

} // namespace Sbx.Core.Attributes
