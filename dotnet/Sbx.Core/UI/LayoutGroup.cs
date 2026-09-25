using System.Runtime.InteropServices;

using Sbx.Core;

namespace Sbx.Core.UI
{

  /** Where a layout group puts its children when they don't fill it. */
  public enum LayoutAlignment : uint
  {
    UpperLeft,
    UpperCenter,
    UpperRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    LowerLeft,
    LowerCenter,
    LowerRight,
  }

  // Mirrors the native interop's layout_group_data field for field (flags as 0/1).
  [StructLayout(LayoutKind.Sequential)]
  internal struct LayoutGroupData
  {
    public float Spacing;
    public float PaddingLeft;
    public float PaddingTop;
    public float PaddingRight;
    public float PaddingBottom;
    public uint ChildAlignment;
    public uint ControlChildWidth;
    public uint ControlChildHeight;
    public uint ChildForceExpandWidth;
    public uint ChildForceExpandHeight;
  }

  /**
   * Lays its RectTransform children out in a row (HorizontalLayoutGroup) or column
   * (VerticalLayoutGroup), in child order, sized from their LayoutElements -- Unity's layout group
   * semantics (see the native canvas layout_resolve). Every property reads the native component and
   * writes the whole of it back.
   */
  public abstract class LayoutGroup : Component
  {

    private protected abstract bool IsVertical { get; }

    public float Spacing
    {
      get => Get().Spacing;
      set => Edit((ref LayoutGroupData data) => data.Spacing = value);
    }

    public LayoutAlignment ChildAlignment
    {
      get => (LayoutAlignment)Get().ChildAlignment;
      set => Edit((ref LayoutGroupData data) => data.ChildAlignment = (uint)value);
    }

    public bool ControlChildWidth
    {
      get => Get().ControlChildWidth != 0;
      set => Edit((ref LayoutGroupData data) => data.ControlChildWidth = value ? 1u : 0u);
    }

    public bool ControlChildHeight
    {
      get => Get().ControlChildHeight != 0;
      set => Edit((ref LayoutGroupData data) => data.ControlChildHeight = value ? 1u : 0u);
    }

    public bool ChildForceExpandWidth
    {
      get => Get().ChildForceExpandWidth != 0;
      set => Edit((ref LayoutGroupData data) => data.ChildForceExpandWidth = value ? 1u : 0u);
    }

    public bool ChildForceExpandHeight
    {
      get => Get().ChildForceExpandHeight != 0;
      set => Edit((ref LayoutGroupData data) => data.ChildForceExpandHeight = value ? 1u : 0u);
    }

    public void SetPadding(float left, float top, float right, float bottom)
    {
      Edit((ref LayoutGroupData data) =>
      {
        data.PaddingLeft = left;
        data.PaddingTop = top;
        data.PaddingRight = right;
        data.PaddingBottom = bottom;
      });
    }

    private delegate void Editor(ref LayoutGroupData data);

    private LayoutGroupData Get()
    {
      var data = new LayoutGroupData();
      unsafe { InternalCalls.LayoutGroup_Get(UUID, IsVertical, &data); }
      return data;
    }

    private void Edit(Editor edit)
    {
      var data = Get();
      edit(ref data);
      unsafe { InternalCalls.LayoutGroup_Set(UUID, IsVertical, &data); }
    }

  } // class LayoutGroup

  public class HorizontalLayoutGroup : LayoutGroup
  {
    private protected override bool IsVertical => false;
  } // class HorizontalLayoutGroup

  public class VerticalLayoutGroup : LayoutGroup
  {
    private protected override bool IsVertical => true;
  } // class VerticalLayoutGroup

} // namespace Sbx.Core.UI
