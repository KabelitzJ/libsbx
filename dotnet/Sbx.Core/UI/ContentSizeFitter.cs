using Sbx.Core;

namespace Sbx.Core.UI
{

  /** How a ContentSizeFitter sizes its rect along one axis. */
  public enum ContentFitMode : uint
  {
    // Leaves the rect's size alone.
    Unconstrained,
    MinSize,
    // Sizes the rect to its content's preferred size (a layout group's children, a text).
    PreferredSize,
  }

  /** Resizes its own RectTransform to fit its content, e.g. a panel around a VerticalLayoutGroup. */
  public class ContentSizeFitter : Component
  {

    public ContentFitMode HorizontalFit
    {
      get { unsafe { uint horizontal = 0, vertical = 0; InternalCalls.ContentSizeFitter_Get(UUID, &horizontal, &vertical); return (ContentFitMode)horizontal; } }
      set { unsafe { InternalCalls.ContentSizeFitter_Set(UUID, (uint)value, (uint)VerticalFit); } }
    }

    public ContentFitMode VerticalFit
    {
      get { unsafe { uint horizontal = 0, vertical = 0; InternalCalls.ContentSizeFitter_Get(UUID, &horizontal, &vertical); return (ContentFitMode)vertical; } }
      set { unsafe { InternalCalls.ContentSizeFitter_Set(UUID, (uint)HorizontalFit, (uint)value); } }
    }

  } // class ContentSizeFitter

} // namespace Sbx.Core.UI
