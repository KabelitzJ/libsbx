using System;

namespace Sbx.Core.Attributes
{

  /// <summary>How a script field appears in the Inspector: DisplayName overrides the label (empty = the field name), IsReadOnly shows it disabled.</summary>
  [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
  public class InspectorAttribute : Attribute
  {
    public string DisplayName { get; set; } = "";
    public bool IsReadOnly { get; set; } = false;

    public InspectorAttribute() { }

    public InspectorAttribute(string displayName)
    {
      DisplayName = displayName;
    }

  } // class InspectorAttribute

} // namespace Sbx.Core.Attributes
