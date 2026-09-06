using Sbx.Math;

namespace Sbx.Core
{

  /**
   * A text label (needs a RectTransform on the same node to be placed). v1 gap: authored but not
   * yet rendered -- the native canvas_pass has no glyph/font-atlas pipeline yet. Safe to set now so
   * game code doesn't need to change once that lands.
   */
  public class UIText : Component
  {

    public string? Text
    {
      get { unsafe { return InternalCalls.UIText_GetText(UUID); } }
      set { unsafe { InternalCalls.UIText_SetText(UUID, value); } }
    }

    public float FontSize
    {
      get { unsafe { float value; InternalCalls.UIText_GetFontSize(UUID, &value); return value; } }
      set { unsafe { InternalCalls.UIText_SetFontSize(UUID, value); } }
    }

    public Color Color
    {
      get { unsafe { Color value; InternalCalls.UIText_GetColor(UUID, &value); return value; } }
      set { unsafe { InternalCalls.UIText_SetColor(UUID, &value); } }
    }

  } // class UIText

} // namespace Sbx.Core
