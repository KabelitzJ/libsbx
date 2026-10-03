using Sbx.Core;
using Sbx.Core.Math;

namespace Sbx.Core.UI
{

  public class UIScrollRect : Component
  {

    public Vector2 NormalizedPosition
    {
      get { unsafe { Vector2 value; InternalCalls.UIScrollRect_GetNormalizedPosition(UUID, &value); return value; } }
      set { unsafe { InternalCalls.UIScrollRect_SetNormalizedPosition(UUID, &value); } }
    }

    public bool Horizontal
    {
      get { unsafe { return InternalCalls.UIScrollRect_GetHorizontal(UUID); } }
      set { unsafe { InternalCalls.UIScrollRect_SetHorizontal(UUID, value); } }
    }

    public bool Vertical
    {
      get { unsafe { return InternalCalls.UIScrollRect_GetVertical(UUID); } }
      set { unsafe { InternalCalls.UIScrollRect_SetVertical(UUID, value); } }
    }

    /** The child that scrolls: laid out at its natural size, moved by NormalizedPosition. */
    public void SetContent(Node content)
    {
      unsafe { InternalCalls.UIScrollRect_SetContent(UUID, content.UUID); }
    }

  } // class UIScrollRect

} // namespace Sbx.Core.UI
