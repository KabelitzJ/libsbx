using System;

namespace Sbx.Attributes
{

  [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
  public class ShowInEditorAttribute : Attribute
  {
    public string DisplayName = "";
    public bool IsReadOnly = false;

    public ShowInEditorAttribute(string displayName)
    {
      DisplayName = displayName;
    }

  } // class ShowInEditorAttribute
  
} // namespace Sbx.Attributes