using Sbx.Math;

namespace Sbx.Core
{

  /**
   * A UI element's placement within its parent canvas/element. Unity RectTransform-compatible
   * anchor math -- see the native rect_resolve.hpp's resolve_rect for the exact formula, which
   * handles both a point anchor (AnchorMin == AnchorMax, SizeDelta is the element's literal size)
   * and a stretched one (AnchorMin != AnchorMax, SizeDelta becomes a margin on the stretched size)
   * uniformly.
   */
  public class RectTransform : Component
  {

    public Vector2 AnchorMin
    {
      get { unsafe { Vector2 value; InternalCalls.RectTransform_GetAnchorMin(UUID, &value); return value; } }
      set { unsafe { InternalCalls.RectTransform_SetAnchorMin(UUID, &value); } }
    }

    public Vector2 AnchorMax
    {
      get { unsafe { Vector2 value; InternalCalls.RectTransform_GetAnchorMax(UUID, &value); return value; } }
      set { unsafe { InternalCalls.RectTransform_SetAnchorMax(UUID, &value); } }
    }

    public Vector2 AnchoredPosition
    {
      get { unsafe { Vector2 value; InternalCalls.RectTransform_GetAnchoredPosition(UUID, &value); return value; } }
      set { unsafe { InternalCalls.RectTransform_SetAnchoredPosition(UUID, &value); } }
    }

    public Vector2 SizeDelta
    {
      get { unsafe { Vector2 value; InternalCalls.RectTransform_GetSizeDelta(UUID, &value); return value; } }
      set { unsafe { InternalCalls.RectTransform_SetSizeDelta(UUID, &value); } }
    }

    public Vector2 Pivot
    {
      get { unsafe { Vector2 value; InternalCalls.RectTransform_GetPivot(UUID, &value); return value; } }
      set { unsafe { InternalCalls.RectTransform_SetPivot(UUID, &value); } }
    }

  } // class RectTransform

} // namespace Sbx.Core
