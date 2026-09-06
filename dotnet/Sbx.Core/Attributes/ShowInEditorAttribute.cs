using System;

namespace Sbx.Core.Attributes
{

  [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
  public class ShowInEditorAttribute : Attribute
  {
    public string DisplayName { get; set; } = "";
    public bool IsReadOnly { get; set; } = false;

    public ShowInEditorAttribute() { }

    public ShowInEditorAttribute(string displayName)
    {
      DisplayName = displayName;
    }

  } // class ShowInEditorAttribute
  
} // namespace Sbx.Attributes
