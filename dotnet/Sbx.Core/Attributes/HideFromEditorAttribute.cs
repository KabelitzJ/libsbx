using System;

namespace Sbx.Core.Attributes
{

  [Obsolete("Use [HideInInspector] instead.")]
  [AttributeUsage(AttributeTargets.Field)]
  public class HideFromEditorAttribute : Attribute
  {

  } // class HideFromEditorAttribute
  
} // namespace Sbx.Attributes
