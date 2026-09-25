using System.Runtime.InteropServices;

using Sbx.Core;

namespace Sbx.Core.UI
{

  // Mirrors the native interop's layout_element_data field for field (flag as 0/1).
  [StructLayout(LayoutKind.Sequential)]
  internal struct LayoutElementData
  {
    public float MinWidth;
    public float MinHeight;
    public float PreferredWidth;
    public float PreferredHeight;
    public float FlexibleWidth;
    public float FlexibleHeight;
    public uint IgnoreLayout;
  }

  /**
   * Overrides how a parent layout group sizes this element. A negative value (the default) means
   * "not set" -- the group falls back to the element's own content/rect. Every property reads the
   * native component and writes the whole of it back.
   */
  public class LayoutElement : Component
  {

    public float MinWidth
    {
      get => Get().MinWidth;
      set => Edit((ref LayoutElementData data) => data.MinWidth = value);
    }

    public float MinHeight
    {
      get => Get().MinHeight;
      set => Edit((ref LayoutElementData data) => data.MinHeight = value);
    }

    public float PreferredWidth
    {
      get => Get().PreferredWidth;
      set => Edit((ref LayoutElementData data) => data.PreferredWidth = value);
    }

    public float PreferredHeight
    {
      get => Get().PreferredHeight;
      set => Edit((ref LayoutElementData data) => data.PreferredHeight = value);
    }

    public float FlexibleWidth
    {
      get => Get().FlexibleWidth;
      set => Edit((ref LayoutElementData data) => data.FlexibleWidth = value);
    }

    public float FlexibleHeight
    {
      get => Get().FlexibleHeight;
      set => Edit((ref LayoutElementData data) => data.FlexibleHeight = value);
    }

    public bool IgnoreLayout
    {
      get => Get().IgnoreLayout != 0;
      set => Edit((ref LayoutElementData data) => data.IgnoreLayout = value ? 1u : 0u);
    }

    private delegate void Editor(ref LayoutElementData data);

    private LayoutElementData Get()
    {
      var data = new LayoutElementData();
      unsafe { InternalCalls.LayoutElement_Get(UUID, &data); }
      return data;
    }

    private void Edit(Editor edit)
    {
      var data = Get();
      edit(ref data);
      unsafe { InternalCalls.LayoutElement_Set(UUID, &data); }
    }

  } // class LayoutElement

} // namespace Sbx.Core.UI
