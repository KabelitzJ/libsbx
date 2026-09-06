using Sbx.Math;

namespace Sbx.Core
{

  /**
   * A clickable rect (needs a RectTransform on the same node to be placed and hit-tested). IsHovered/
   * IsPressed/WasClicked are polled, native-computed state -- read them from OnUpdate, don't expect a
   * callback/event (see the native canvas::ui_button's own doc comment for why: polling avoids any
   * native/managed callback plumbing).
   */
  public class UIButton : Component
  {

    public bool Interactable
    {
      get { unsafe { return InternalCalls.UIButton_GetInteractable(UUID); } }
      set { unsafe { InternalCalls.UIButton_SetInteractable(UUID, value); } }
    }

    public Color NormalColor
    {
      get { unsafe { Color value; InternalCalls.UIButton_GetNormalColor(UUID, &value); return value; } }
      set { unsafe { InternalCalls.UIButton_SetNormalColor(UUID, &value); } }
    }

    public Color HoveredColor
    {
      get { unsafe { Color value; InternalCalls.UIButton_GetHoveredColor(UUID, &value); return value; } }
      set { unsafe { InternalCalls.UIButton_SetHoveredColor(UUID, &value); } }
    }

    public Color PressedColor
    {
      get { unsafe { Color value; InternalCalls.UIButton_GetPressedColor(UUID, &value); return value; } }
      set { unsafe { InternalCalls.UIButton_SetPressedColor(UUID, &value); } }
    }

    /** True while the cursor is over this button (and it's interactable). */
    public bool IsHovered
    {
      get { unsafe { return InternalCalls.UIButton_GetIsHovered(UUID); } }
    }

    /** True while the mouse button is held down, having been pressed while over this button. */
    public bool IsPressed
    {
      get { unsafe { return InternalCalls.UIButton_GetIsPressed(UUID); } }
    }

    /** True for exactly one OnUpdate -- the frame a press-and-release-both-while-hovered click completed. */
    public bool WasClicked
    {
      get { unsafe { return InternalCalls.UIButton_GetWasClicked(UUID); } }
    }

  } // class UIButton

} // namespace Sbx.Core
