using System;

namespace Sbx.Core.Attributes
{

  /// <summary>Shows Text when hovering the field in the Inspector.</summary>
  [AttributeUsage(AttributeTargets.Field)]
  public class TooltipAttribute : Attribute
  {
    public string Text { get; set; }

    public TooltipAttribute(string text)
    {
      Text = text;
    }

  } // class TooltipAttribute

} // namespace Sbx.Core.Attributes
