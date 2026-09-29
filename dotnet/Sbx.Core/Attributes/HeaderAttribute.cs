using System;

namespace Sbx.Core.Attributes
{

  /// <summary>Draws a labeled separator above the field in the Inspector, to group the fields that follow.</summary>
  [AttributeUsage(AttributeTargets.Field)]
  public class HeaderAttribute : Attribute
  {
    public string Text { get; set; }

    public HeaderAttribute(string text)
    {
      Text = text;
    }

  } // class HeaderAttribute

} // namespace Sbx.Core.Attributes
